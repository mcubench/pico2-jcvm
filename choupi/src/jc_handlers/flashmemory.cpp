/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#include "flashmemory.hpp"
#include "../exceptions.hpp"
#include "../heap.hpp"
#include "../jc_types/jc_array.hpp"
#include "../jc_types/jc_field.hpp"
#include "../jc_types/jc_instance.hpp"
#include "../jc_utils.hpp"
#include "ffi.h"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#ifndef PC_VERSION
extern "C" void choupi_vm_record_array_write_failure(
    uint8_t stage, uint16_t index, uint16_t size, uint16_t ref,
    uint8_t type, uint8_t exception_id);
#endif
extern "C" void choupi_record_transaction_buffer_full(void);

extern "C" const uint8_t *choupi_cap_store_data(uint32_t *length);
extern "C" const uint8_t *choupi_cap_store_data_for_package(
    uint8_t package_id, uint32_t *length);
extern "C" uint8_t fs_sync(void);
extern "C" void path_applet_field16(uint8_t applet, uint8_t package,
                                    uint16_t class_index, uint8_t field,
                                    uint8_t (*tag)[128], uint8_t *length);
#ifdef PC_VERSION
extern "C" const uint8_t *choupi_pc_rom_package_data(uint8_t package_id,
                                                       uint32_t *length);
#endif

namespace jcvm {

namespace {

constexpr uint8_t kPersistentObjectAlias = 0xFE;
constexpr uint8_t kPersistentArrayAlias = 0xFD;
constexpr size_t kMaxPersistentObjectDepth = TAG_MAX_LENGTH / 2 + 1;
constexpr uint16_t kMaxCommitCapacity = 0x7fff;
// Per-entry metadata is one tag-length byte plus a two-byte payload length.
constexpr uint32_t kCommitEntryMetadataOverhead = 3;

struct PersistentObjectFrame {
  const JC_Instance *instance = nullptr;
};

PersistentObjectFrame persistent_object_stack[kMaxPersistentObjectDepth];
size_t persistent_object_depth = 0;

struct PersistentWriteSnapshot {
  fs::Tag tag;
  bool existed = false;
  std::vector<uint8_t> data;
  uint32_t charged_bytes = 0;
};

std::vector<PersistentWriteSnapshot> persistent_write_snapshots;
bool persistent_write_transaction_active = false;
bool persistent_write_capacity_enforced = false;
uint32_t persistent_write_transaction_used_bytes = 0;
uint32_t persistent_write_rollback_generation = 0;

bool tags_equal(const fs::Tag &left, const fs::Tag &right) noexcept {
  return left.len == right.len &&
         std::memcmp(left.value, right.value, left.len) == 0;
}

bool legacy_applet_tag(const fs::Tag &tag, fs::Tag &legacy) noexcept {
  if (tag.len < 6 || (tag.len & 1u) != 0 || tag.value[0] != 0x03) {
    return false;
  }
  legacy = {};
  legacy.len = static_cast<uint8_t>(tag.len - 1);
  std::memcpy(legacy.value, tag.value, 3);
  std::memcpy(legacy.value + 3, tag.value + 4, tag.len - 4);
  return true;
}

void charge_persistent_snapshot(PersistentWriteSnapshot &snapshot) {
  if (!persistent_write_capacity_enforced) return;
  // Rollback stores the previous record. New records need only an erase
  // marker, so their new payload does not occupy snapshot payload capacity.
  const uint64_t payload_bytes = snapshot.existed ? snapshot.data.size() : 0;
  const uint64_t new_charge = payload_bytes + snapshot.tag.len +
                              kCommitEntryMetadataOverhead;
  const uint32_t old_charge = snapshot.charged_bytes;
  const uint64_t next_used = persistent_write_transaction_used_bytes -
                             old_charge + new_charge;
  if (next_used > kMaxCommitCapacity) {
    choupi_record_transaction_buffer_full();
    throw Exceptions::TransactionException;
  }
  snapshot.charged_bytes = static_cast<uint32_t>(new_charge);
  persistent_write_transaction_used_bytes = static_cast<uint32_t>(next_used);
}

void snapshot_persistent_tag(const fs::Tag &tag) {
  if (!persistent_write_transaction_active) {
    return;
  }
  for (auto &snapshot : persistent_write_snapshots) {
    if (tags_equal(snapshot.tag, tag)) {
      charge_persistent_snapshot(snapshot);
      return;
    }
  }

  PersistentWriteSnapshot snapshot;
  snapshot.tag = tag;
  snapshot.existed = fs_exists(tag.value, tag.len) != 0;
  if (snapshot.existed) {
    uint32_t length = 0;
    if (fs_length(tag.value, tag.len, &length) != 0) {
      throw Exceptions::IOException;
    }
    snapshot.data.resize(length);
    if (length != 0 && fs_read(tag.value, tag.len, snapshot.data.data(), length) != 0) {
      throw Exceptions::IOException;
    }
  }
  charge_persistent_snapshot(snapshot);
  persistent_write_snapshots.push_back(std::move(snapshot));
}

uint8_t write_persistent_tag(const fs::Tag &tag, const uint8_t *data,
                             const uint32_t length) {
  snapshot_persistent_tag(tag);
#if defined(PC_VERSION) && defined(DEBUG)
  std::fprintf(stderr, "PC_FS:WRITE tag=");
  for (uint8_t index = 0; index < tag.len; ++index) {
    std::fprintf(stderr, "%02X", tag.value[index]);
  }
  std::fprintf(stderr, " length=%u\n", length);
#endif
  return fs_write(tag.value, tag.len, data, length);
}

bool rollback_persistent_writes() noexcept {
  bool success = true;
  // Erase transaction-created records first so a nearly full filesystem can
  // reclaim their space before restoring snapshots of replaced records.
  for (auto it = persistent_write_snapshots.rbegin();
       it != persistent_write_snapshots.rend(); ++it) {
    if (!it->existed && fs_exists(it->tag.value, it->tag.len) != 0 &&
        fs_erase(it->tag.value, it->tag.len) != 0) {
      success = false;
    }
  }
  for (auto it = persistent_write_snapshots.rbegin();
       it != persistent_write_snapshots.rend(); ++it) {
    if (!it->existed) {
      continue;
    }
    const uint8_t empty_data = 0;
    const uint8_t *data = it->data.empty() ? &empty_data : it->data.data();
    if (fs_write(it->tag.value, it->tag.len, data,
                 static_cast<uint32_t>(it->data.size())) != 0) {
      success = false;
    }
  }
  persistent_write_transaction_active = false;
  persistent_write_capacity_enforced = false;
  persistent_write_transaction_used_bytes = 0;
  if (fs_sync() != 0) {
    success = false;
  }
  persistent_write_snapshots.clear();
  return success;
}

class PersistentObjectFrameGuard {
public:
  explicit PersistentObjectFrameGuard(const JC_Instance *instance) noexcept {
    if (persistent_object_depth < kMaxPersistentObjectDepth) {
      auto &frame = persistent_object_stack[persistent_object_depth++];
      frame.instance = instance;
      this->active = true;
    }
  }

  ~PersistentObjectFrameGuard() noexcept {
    if (this->active) {
      persistent_object_stack[--persistent_object_depth] =
          PersistentObjectFrame{};
    }
  }

  bool isActive() const noexcept { return this->active; }

private:
  bool active = false;
};

bool is_persistent_object_active(const JC_Instance *instance) {
  for (size_t index = 0; index < persistent_object_depth; ++index) {
    if (persistent_object_stack[index].instance == instance) {
      return true;
    }
  }
  return false;
}

void write_persistent_object_alias(const fs::Tag &tag,
                                   const fs::Tag &target_tag) {
  uint8_t data[2 + TAG_MAX_LENGTH] = {kPersistentObjectAlias, target_tag.len};
  std::memcpy(data + 2, target_tag.value, target_tag.len);
  if (write_persistent_tag(tag, data, 2 + target_tag.len)) {
    throw Exceptions::IOException;
  }
}

void write_persistent_array_alias(const fs::Tag &tag,
                                  const fs::Tag &target_tag) {
  uint8_t data[2 + TAG_MAX_LENGTH] = {kPersistentArrayAlias, target_tag.len};
  std::memcpy(data + 2, target_tag.value, target_tag.len);
  if (write_persistent_tag(tag, data, 2 + target_tag.len)) {
    throw Exceptions::IOException;
  }
}

fs::Tag read_persistent_alias_target(uint32_t length, const uint8_t *data) {
  if (length < 2 || data[1] == 0 ||
      length != static_cast<uint32_t>(2u + data[1]) ||
      data[1] > TAG_MAX_LENGTH) {
    throw Exceptions::SecurityException;
  }
  fs::Tag target_tag;
  target_tag.len = data[1];
  std::memcpy(target_tag.value, data + 2, target_tag.len);
  return target_tag;
}

// Persistent array contents need an identity tag independent of the field
// that currently references the array. Reserve the impossible array element
// index 0xffff as a child tag for that body record. Array element indexes are
// at most 0xfffe (the largest valid index in a 65535-element array).
fs::Tag persistent_array_body_tag(const fs::Tag &field_tag) {
  if (field_tag.len > TAG_MAX_LENGTH - 2) {
    throw Exceptions::IOException;
  }
  fs::Tag body_tag = field_tag;
  body_tag.value[body_tag.len++] = 0xff;
  body_tag.value[body_tag.len++] = 0xff;
  return body_tag;
}

} // namespace

extern "C" uint8_t choupi_persistent_write_transaction_active(void) {
  return persistent_write_transaction_active ? 1u : 0u;
}

bool FlashMemory_Handler::beginPersistentWriteTransaction(
    const bool enforce_commit_capacity) noexcept {
  if (persistent_write_transaction_active) {
    return false;
  }
  persistent_write_snapshots.clear();
  persistent_write_capacity_enforced = enforce_commit_capacity;
  persistent_write_transaction_used_bytes = 0;
  persistent_write_transaction_active = true;
  return true;
}

bool FlashMemory_Handler::commitPersistentWriteTransaction() noexcept {
  if (!persistent_write_transaction_active) {
    return false;
  }
  persistent_write_transaction_active = false;
  if (fs_sync() != 0) {
    persistent_write_transaction_active = true;
    const bool restored_persistent_records =
        !persistent_write_snapshots.empty();
    (void)rollback_persistent_writes();
    if (restored_persistent_records) {
      ++persistent_write_rollback_generation;
    }
    return false;
  }
  persistent_write_snapshots.clear();
  persistent_write_capacity_enforced = false;
  persistent_write_transaction_used_bytes = 0;
  return true;
}

bool FlashMemory_Handler::rollbackPersistentWriteTransaction() noexcept {
  if (!persistent_write_transaction_active) {
    return false;
  }
  const bool restored_persistent_records =
      !persistent_write_snapshots.empty();
  const bool success = rollback_persistent_writes();
  if (restored_persistent_records) {
    ++persistent_write_rollback_generation;
  }
  return success;
}

bool FlashMemory_Handler::persistentWriteTransactionActive() noexcept {
  return persistent_write_transaction_active;
}

uint16_t FlashMemory_Handler::maxCommitCapacity() noexcept {
  return kMaxCommitCapacity;
}

uint16_t FlashMemory_Handler::unusedCommitCapacity() noexcept {
  if (!persistent_write_transaction_active ||
      !persistent_write_capacity_enforced) {
    return kMaxCommitCapacity;
  }
  return static_cast<uint16_t>(kMaxCommitCapacity -
                               persistent_write_transaction_used_bytes);
}

uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return persistent_write_rollback_generation;
}

std::pair<uint32_t, std::unique_ptr<uint8_t[]>>
FlashMemory_Handler::readPersistentRecord(const fs::Tag &tag) {
  return getDataFromTag(tag);
}

bool FlashMemory_Handler::writePersistentRecord(
    const fs::Tag &tag, const uint8_t *data, uint32_t length) noexcept {
  if (tag.len == 0 || tag.len > TAG_MAX_LENGTH || data == nullptr ||
      length == 0) {
    return false;
  }
  try {
    return write_persistent_tag(tag, data, length) == 0;
  } catch (...) {
    return false;
  }
}

bool FlashMemory_Handler::erasePersistentTag(const fs::Tag &tag) noexcept {
  if (tag.len == 0 || tag.len > TAG_MAX_LENGTH) {
    return false;
  }
  try {
    if (fs_exists(tag.value, tag.len) == 0) {
      return true;
    }
    snapshot_persistent_tag(tag);
    return fs_erase(tag.value, tag.len) == 0;
  } catch (...) {
    return false;
  }
}

/// Get packages array length
static constexpr uint16_t getPackagesArrayLength() {
  static_assert((JCVM_MAX_PACKAGES % 8) == 0,
                "JCVM_MAX_PACKAGES value must be a multiple of 8.");
  return JCVM_MAX_PACKAGES / 8;
}

/**
 * Read data from tag
 *
 * @param[tag] Associated tag value.
 * @return data associated to the tag.
 */
std::pair<uint32_t, std::unique_ptr<uint8_t[]>>
FlashMemory_Handler::getDataFromTag(const fs::Tag &tag) {
  fs::Tag read_tag = tag;
  uint32_t data_length = 0;

  if (fs_length(read_tag.value, read_tag.len, &data_length) != 0) {
    if (!legacy_applet_tag(tag, read_tag) ||
        fs_length(read_tag.value, read_tag.len, &data_length) != 0) {
      throw Exceptions::IOException;
    }
  }

  if (data_length == 0) {
    throw Exceptions::IOException;
  }

  std::unique_ptr<uint8_t[]> data(new uint8_t[data_length]);
  if (fs_read(read_tag.value, read_tag.len, data.get(), data_length)) {
    throw Exceptions::IOException;
  }

  return std::make_pair(data_length, std::move(data));
}

/**
 * Write persistant instance header
 *
 * @param[tag] Associated tag value.
 * param[package] instance package ID
 * @param[class_index] instance class index
 */
void FlashMemory_Handler::writeInstanceHeader(
    const fs::Tag &tag, const jpackage_ID_t package,
    const jclass_index_t class_index) {
  static_assert(sizeof(jpackage_ID_t) == sizeof(uint8_t),
                "jpackage_ID_t type size must be same as uint8_t");
  static_assert(sizeof(jclass_index_t) == sizeof(uint16_t),
                "jclass_index_t type size must be same as uint16_t");

  uint8_t data[] = {FieldType::FIELD_TYPE_OBJECT, package,
                    HIGH_BYTE_SHORT(class_index), LOW_BYTE_SHORT(class_index)};

  if (write_persistent_tag(
          tag, data, static_cast<uint32_t>(sizeof(data) / sizeof(data[0])))) {
    throw Exceptions::IOException;
  }
}

/**
 * Write persistant array data
 *
 * @param[tag] Associated tag value.
 * @param[array_type] array type to write
 * @param[data] array data
 */
void FlashMemory_Handler::writeArray(const fs::Tag &tag, const FieldType type,
                                     JC_Array &array, Heap &heap) {

  static_assert(sizeof(decltype(array.getReferenceType())) == sizeof(uint16_t),
                "japplet_ID_t should be encoded on 2-byte.");

  uint8_t *data = nullptr;
  uint16_t header = sizeof(FieldType) + sizeof(array.size());
  uint16_t array_size = 0;

  if (array.isTransientArray()) {
    header += sizeof(ClearEvent);
  }

  if ((type == FieldType::FIELD_TYPE_ARRAY_OBJECT) ||
      (type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT)) {
    header += sizeof(array.getReferenceType());
  }

  if (array.isTransientArray() ||
      (type == FieldType::FIELD_TYPE_ARRAY_OBJECT)) {
    array_size = header;
  } else {
    array_size = header + array.size() * array.getEntrySize();
  }

  data = new uint8_t[array_size];
  std::unique_ptr<uint8_t[]> data_owner(data);

  uint8_t pos = 0;

  data[pos++] = type;
  data[pos++] = HIGH_BYTE_SHORT(array.size());
  data[pos++] = LOW_BYTE_SHORT(array.size());

  if (array.isTransientArray()) {
    data[pos++] = array.getClearEvent();
  }

  if ((type == FieldType::FIELD_TYPE_ARRAY_OBJECT) ||
      (type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT)) {
    data[pos++] = HIGH_BYTE_SHORT(array.getReferenceType());
    data[pos++] = LOW_BYTE_SHORT(array.getReferenceType());
  }

  if (type == FieldType::FIELD_TYPE_ARRAY_OBJECT) {
    for (decltype(array.size()) idx = 0; idx < array.size(); idx++) {
      if ((tag.len + sizeof(idx)) >=
          (sizeof(tag.value) / sizeof(tag.value[0]))) {
        throw Exceptions::IOException;
      }

      fs::Tag new_tag = FlashMemory_Handler::computeTag(tag, idx);

      jref_t objectref;
      try {
        objectref = array.getReferenceEntry(idx);
      } catch (Exceptions exception) {
#ifndef PC_VERSION
        choupi_vm_record_array_write_failure(
            1, idx, array.size(), 0, static_cast<uint8_t>(type),
            static_cast<uint8_t>(exception));
#endif
        throw;
      }
      try {
        FlashMemory_Handler::setPersistentField_Reference(new_tag, objectref,
                                                          heap);
      } catch (Exceptions exception) {
#ifndef PC_VERSION
        choupi_vm_record_array_write_failure(
            2, idx, array.size(), objectref.compact(),
            static_cast<uint8_t>(type), static_cast<uint8_t>(exception));
#endif
        throw;
      }
    }
  } else {
    if (array.isTransientArray()) {
      // Do not copy data
    } else {
      for (uint16_t index = 0; index < (array_size - header); index++) {
        data[index + header] = array.getData()[index];
      }
    }
  }

  // How to write data?
  if (type == FieldType::FIELD_TYPE_ARRAY_OBJECT) {
    /*
     * If transient:
     * 0            8        24            32                40
     * +------------+---------+-------------+-----------------+
     * | Field Type | nbEntry | Clean Event | Reference Type  |
     * +------------+---------+-------------+-----------------+
     *
     *  else:
     * 0            8        24                40
     * +------------+---------+-----------------+
     * | Field Type | nbEntry | Reference Type  |
     * +------------+---------+-----------------+
     */
  } else {
    /*
     * If transient:
     * 0            8        24
     * +------------+---------+-------------+--------+--------+-----
     * | Field Type | nbEntry | Clean Event | Word 1 | Word 2 | ...
     * +------------+---------+-------------+--------+--------+-----
     *
     *  else:
     * 0            8        24
     * +------------+---------+--------+--------+------
     * | Field Type | nbEntry | Word 1 | Word 2 | ...
     * +------------+---------+--------+--------+------
     */
  }

  if (write_persistent_tag(tag, data, array_size)) {
    throw Exceptions::IOException;
  }

  if (!array.isTransientArray()) {
    array.setPersistentTag(tag);
  }

}
///
///        Compute tag to access to persistant data
fs::Tag FlashMemory_Handler::computeTag(const fs::Tag &tag,
                                        const uint16_t index)
#ifndef JCVM_SECURE_HEAP_ACCESS
    noexcept
#endif /* JCVM_SECURE_HEAP_ACCESS */
{
  fs::Tag new_tag;

  new_tag.len = tag.len + sizeof(index);

#ifdef JCVM_SECURE_HEAP_ACCESS

  if (new_tag.len > (sizeof(new_tag.value) / sizeof(new_tag.value[0]))) {
    std::printf("CHOUPI:PERSIST_TAG_LIMIT base_len=%u new_len=%u capacity=%u\r\n",
                tag.len, new_tag.len,
                static_cast<unsigned>(sizeof(new_tag.value)));
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  for (uint8_t idx = 0; idx < tag.len; idx++) {
    new_tag.value[idx] = tag.value[idx];
  }

  new_tag.value[tag.len] = LOW_BYTE_SHORT(index);
  new_tag.value[tag.len + 1] = LOW_BYTE_SHORT(index);

  return new_tag;
}

/**
 * Read data in place from tag
 *
 * @param[tag] Associated tag value.
 * @return data associated to the tag.
 */
std::pair<uint32_t, const uint8_t *>
FlashMemory_Handler::getDataInPlaceFromTag(const fs::Tag &tag) {
#ifdef PC_VERSION
  if (tag.len == 2 && tag.value[0] == 0x01) {
    uint32_t rom_length = 0;
    const uint8_t *rom_data =
        choupi_pc_rom_package_data(tag.value[1], &rom_length);
    if (rom_data != nullptr && rom_length != 0) {
      return std::make_pair(rom_length, rom_data);
    }
  }
#endif

  const uint8_t *data = nullptr;
  uint32_t data_length = 0;

  uint8_t read_status =
      fs_read_inplace(&(tag.value[0]), tag.len, &data, &data_length);
  fs::Tag legacy_tag;
  if ((read_status || data_length == 0 || data == nullptr) &&
      legacy_applet_tag(tag, legacy_tag)) {
    read_status = fs_read_inplace(legacy_tag.value, legacy_tag.len, &data,
                                  &data_length);
  }
  if (read_status || data_length == 0 || data == nullptr) {
    if (tag.len == 5 && tag.value[0] == 0x03 && tag.value[4] == 0xff) {
      std::printf("CHOUPI:RESTORE_STORAGE status=%u length=%lu\r\n",
                  static_cast<unsigned>(read_status),
                  static_cast<unsigned long>(data_length));
      std::fflush(stdout);
    }
#ifdef PC_VERSION
    std::fprintf(stderr,
                 "PC_FS:READ_IN_PLACE status=%u length=%u tag_len=%u tag=",
                 read_status, data_length, tag.len);
    for (uint8_t index = 0; index < tag.len; ++index) {
      std::fprintf(stderr, "%02X", tag.value[index]);
    }
    std::fprintf(stderr, "\n");
#endif
    throw Exceptions::IOException;
  }

  return std::make_pair(data_length, data);
}

/*
 * Write data from tag
 *
 * @param[tag] Associated tag value
 * @param[length] data length to write
 * @param[data] data to write
 */
void FlashMemory_Handler::setDataFromTag(const fs::Tag &tag, uint32_t length,
                                         const uint8_t data[]) {
  if (write_persistent_tag(tag, data, length)) {
    throw Exceptions::IOException;
  }
}

/**
 * Make tag for package list.
 *
 * @return package list's tag.
 */
const fs::Tag FlashMemory_Handler::getPackagesListTag() noexcept {
  fs::Tag tag;
  path_package_list(&(tag.value), &(tag.len));
  return tag;
}

/**
 * Make tag for a CAP file.
 *
 * @return CAP file's tag.
 */
const fs::Tag
FlashMemory_Handler::getCapTag(const jpackage_ID_t package) noexcept {
  fs::Tag tag;
  path_cap(package, &(tag.value), &(tag.len));
  return tag;
}

/**
 * Make tag for a static field.
 *
 * @param[package] the associated package ID
 * @param[static_id] the static field index to read
 *
 * @return static field's tag.
 */
const fs::Tag
FlashMemory_Handler::getStaticFieldTag(const jpackage_ID_t package,
                                       const uint16_t static_id) noexcept {
  fs::Tag tag;
  if (static_id <= UINT8_MAX) {
    path_static(package, static_cast<uint8_t>(static_id), &(tag.value),
                &(tag.len));
  } else {
    path_static16(package, static_id, &(tag.value), &(tag.len));
  }
  return tag;
}

/**
 * Make tag for an applet field.
 *
 * @param[applet_owner] applet owner ID.
 * @param[package] the associated package ID
 * @param[claz] the associated class ID
 * @param[field] the associated field number
 *
 * @return applet field's tag.
 */
const fs::Tag FlashMemory_Handler::getPersistentFieldTag(
    const japplet_ID_t applet_owner, const jpackage_ID_t package,
    const jclass_index_t claz, const uint8_t field) noexcept {
  fs::Tag tag;
  if (claz <= UINT8_MAX) {
    path_applet_field(applet_owner, package, static_cast<uint8_t>(claz), field,
                      &(tag.value), &(tag.len));
  } else {
    path_applet_field16(applet_owner, package, claz, field, &(tag.value),
                        &(tag.len));
  }
  return tag;
}

const fs::Tag FlashMemory_Handler::getLegacyPersistentFieldTag(
    const japplet_ID_t applet_owner, const jpackage_ID_t package,
    const jclass_index_t claz, const uint8_t field) noexcept {
  fs::Tag tag;
  path_applet_field(applet_owner, package, static_cast<uint8_t>(claz), field,
                    &(tag.value), &(tag.len));
  return tag;
}

const fs::Tag FlashMemory_Handler::getAppletInstanceTag(
    const japplet_ID_t applet_owner, const jpackage_ID_t package,
    const jclass_index_t claz) noexcept {
  // Instance-field counts are encoded as a byte, so field index 0xFF is
  // reserved for the applet object's root record.
  return FlashMemory_Handler::getPersistentFieldTag(
      applet_owner, package, claz, 0xff);
}

/**
 * Get a static field from a tag. This field should be a serialized instance or
 * array
 *
 * @param[tag] the data tag value to read.
 *
 * @return the read static field value.
 */
jref_t FlashMemory_Handler::getPersistentField_Reference(const fs::Tag &tag,
                                                         Heap &heap) {
  jref_t existing_reference;
  if (heap.getPersistentReference(tag, existing_reference)) {
    return existing_reference;
  }

  uint32_t length = 0;
  const uint8_t *data = nullptr;

  try {
    auto [length_tmp, data_tmp] =
        FlashMemory_Handler::getDataInPlaceFromTag(tag);
    length = length_tmp;
    data = data_tmp;
  } catch (Exceptions e) {
    std::printf("CHOUPI:RESTORE_READ_FAIL id=%u tag=",
                static_cast<unsigned>(e));
    for (uint8_t index = 0; index < tag.len; ++index) {
      std::printf("%02X", tag.value[index]);
    }
    std::printf("\r\n");
    std::fflush(stdout);
    jref_t out;
    out.setAsArray(false);
    out.setOffset(0);
    return out;
  }

  if (tag.len == 5 && tag.value[0] == 0x03 && tag.value[4] == 0xff) {
    std::printf("CHOUPI:RESTORE_READ_OK len=%lu type=%02X tag=",
                static_cast<unsigned long>(length), data[0]);
    for (uint8_t index = 0; index < tag.len; ++index) {
      std::printf("%02X", tag.value[index]);
    }
    std::printf("\r\n");
    std::fflush(stdout);
  }

  FieldType type = static_cast<FieldType>(data[0]);

  if (data[0] == kPersistentObjectAlias) {
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    auto [target_length, target_data] =
        FlashMemory_Handler::getDataInPlaceFromTag(target_tag);
    if (target_length < 4 || target_data[0] != FieldType::FIELD_TYPE_OBJECT) {
      throw Exceptions::SecurityException;
    }
    const jpackage_ID_t package = target_data[1];
    const jclass_index_t claz =
        static_cast<jclass_index_t>(BYTES_TO_SHORT(target_data[2], target_data[3]));
    if (heap.getPersistentReference(target_tag, existing_reference)) {
      return existing_reference;
    }
    return heap.addInstance(JC_Instance(heap, package, claz, target_tag));
  }

  if (data[0] == kPersistentArrayAlias) {
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    return FlashMemory_Handler::getPersistentField_Reference(target_tag, heap);
  }

  if (type == FieldType::FIELD_TYPE_UNINITIALIZED) {
#ifdef JCVM_ARRAY_SIZE_CHECK
    if (length != sizeof(uint8_t)) {
      throw Exceptions::SecurityException;
    }
#endif
    jref_t out;
    out.setAsArray(false);
    out.setOffset(0);
    return out;
  }

#ifdef JCVM_ARRAY_SIZE_CHECK
  if (length < 3) {
    throw Exceptions::SecurityException;
  }
#endif

  uint16_t size = BYTES_TO_SHORT(data[1], data[2]);

  switch (type) {
  case FieldType::FIELD_TYPE_ARRAY_BYTE: {
    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_BYTE, 0, tag, false,
                                  ClearEvent::None, size));
  }

  case FieldType::FIELD_TYPE_ARRAY_BOOLEAN: {
    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_BOOLEAN, 0, tag, false,
                                  ClearEvent::None, size));
  }

  case FieldType::FIELD_TYPE_ARRAY_SHORT: {
    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_SHORT, 0, tag, false,
                                  ClearEvent::None, size));
  }

#ifdef JCVM_INT_SUPPORTED

  case FieldType::FIELD_TYPE_ARRAY_INT: {
    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_INT, 0, tag, false,
                                  ClearEvent::None, size));
  }

#endif /* JCVM_INT_SUPPORTED */

  case FieldType::FIELD_TYPE_ARRAY_OBJECT: {
    jc_cp_offset_t cp_offset =
        static_cast<jc_cp_offset_t>(BYTES_TO_SHORT(data[3], data[4]));
    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_REFERENCE, cp_offset, tag,
                                  false, ClearEvent::None, size));
  }

  case FieldType::FIELD_TYPE_OBJECT: {
    static_assert(sizeof(jclass_index_t) == sizeof(uint16_t),
                  "jclass_index_t type should be equals to uint16_t");

    jpackage_ID_t package = static_cast<jpackage_ID_t>(data[1]);
    jclass_index_t claz =
        static_cast<jclass_index_t>(BYTES_TO_SHORT(data[2], data[3]));
    return heap.addInstance(JC_Instance(heap, package, claz, tag));
  }

  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE: {
    ClearEvent event = static_cast<ClearEvent>(data[3]);
    return heap.addArray(
        JC_Array(heap, JAVA_ARRAY_T_BYTE, 0, tag, true, event, size));
  }

  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN: {
    ClearEvent event = static_cast<ClearEvent>(data[3]);
    return heap.addArray(
        JC_Array(heap, JAVA_ARRAY_T_BOOLEAN, 0, tag, true, event, size));
  }

  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT: {
    ClearEvent event = static_cast<ClearEvent>(data[3]);
    return heap.addArray(
        JC_Array(heap, JAVA_ARRAY_T_SHORT, 0, tag, true, event, size));
  }

#ifdef JCVM_INT_SUPPORTED

  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT: {
    ClearEvent event = static_cast<ClearEvent>(data[3]);
    return heap.addArray(
        JC_Array(heap, JAVA_ARRAY_T_INT, 0, tag, true, event, size));
  }

#endif /* JCVM_INT_SUPPORTED */

  case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
    ClearEvent event = static_cast<ClearEvent>(data[3]);
    jc_cp_offset_t cp_offset =
        static_cast<jc_cp_offset_t>(BYTES_TO_SHORT(data[4], data[5]));

    return heap.addArray(JC_Array(heap, JAVA_ARRAY_T_REFERENCE, cp_offset, tag,
                                  true, event, size));
  }

  case FieldType::FIELD_TYPE_UNINITIALIZED: {
    jref_t out;
    out.setAsArray(false);
    out.setOffset(0);
    return out;
  }

  default:
    throw Exceptions::IOException;
  }
}

/** Return the stored representation of one persistent instance field. */
FieldType FlashMemory_Handler::getPersistentField_Type(const fs::Tag &tag) {
  auto [length, data] = FlashMemory_Handler::getDataInPlaceFromTag(tag);
  if (length == 0) {
    throw Exceptions::SecurityException;
  }
  if (data[0] == kPersistentObjectAlias) {
    return FieldType::FIELD_TYPE_OBJECT;
  }
  if (data[0] == kPersistentArrayAlias) {
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    auto [target_length, target_data] =
        FlashMemory_Handler::getDataInPlaceFromTag(target_tag);
    if (target_length == 0 || target_data == nullptr) {
      throw Exceptions::SecurityException;
    }
    const FieldType target_type = static_cast<FieldType>(target_data[0]);
    switch (target_type) {
    case FieldType::FIELD_TYPE_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_ARRAY_INT:
#endif
    case FieldType::FIELD_TYPE_ARRAY_OBJECT:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT:
#endif
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT:
      return target_type;
    default:
      throw Exceptions::SecurityException;
    }
  }
  return static_cast<FieldType>(data[0]);
}

/**
 * Persist a reference-valued static field.
 *
 * Object and array bodies are serialized using the same routines used for
 * nested persistent fields. A one-byte uninitialized marker represents null;
 * getPersistentField_Reference already decodes that marker as a null ref.
 */
void FlashMemory_Handler::setPersistentField_Reference(const fs::Tag &tag,
                                                       const jref_t value,
                                                       Heap &heap) {
  static uint16_t deep_trace_count = 0;
  if (tag.len >= 19 && deep_trace_count < 64) {
    if (value.isArray()) {
      auto array = heap.getArray(value);
      std::printf("CHOUPI:PERSIST_REF tag_len=%u ref=%04X array_type=%u size=%u\r\n",
                  tag.len, value.compact(), static_cast<unsigned>(array->getType()),
                  static_cast<unsigned>(array->size()));
    } else {
      auto instance = heap.getInstance(value);
      std::printf("CHOUPI:PERSIST_REF tag_len=%u ref=%04X package=%u class=%u\r\n",
                  tag.len, value.compact(),
                  static_cast<unsigned>(instance->getPackageID()),
                  static_cast<unsigned>(instance->getClassIndex()));
    }
    ++deep_trace_count;
  }

  try {
  if (value.isNullPointer()) {
    // Missing child records already decode as null in
    // getPersistentField_Reference(). Avoid allocating one filesystem entry
    // for every empty slot in a persistent reference array. If a record
    // exists in ROM or the overlay, retain an explicit null marker so an
    // existing value is still cleared.
    if (!fs_exists(tag.value, tag.len)) {
      return;
    }
    const uint8_t uninitialized =
        static_cast<uint8_t>(FieldType::FIELD_TYPE_UNINITIALIZED);
    FlashMemory_Handler::setDataFromTag(tag, sizeof(uninitialized),
                                        &uninitialized);
    return;
  }

  if (value.isArray()) {
    auto array = heap.getArray(value);
    if (array->isPersistent() && !array->isTransientArray()) {
      const fs::Tag array_tag = array->getPersistentTag();
      if (tag.len != array_tag.len ||
          std::memcmp(tag.value, array_tag.value, tag.len) != 0) {
        write_persistent_array_alias(tag, array_tag);
        return;
      }
      // Legacy arrays used their first referencing field as their storage
      // tag. Promote them to a stable body tag before the field can be
      // cleared or retargeted while aliases still refer to the same object.
      const fs::Tag body_tag = persistent_array_body_tag(tag);
      FlashMemory_Handler::setPersistentField_Array(body_tag, *array, heap);
      write_persistent_array_alias(tag, body_tag);
      return;
    }
    const fs::Tag body_tag = persistent_array_body_tag(tag);
    FlashMemory_Handler::setPersistentField_Array(body_tag, *array, heap);
    write_persistent_array_alias(tag, body_tag);
    return;
  }

  auto instance = heap.getInstance(value);
  if (instance->hasPersistentTag()) {
    write_persistent_object_alias(tag, instance->getPersistentTag());
    return;
  }
  FlashMemory_Handler::setPersistentField_Instance(tag, *instance, heap);
  } catch (Exceptions exception) {
    std::printf("CHOUPI:PERSIST_EXCEPTION id=%u tag_len=%u ref=%04X\r\n",
                static_cast<unsigned>(exception), tag.len, value.compact());
    std::fflush(stdout);
    throw;
  }
}

/*
 * Write static array field from tag
 *
 */
void FlashMemory_Handler::setPersistentField_Array(const fs::Tag &tag,
                                                   JC_Array &array,
                                                   Heap &heap) {
  FieldType type;

  switch (array.getType()) {
  case JAVA_ARRAY_T_BOOLEAN:
    if (array.isTransientArray()) {
      type = FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN;
    } else {
      type = FieldType::FIELD_TYPE_ARRAY_BOOLEAN;
    }

    break;

  case JAVA_ARRAY_T_BYTE:
    if (array.isTransientArray()) {
      type = FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE;
    } else {
      type = FieldType::FIELD_TYPE_ARRAY_BYTE;
    }

    break;

  case JAVA_ARRAY_T_SHORT:
    if (array.isTransientArray()) {
      type = FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT;
    } else {
      type = FieldType::FIELD_TYPE_ARRAY_SHORT;
    }

    break;

#ifdef JCVM_INT_SUPPORTED

  case JAVA_ARRAY_T_INT:
    if (array.isTransientArray()) {
      type = FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT;
    } else {
      type = FieldType::FIELD_TYPE_ARRAY_INT;
    }

    break;
#endif /* JCVM_INT_SUPPORTED */

  case JAVA_ARRAY_T_REFERENCE:
    if (array.isTransientArray()) {
      type = FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT;
    } else {
      type = FieldType::FIELD_TYPE_ARRAY_OBJECT;
    }

    break;

  default:
    std::printf("CHOUPI:PERSIST_INVALID array-type=%u tag_len=%u\r\n",
                static_cast<unsigned>(array.getType()), tag.len);
    throw Exceptions::SecurityException;
  }

  FlashMemory_Handler::writeArray(tag, type, array, heap);
}

/*
 * Write static instance field from tag
 *
 * @param[tag] associated tag
 * @param[instance] instance to write
 * @param[heap] associated heap
 */
void FlashMemory_Handler::setPersistentField_Instance(
    const fs::Tag &tag, const JC_Instance &instance, Heap &heap) {
  if (is_persistent_object_active(&instance)) {
    if (!instance.hasPersistentTag()) {
      throw Exceptions::IOException;
    }
    write_persistent_object_alias(tag, instance.getPersistentTag());
    return;
  }
  if (!instance.hasPersistentTag()) {
    const_cast<JC_Instance &>(instance).setPersistentTag(tag);
  }
  PersistentObjectFrameGuard frame(&instance);
  if (!frame.isActive()) {
    throw Exceptions::IOException;
  }

  auto fields = instance.getFields();

  if (fields == nullptr) {
    std::printf("CHOUPI:PERSIST_INVALID fields=null tag_len=%u\r\n", tag.len);
    // TODO: It's a normal behavior?
    throw Exceptions::SecurityException;
  }

  FlashMemory_Handler::writeInstanceHeader(tag, instance.getPackageID(),
                                           instance.getClassIndex());

  for (uint16_t idx = 0; idx < fields->size(); idx++) {

    fs::Tag field_tag = FlashMemory_Handler::computeTag(tag, idx);

    jc_field_t field = fields->at(idx);

    switch (field.type) {
    case FieldType::FIELD_TYPE_BYTE:
    case FieldType::FIELD_TYPE_BOOLEAN: {
      const uint8_t data[] = {field.type, static_cast<uint8_t>(field.value)};

      if (write_persistent_tag(field_tag, data,
                               sizeof(jbyte_t) + sizeof(uint8_t))) {
        throw Exceptions::IOException;
      }

      break;
    }

    case FieldType::FIELD_TYPE_SHORT: {
      const uint8_t data[] = {
          field.type, static_cast<uint8_t>(HIGH_BYTE_SHORT(field.value)),
          static_cast<uint8_t>(LOW_BYTE_SHORT(field.value))};

      if (write_persistent_tag(field_tag, data,
                               sizeof(jshort_t) + sizeof(uint8_t))) {
        throw Exceptions::IOException;
      }

      break;
    }

#ifdef JCVM_INT_SUPPORTED

    case FieldType::FIELD_TYPE_INT: {
      jc_field_t field_low = fields->at(++idx);
      jint_t value = SHORTS_TO_INT(static_cast<jshort_t>(field.value),
                                   static_cast<jshort_t>(field_low.value));
      const uint8_t data[] = {
          field.type,
          static_cast<uint8_t>(HIGH_BYTE_SHORT(INT_2_MSSHORTS(value))),
          static_cast<uint8_t>(LOW_BYTE_SHORT(INT_2_MSSHORTS(value))),
          static_cast<uint8_t>(HIGH_BYTE_SHORT(INT_2_LSSHORTS(value))),
          static_cast<uint8_t>(LOW_BYTE_SHORT(INT_2_LSSHORTS(value))),
      };

      if (write_persistent_tag(field_tag, data,
                               sizeof(jint_t) + sizeof(uint8_t))) {
        throw Exceptions::IOException;
      }

      break;
    }

#endif /* JCVM_INT_SUPPORTED */

    case FieldType::FIELD_TYPE_OBJECT: {
      jref_t objectref = static_cast<jref_t>(field.value);

      if (objectref.isArray()) {
        std::printf("CHOUPI:PERSIST_INVALID object-field-is-array tag_len=%u ref=%04X\r\n",
                    field_tag.len, objectref.compact());
        throw Exceptions::SecurityException;
      }

      auto field_instance = heap.getInstance(objectref);
      FlashMemory_Handler::setPersistentField_Instance(field_tag,
                                                       *field_instance, heap);
      break;
    }

    case FieldType::FIELD_TYPE_UNINITIALIZED: {
      FlashMemory_Handler::setPersistentField_Reference(field_tag, jref_t(),
                                                        heap);
      break;
    }

    case FieldType::FIELD_TYPE_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_ARRAY_INT:
#endif /* JCVM_INT_SUPPORTED */
    case FieldType::FIELD_TYPE_ARRAY_OBJECT:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN:
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT:
#endif /* JCVM_INT_SUPPORTED */
    case FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
      jref_t arrayref = static_cast<jref_t>(field.value);

      if (!arrayref.isArray()) { // Is an array?
        std::printf("CHOUPI:PERSIST_INVALID array-field-is-object tag_len=%u ref=%04X type=%u\r\n",
                    field_tag.len, arrayref.compact(),
                    static_cast<unsigned>(field.type));
        throw Exceptions::SecurityException;
      }

      FlashMemory_Handler::setPersistentField_Reference(field_tag, arrayref,
                                                        heap);

      break;
    }

    default:
      std::printf("CHOUPI:PERSIST_INVALID field-type=%u tag_len=%u\r\n",
                  static_cast<unsigned>(field.type), field_tag.len);
      throw Exceptions::SecurityException;
    }
  }
}

/**
 * Get byte data store in flash memory.
 *
 * @param[tag] associated tag
 *
 * @return the requested field
 */
const jbyte_t FlashMemory_Handler::getPersistentField_Byte(const fs::Tag &tag) {
  auto [length, data] = FlashMemory_Handler::getDataFromTag(tag);
  const FieldType type = static_cast<FieldType>(data[0]);
  // Older persisted applet images can contain a zero/one boolean written in
  // the short representation. Preserve its value, but reject arbitrary shorts
  // so a field-layout or type mismatch does not silently pass as a byte read.
  const bool legacy_boolean =
      type == FieldType::FIELD_TYPE_SHORT &&
      length == sizeof(jshort_t) + 1 && data[1] == 0 && data[2] <= 1;

  if (length != sizeof(jbyte_t) + 1 && !legacy_boolean) {
    throw Exceptions::SecurityException;
  }

#ifdef JCVM_SECURE_HEAP_ACCESS

  switch (type) {
  case FieldType::FIELD_TYPE_BOOLEAN:
  case FieldType::FIELD_TYPE_BYTE:
    // OK
    break;
  case FieldType::FIELD_TYPE_SHORT:
    if (legacy_boolean) {
      break;
    }
    [[fallthrough]];

  default:
    // NOK
    throw SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  return static_cast<jbyte_t>(legacy_boolean ? data[2] : data[1]);
}

/**
 * Set byte data store in flash memory.
 *
 * @param[tag] associated tag
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Byte(const fs::Tag &tag,
                                                  const jbyte_t value) {
  static_assert(sizeof(jbyte_t) == sizeof(uint8_t),
                "jbyte_t and uint8_t are not compatible!");
  const uint8_t data[] = {FieldType::FIELD_TYPE_BYTE,
                          static_cast<const uint8_t>(value)};
  uint32_t length = sizeof(data) / sizeof((data)[0]);
  FlashMemory_Handler::setDataFromTag(tag, length, data);
}

/**
 * Get short data store in flash memory.
 *
 * @param[tag] associated tag
 *
 *  @return the requested field
 */
const jshort_t
FlashMemory_Handler::getPersistentField_Short(const fs::Tag &tag) {
  auto [length, data] = FlashMemory_Handler::getDataFromTag(tag);

#ifdef JCVM_ARRAY_SIZE_CHECK

  if (length != (sizeof(jshort_t) + 1)) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_ARRAY_SIZE_CHECK */

#ifdef JCVM_SECURE_HEAP_ACCESS
  FieldType type = static_cast<FieldType>(data[0]);

  if (type != FieldType::FIELD_TYPE_SHORT) {
    // NOK
    throw SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  return BYTES_TO_SHORT(data[1], data[2]);
}

/**
 * Set short data store in flash memory.
 *
 * @param[tag] associated tag
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Short(const fs::Tag &tag,
                                                   const jshort_t value) {
  static_assert(sizeof(jshort_t) == 2 * sizeof(uint8_t),
                "jshort_t is not encoded on 2 uint8_t!");

  const uint8_t data[] = {
      FieldType::FIELD_TYPE_SHORT,
      HIGH_BYTE_SHORT(value),
      LOW_BYTE_SHORT(value),
  };

  uint32_t length = sizeof(data) / sizeof((data)[0]);
  FlashMemory_Handler::setDataFromTag(tag, length, data);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Get short data store in flash memory.
 *
 * @param[tag] associated tag
 *
 *  @return the requested field
 */
const jint_t FlashMemory_Handler::getPersistentField_Int(const fs::Tag &tag) {
  auto [length, data] = FlashMemory_Handler::getDataFromTag(tag);

#ifdef JCVM_ARRAY_SIZE_CHECK

  if (length != (sizeof(jint_t) + 1)) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_ARRAY_SIZE_CHECK */

#ifdef JCVM_SECURE_HEAP_ACCESS
  FieldType type = static_cast<FieldType>(data[0]);

  if (type != FieldType::FIELD_TYPE_INT) {
    // NOK
    throw SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  return BYTES_TO_INT(data[1], data[2], data[3], data[4]);
}

/**
 * Set short data store in flash memory.
 *
 * @param[tag] associated tag
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Int(const fs::Tag &tag,
                                                 const jint_t value) {
  static_assert(sizeof(jshort_t) == 2 * sizeof(uint8_t),
                "jint_t is not encoded on 2 uint8_t!");
  const uint8_t data[] = {
      FieldType::FIELD_TYPE_INT,
      HIGH_BYTE_SHORT(INT_2_MSSHORTS(value)),
      LOW_BYTE_SHORT(INT_2_MSSHORTS(value)),
      HIGH_BYTE_SHORT(INT_2_LSSHORTS(value)),
      LOW_BYTE_SHORT(INT_2_LSSHORTS(value)),
  };

  uint32_t length = sizeof(data) / sizeof((data)[0]);
  FlashMemory_Handler::setDataFromTag(tag, length, data);
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Get array data store in flash memory.
 *
 * @param[tag] Associated tag value.
 *
 * @return the requested field as an array.
 */
std::shared_ptr<JC_Array>
FlashMemory_Handler::getPersistentField_Array(const fs::Tag &tag, Heap &heap) {
  auto [length, array_flash] = FlashMemory_Handler::getDataInPlaceFromTag(tag);

  uint16_t pos = 0;

  FieldType type = static_cast<FieldType>(array_flash[pos++]);
  uint16_t size = BYTES_TO_SHORT(array_flash[pos], array_flash[pos + 1]);
  pos += 2;
  ClearEvent event = ClearEvent::None;
  jc_cp_offset_t reference_type = 0;

  jc_array_type array_type;
  bool isTransient = false;

  switch (type) {
  case FIELD_TYPE_ARRAY_BOOLEAN: {
    array_type = JAVA_ARRAY_T_BOOLEAN;
    isTransient = false;
    event = static_cast<ClearEvent>(array_flash[pos]);
    break;
  }

  case FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN: {
    array_type = JAVA_ARRAY_T_BOOLEAN;
    isTransient = true;
    event = static_cast<ClearEvent>(array_flash[pos]);
    break;
  }

  case FIELD_TYPE_ARRAY_BYTE: {
    array_type = JAVA_ARRAY_T_BYTE;
    isTransient = false;
    break;
  }

  case FIELD_TYPE_TRANSIENT_ARRAY_BYTE: {
    array_type = JAVA_ARRAY_T_BYTE;
    isTransient = true;
    event = static_cast<ClearEvent>(array_flash[pos]);
    break;
  }

  case FIELD_TYPE_ARRAY_SHORT: {
    array_type = JAVA_ARRAY_T_SHORT;
    isTransient = false;
    break;
  }

  case FIELD_TYPE_TRANSIENT_ARRAY_SHORT: {
    array_type = JAVA_ARRAY_T_SHORT;
    isTransient = true;
    event = static_cast<ClearEvent>(array_flash[pos]);
    break;
  }

#ifdef JCVM_INT_SUPPORTED

  case FIELD_TYPE_ARRAY_INT: {
    array_type = JAVA_ARRAY_T_INT;
    isTransient = false;
    break;
  }

  case FIELD_TYPE_TRANSIENT_ARRAY_INT: {
    array_type = JAVA_ARRAY_T_INT;
    isTransient = true;
    event = static_cast<ClearEvent>(array_flash[pos]);
    break;
  }

#endif /* JCVM_INT_SUPPORTED */

  case FIELD_TYPE_ARRAY_OBJECT: {
    array_type = JAVA_ARRAY_T_REFERENCE;
    isTransient = false;
    reference_type = BYTES_TO_SHORT(array_flash[pos], array_flash[pos + 1]);
    // pos += 2;
    break;
  }

  case FIELD_TYPE_TRANSIENT_ARRAY_OBJECT: {
    array_type = JAVA_ARRAY_T_REFERENCE;
    isTransient = true;
    event = static_cast<ClearEvent>(array_flash[pos++]);
    reference_type = BYTES_TO_SHORT(array_flash[pos], array_flash[pos + 1]);
    // pos += 2;
    break;
  }

  default:
    throw Exceptions::SecurityException;
  }

  return std::make_shared<JC_Array>(heap, array_type, reference_type, tag,
                                    isTransient, event, size);
}

/*
 * Get array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 *
 * @return indexed data value
 */
const jbyte_t
FlashMemory_Handler::getPersistentField_Array_Byte(const fs::Tag &tag,
                                                   const uint16_t index
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                   ,
                                                   Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
  uint8_t value;

#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->isTransientArray() ||
      array->getType() != JAVA_ARRAY_T_BYTE || array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (fs_read_1b_at(&(tag.value[0]), tag.len, 3u + index, &value)) {
    throw Exceptions::IOException;
  }

  return static_cast<const jbyte_t>(value);
}

/*
 * Set array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Array_Byte(const fs::Tag &tag,
                                                        const uint16_t index,
                                                        const jbyte_t value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                        ,
                                                        Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
#ifdef PC_VERSION
  if (index < 2) {
    std::fprintf(stderr,
                 "PERSIST_BYTE_WRITE index=%u value=%02X taglen=%u tag0=%02X\n",
                 static_cast<unsigned>(index),
                 static_cast<unsigned>(static_cast<uint8_t>(value)),
                 static_cast<unsigned>(tag.len),
                 tag.len > 0 ? static_cast<unsigned>(tag.value[0]) : 0u);
  }
#endif
#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  snapshot_persistent_tag(tag);
  if (fs_write_1b_at(&(tag.value[0]), tag.len, 3u + index, value)) {
    throw Exceptions::IOException;
  }
}

/*
 * Get array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 *
 * @return indexed data value
 */
const jshort_t
FlashMemory_Handler::getPersistentField_Array_Short(const fs::Tag &tag,
                                                    const uint16_t index
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                    ,
                                                    Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
  uint8_t value_high = 0;
  uint8_t value_low = 0;

#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const uint32_t byte_offset = 3u + 2u * index;
  if (fs_read_1b_at(&(tag.value[0]), tag.len, byte_offset, &value_high) ||
      fs_read_1b_at(&(tag.value[0]), tag.len, byte_offset + 1, &value_low)) {
    throw Exceptions::IOException;
  }

  return static_cast<const jshort_t>(BYTES_TO_SHORT(value_high, value_low));
}

/*
 * Set array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Array_Short(const fs::Tag &tag,
                                                         const uint16_t index,
                                                         const jshort_t value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                         ,
                                                         Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  snapshot_persistent_tag(tag);
  const uint16_t encoded = static_cast<uint16_t>(value);
  const uint32_t byte_offset = 3u + 2u * index;
  if (fs_write_1b_at(&(tag.value[0]), tag.len, byte_offset,
                     static_cast<uint8_t>(encoded >> 8)) ||
      fs_write_1b_at(&(tag.value[0]), tag.len, byte_offset + 1,
                     static_cast<uint8_t>(encoded))) {
    throw Exceptions::IOException;
  }
}

#ifdef JCVM_INT_SUPPORTED

/*
 * Get array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 *
 * @return indexed data value
 */
const jint_t
FlashMemory_Handler::getPersistentField_Array_Int(const fs::Tag &tag,
                                                  const uint16_t index
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                  ,
                                                  Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
  uint8_t value_bytes[4]{};

#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  const uint32_t byte_offset = 3u + 4u * index;
  for (uint32_t byte_index = 0; byte_index < sizeof(value_bytes); ++byte_index) {
    if (fs_read_1b_at(&(tag.value[0]), tag.len, byte_offset + byte_index,
                      &value_bytes[byte_index])) {
      throw Exceptions::IOException;
    }
  }

  const uint32_t value = (static_cast<uint32_t>(value_bytes[0]) << 24) |
                         (static_cast<uint32_t>(value_bytes[1]) << 16) |
                         (static_cast<uint32_t>(value_bytes[2]) << 8) |
                         value_bytes[3];
  return static_cast<const jint_t>(value);
}

/*
 * Set array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Array_Int(const fs::Tag &tag,
                                                       const uint16_t index,
                                                       const jint_t value
#ifdef JCVM_SECURE_HEAP_ACCESS
                                                       ,
                                                       Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  snapshot_persistent_tag(tag);
  const uint32_t encoded = static_cast<uint32_t>(value);
  const uint32_t byte_offset = 3u + 4u * index;
  for (uint32_t byte_index = 0; byte_index < sizeof(encoded); ++byte_index) {
    const uint8_t byte = static_cast<uint8_t>(
        encoded >> (24u - 8u * byte_index));
    if (fs_write_1b_at(&(tag.value[0]), tag.len, byte_offset + byte_index,
                       byte)) {
      throw Exceptions::IOException;
    }
  }
}
#endif /* JCVM_INT_SUPPORTED */

/*
 * Get array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 *
 * @return indexed data value
 */
const jref_t FlashMemory_Handler::getPersistentField_Array_Reference(
    const fs::Tag &tag, const uint16_t index, Heap &heap) {

#ifdef JCVM_SECURE_HEAP_ACCESS
  {
    auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

    if (array->isTransientArray()) {
      throw Exceptions::SecurityException;
    }

    if (array->getType() != JAVA_ARRAY_T_REFERENCE) {
      throw Exceptions::SecurityException;
    }

    if (array->size() <= index) {
      throw Exceptions::SecurityException;
    }
  }
#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (static_cast<std::size_t>(tag.len) + sizeof(uint16_t) >
      (sizeof(tag.value) / sizeof(tag.value[0]))) {
    throw Exceptions::IOException;
  }

  fs::Tag field_tag = FlashMemory_Handler::computeTag(tag, index);
  return FlashMemory_Handler::getPersistentField_Reference(field_tag, heap);
}

/*
 * Set array data store value in flash memory at a specific index.
 *
 * @param[tag] Associated tag value.
 * @param[index] data index
 * @param[value] value to write
 */
void FlashMemory_Handler::setPersistentField_Array_Reference(
    const fs::Tag &tag, const uint16_t index, const jref_t value, Heap &heap) {

#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);

  if (array->size() <= index) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_SECURE_HEAP_ACCESS */

  fs::Tag field_tag = FlashMemory_Handler::computeTag(tag, index);
  FlashMemory_Handler::setPersistentField_Reference(field_tag, value, heap);
}

/**
 * Get a pointer to the Packages array
 *
 * @return a pointer to the packages array.
 */
const uint8_t *FlashMemory_Handler::getPackagesArray() {
  auto &tag = FlashMemory_Handler::getPackagesListTag();
  auto packages_array = FlashMemory_Handler::getDataInPlaceFromTag(tag);

  if (packages_array.first != getPackagesArrayLength()) {
    throw Exceptions::SecurityException;
  }

  // This data points directly into persistent flash and is not heap-owned.
  return packages_array.second;
}

/**
 * Enable a Java Card Package in the Java Card Package Array in the flash
 * memory.
 *
 * @param[id] The Package ID to enable.
 */
void FlashMemory_Handler::enablePackage(const jpackage_ID_t id) {
  auto &tag = FlashMemory_Handler::getPackagesListTag();
  uint8_t packages_byte;

  // Reading the value to update
  if (fs_read_1b_at(tag.value, tag.len, (id / 8), &packages_byte)) {
    throw Exceptions::IOException;
  }

  packages_byte |= (1 << (id % 8));

  // Writing the updated value
  snapshot_persistent_tag(tag);
  if (fs_write_1b_at(tag.value, tag.len, (id / 8), packages_byte)) {
    throw Exceptions::IOException;
  }
}

/**
 * Disable a Java Card Package in the Java Card Package Array in the flash
 * memory.
 *
 * @param[id] The Package ID to disable.
 */
void FlashMemory_Handler::disablePackage(const jpackage_ID_t id) {
  auto &tag = FlashMemory_Handler::getPackagesListTag();
  uint8_t packages_byte;

  // Reading the value to update
  if (fs_read_1b_at(tag.value, tag.len, (id / 8), &packages_byte)) {
    throw Exceptions::IOException;
  }

  packages_byte &= ~(1 << (id % 8));

  // Writing the updated value
  snapshot_persistent_tag(tag);
  if (fs_write_1b_at(tag.value, tag.len, (id / 8), packages_byte)) {
    throw Exceptions::IOException;
  }
}

/**
 * Is there package exist?
 *
 * @param[id] The Package ID to disable.
 */
bool FlashMemory_Handler::isPackageExist(const jpackage_ID_t id) {
#ifdef PC_VERSION
  // The host VPCD image contains its built-in ROM-mask CAPs outside the
  // mutable package bitmap. Treat those CAPs as resident packages during
  // import resolution, just as the embedded card does for ROM packages.
  uint32_t rom_length = 0;
  const uint8_t *rom_data = choupi_pc_rom_package_data(id, &rom_length);
  if (rom_data != nullptr && rom_length != 0) {
    return true;
  }
#endif

  auto &tag = FlashMemory_Handler::getPackagesListTag();
  uint8_t packages_byte;

  // Reading the value to update
  if (fs_read_1b_at(tag.value, tag.len, (id / 8), &packages_byte)) {
    throw Exceptions::IOException;
  }

  if ((packages_byte & (1 << (id % 8))) == 0) {
    return false;
  }

  if (FlashMemory_Handler::isDynamicPackageID(id)) {
    // The bitmap can outlive a CAP slot after interrupted cleanup or an
    // upgrade from the older single-package store. Do not treat that stale
    // bit as a resident package: import resolution would otherwise call
    // getCap() on an empty slot and turn a missing dependency into 6F00.
    uint32_t cap_length = 0;
    const uint8_t *cap_code =
        choupi_cap_store_data_for_package(id, &cap_length);
    return cap_code != nullptr && cap_length != 0;
  }

  return true;
}

/**
 * Find a statically installed applet by its complete AID.
 *
 * @param[aid] requested applet AID bytes.
 * @param[aid_length] number of bytes in the requested AID.
 * @param[package_id] receives the package ID containing the applet.
 * @param[install_method_offset] receives the applet install method offset.
 * @return true if a matching applet entry was found.
 */
bool FlashMemory_Handler::findAppletByAID(
    const uint8_t *aid, const uint8_t aid_length, jpackage_ID_t &package_id,
    uint16_t &install_method_offset) {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) {
    return false;
  }

  const uint8_t *packages = FlashMemory_Handler::getPackagesArray();
  for (uint8_t id = 0; id < JCVM_MAX_PACKAGES; ++id) {
    if ((packages[id / 8] & (1u << (id % 8))) == 0) {
      continue;
    }

    const JC_Cap cap = FlashMemory_Handler::getCap(id);
    const jc_cap_applet_component *applets = cap.getApplet();
    if (applets == nullptr) {
      continue;
    }

    const jc_cap_app *entry = applets->findByAID(aid, aid_length);
    if (entry != nullptr) {
      package_id = id;
      install_method_offset = entry->install_method_offset();
      return true;
    }
  }

  return false;
}

/**
 * Get a CAP file from a package ID.
 *
 * @param[packageID] Package ID where the CAP file is located.
 * @return the read CAP file.
 */
JC_Cap FlashMemory_Handler::getCap(const jpackage_ID_t packageID) {
  if (FlashMemory_Handler::isDynamicPackageID(packageID)) {
    uint32_t cap_length = 0;
    const uint8_t *cap_data =
        choupi_cap_store_data_for_package(packageID, &cap_length);
    if (cap_data != nullptr && cap_length != 0 && cap_length <= UINT16_MAX) {
      return JC_Cap(static_cast<uint16_t>(cap_length), cap_data);
    }
  }

  auto &tag = FlashMemory_Handler::getCapTag(packageID);
  auto cap = getDataInPlaceFromTag(tag);

  // ROM-mask CAP payloads are prelinked structures generated from JCA files;
  // their Descriptor tables are not serialized in standard CAP byte order.
  // The outer framing remains checked by JC_Cap, while loaded dynamic CAPs
  // continue to receive full payload validation above.
  return JC_Cap(cap.first, cap.second,
                FlashMemory_Handler::isDynamicPackageID(packageID));
}

} // namespace jcvm
