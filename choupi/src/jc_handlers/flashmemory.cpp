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
#include "../context.hpp"
#include "../exceptions.hpp"
#include "../heap.hpp"
#include "../jc_types/jc_array.hpp"
#include "../jc_types/jc_field.hpp"
#include "../jc_types/jc_instance.hpp"
#include "../jc_utils.hpp"
#include "ffi.h"
#include "../../../src/vm_diagnostics.h"
#include "../../../src/vm_filesystem.h"
#include "../../../src/cap_store.hpp"

#include <cassert>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

extern "C" void choupi_record_transaction_buffer_full(void);

namespace jcvm {

namespace {

constexpr uint8_t kPersistentObjectAlias = 0xFE;
constexpr uint8_t kPersistentArrayAlias = 0xFD;
constexpr uint8_t kPersistentArrayIdentityVersion = 1;
constexpr uint8_t kPersistentArrayIdentityCounterTagBytes[] = {
    0x02, 0xfe, 'J', 'C', 'V', 'M', 'A', 'R', 'R', 'I', 'D'};
constexpr uint8_t kMaxPersistentAliasDepth = 32;
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
bool persistent_write_transaction_applet_owned = false;
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

void preserve_non_atomic_array_byte_in_snapshot(const fs::Tag &tag,
                                                const uint16_t index,
                                                const jbyte_t value) noexcept {
  if (!persistent_write_transaction_active) return;
  const size_t data_offset = 3u + static_cast<size_t>(index);
  for (auto &snapshot : persistent_write_snapshots) {
    if (!tags_equal(snapshot.tag, tag)) continue;
    if (snapshot.existed &&
        snapshot.data.size() > data_offset &&
        snapshot.data[0] == static_cast<uint8_t>(
                                FieldType::FIELD_TYPE_ARRAY_BYTE)) {
      snapshot.data[data_offset] = static_cast<uint8_t>(value);
    }
    return;
  }
}

bool snapshot_matching_persistent_tags(const uint8_t *prefix,
                                       const uint8_t prefix_length,
                                       const int package_filter) noexcept {
  if (!persistent_write_transaction_active || prefix_length > TAG_MAX_LENGTH ||
      (prefix_length != 0 && prefix == nullptr)) {
    return false;
  }
  try {
    fs_tag_next_request_t request{};
    uint16_t previous_bucket = 0;
    uint16_t previous_offset = 0;
    while (true) {
      const uint8_t status = fs_next_tag(prefix, prefix_length, &request);
      if (status == 1) return true;
      if (status != 0 || request.bucket < previous_bucket ||
          (request.bucket == previous_bucket &&
           request.offset <= previous_offset)) {
        return false;
      }
      previous_bucket = request.bucket;
      previous_offset = request.offset;

      const uint8_t tag_length = request.tag[0];
      if (tag_length == 0 || tag_length > TAG_MAX_LENGTH ||
          tag_length < prefix_length ||
          (prefix_length != 0 &&
           std::memcmp(request.tag + 1, prefix, prefix_length) != 0)) {
        return false;
      }
      const uint8_t *tag_bytes = request.tag + 1;
      bool matches = package_filter < 0;
      if (package_filter >= 0) {
        const uint8_t package_id = static_cast<uint8_t>(package_filter);
        matches = (tag_length >= 2 && tag_bytes[0] == 0x02 &&
                   tag_bytes[1] == package_id) ||
                  (tag_length >= 3 && tag_bytes[0] == 0x03 &&
                   tag_bytes[2] == package_id) ||
                  (tag_length >= 3 && tag_bytes[0] == 0x02 &&
                   tag_bytes[1] == 0xff && tag_bytes[2] == package_id);
      }
      if (matches) {
        fs::Tag tag{};
        tag.len = tag_length;
        std::memcpy(tag.value, tag_bytes, tag_length);
        snapshot_persistent_tag(tag);
      }
    }
  } catch (...) {
    return false;
  }
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

void write_persistent_tag_or_throw(const fs::Tag &tag, const uint8_t *data,
                                   const uint32_t length) {
  const uint8_t status = write_persistent_tag(tag, data, length);
  if (status == 0) {
    return;
  }
  std::printf("CHOUPI:PERSIST_WRITE_FAIL status=%u size=%lu tag=",
              static_cast<unsigned>(status),
              static_cast<unsigned long>(length));
  for (uint8_t index = 0; index < tag.len; ++index) {
    std::printf("%02X", tag.value[index]);
  }
  std::printf("\r\n");
  std::fflush(stdout);
  // The filesystem ABI maps OutOfFlash to status 1. Java Card persistent
  // storage exhaustion is a resource limit, not an I/O device failure.
  if (status == 1) {
    Context::getActiveContext().setPendingSystemExceptionReason(
        5); // SystemException.NO_RESOURCE
    throw Exceptions::SystemException;
  }
  throw Exceptions::IOException;
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
  persistent_write_transaction_applet_owned = false;
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
  write_persistent_tag_or_throw(tag, data, 2 + target_tag.len);
}

void write_persistent_array_alias(const fs::Tag &tag,
                                  const fs::Tag &target_tag) {
  uint8_t data[2 + TAG_MAX_LENGTH] = {kPersistentArrayAlias, target_tag.len};
  std::memcpy(data + 2, target_tag.value, target_tag.len);
  write_persistent_tag_or_throw(tag, data, 2 + target_tag.len);
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

bool is_persistent_array_type(const FieldType type) noexcept {
  switch (type) {
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
    return true;
  default:
    return false;
  }
}

uint32_t allocate_persistent_array_identity() {
  static bool initialized = false;
  static uint32_t last_identity = 0;

  fs::Tag counter_tag;
  counter_tag.len = sizeof(kPersistentArrayIdentityCounterTagBytes);
  std::memcpy(counter_tag.value, kPersistentArrayIdentityCounterTagBytes,
              counter_tag.len);

  if (!initialized) {
    if (fs_exists(counter_tag.value, counter_tag.len) != 0) {
      const uint8_t *stored_counter = nullptr;
      uint32_t stored_length = 0;
      if (fs_read_inplace(counter_tag.value, counter_tag.len, &stored_counter,
                          &stored_length) != 0 ||
          stored_counter == nullptr || stored_length != 5 ||
          stored_counter[0] != kPersistentArrayIdentityVersion) {
        throw Exceptions::SecurityException;
      }
      last_identity = (static_cast<uint32_t>(stored_counter[1]) << 24) |
                      (static_cast<uint32_t>(stored_counter[2]) << 16) |
                      (static_cast<uint32_t>(stored_counter[3]) << 8) |
                      static_cast<uint32_t>(stored_counter[4]);
    }
    initialized = true;
  }

  if (last_identity == UINT32_MAX) throw Exceptions::IOException;
  const uint32_t next_identity = last_identity + 1;
  const uint8_t encoded_counter[] = {
      kPersistentArrayIdentityVersion,
      static_cast<uint8_t>(next_identity >> 24),
      static_cast<uint8_t>(next_identity >> 16),
      static_cast<uint8_t>(next_identity >> 8),
      static_cast<uint8_t>(next_identity)};
  write_persistent_tag_or_throw(counter_tag, encoded_counter,
                                sizeof(encoded_counter));
  last_identity = next_identity;
  return next_identity;
}

// Persistent array contents need an identity independent of the field's
// current value. Keep the original field tag as a prefix for owner-scoped
// enumeration and deletion. The reserved 0xffff index marks the body; the
// following 32-bit identity distinguishes successive objects.
fs::Tag persistent_array_body_tag(const fs::Tag &field_tag,
                                  uint32_t identity) {
  if (field_tag.len > TAG_MAX_LENGTH - 6) {
    throw Exceptions::IOException;
  }
  fs::Tag body_tag = field_tag;
  body_tag.value[body_tag.len++] = 0xff;
  body_tag.value[body_tag.len++] = 0xff;
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 24);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 16);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 8);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity);
  return body_tag;
}

// Ordinary object bodies need the same stable-identity treatment as arrays.
// Keep the originating field tag as the owner-scoped prefix; 0xfffe is
// reserved for object bodies while 0xffff identifies array bodies.
fs::Tag persistent_object_body_tag(const fs::Tag &field_tag,
                                   uint32_t identity) {
  if (field_tag.len > TAG_MAX_LENGTH - 6) {
    throw Exceptions::IOException;
  }
  fs::Tag body_tag = field_tag;
  body_tag.value[body_tag.len++] = 0xff;
  body_tag.value[body_tag.len++] = 0xfe;
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 24);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 16);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 8);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity);
  return body_tag;
}

// Legacy objects promoted to an identity body retain a distinct marker so an
// aborted migration can recover the original direct-record tag.
fs::Tag persistent_migrated_object_body_tag(const fs::Tag &field_tag,
                                            uint32_t identity) {
  if (field_tag.len > TAG_MAX_LENGTH - 6) {
    throw Exceptions::IOException;
  }
  fs::Tag body_tag = field_tag;
  body_tag.value[body_tag.len++] = 0xff;
  body_tag.value[body_tag.len++] = 0xfd;
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 24);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 16);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity >> 8);
  body_tag.value[body_tag.len++] = static_cast<uint8_t>(identity);
  return body_tag;
}

bool is_persistent_object_body_tag(const fs::Tag &tag) noexcept {
  return tag.len >= 6 && tag.value[tag.len - 6] == 0xff &&
         (tag.value[tag.len - 5] == 0xfe ||
          tag.value[tag.len - 5] == 0xfd);
}

bool is_persistent_migrated_object_body_tag(const fs::Tag &tag) noexcept {
  return tag.len >= 6 && tag.value[tag.len - 6] == 0xff &&
         tag.value[tag.len - 5] == 0xfd;
}

fs::Tag persistent_object_origin_tag(const fs::Tag &body_tag) {
  fs::Tag origin{};
  origin.len = static_cast<uint8_t>(body_tag.len - 6);
  std::memcpy(origin.value, body_tag.value, origin.len);
  return origin;
}

bool is_valid_persistent_object_body(const fs::Tag &tag) noexcept {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  return fs_read_inplace(tag.value, tag.len, &data, &length) == 0 &&
         data != nullptr && length >= 4 &&
         (data[0] == FieldType::FIELD_TYPE_OBJECT ||
          data[0] == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT);
}

void validate_persistent_object_body(const fs::Tag &tag) {
  if (!is_valid_persistent_object_body(tag)) {
    throw Exceptions::SecurityException;
  }
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
  persistent_write_transaction_applet_owned = false;
  persistent_write_transaction_used_bytes = 0;
  persistent_write_transaction_active = true;
  return true;
}

bool FlashMemory_Handler::beginAppletWriteTransaction() noexcept {
  if (!beginPersistentWriteTransaction(true)) return false;
  persistent_write_transaction_applet_owned = true;
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
  persistent_write_transaction_applet_owned = false;
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

bool FlashMemory_Handler::appletWriteTransactionActive() noexcept {
  return persistent_write_transaction_active &&
         persistent_write_transaction_applet_owned;
}

bool FlashMemory_Handler::abortAppletWriteTransaction() noexcept {
  if (!appletWriteTransactionActive()) return true;
  return rollbackPersistentWriteTransaction();
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

bool FlashMemory_Handler::erasePersistentPrefix(
    const fs::Tag &prefix) noexcept {
  if (prefix.len == 0 || prefix.len > TAG_MAX_LENGTH ||
      !snapshot_matching_persistent_tags(prefix.value, prefix.len, -1)) {
    return false;
  }
  return fs_erase_prefix(prefix.value, prefix.len) == 0;
}

bool FlashMemory_Handler::erasePersistentPackageState(
    const uint8_t package_id) noexcept {
  const uint8_t empty_prefix = 0;
  if (!snapshot_matching_persistent_tags(&empty_prefix, 0, package_id)) {
    return false;
  }
  return fs_erase_package_state(package_id) == 0;
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
#ifdef PC_VERSION
  const auto log_read_failure = [&](const char *stage, uint8_t status) {
    std::fprintf(stderr,
                 "PC_FS:READ_COPY stage=%s status=%u length=%u tag_len=%u tag=",
                 stage, static_cast<unsigned>(status),
                 static_cast<unsigned>(data_length),
                 static_cast<unsigned>(read_tag.len));
    for (uint8_t index = 0; index < read_tag.len; ++index) {
      std::fprintf(stderr, "%02X", read_tag.value[index]);
    }
    std::fprintf(stderr, "\n");
  };
#endif

  uint8_t read_status =
      fs_length(read_tag.value, read_tag.len, &data_length);
  if (read_status != 0) {
    if (!legacy_applet_tag(tag, read_tag) ||
        (read_status = fs_length(read_tag.value, read_tag.len,
                                 &data_length)) != 0) {
#ifdef PC_VERSION
      log_read_failure("length", read_status);
#endif
      throw Exceptions::IOException;
    }
  }

  if (data_length == 0) {
#ifdef PC_VERSION
    log_read_failure("empty", 0);
#endif
    throw Exceptions::IOException;
  }

  std::unique_ptr<uint8_t[]> data(new uint8_t[data_length]);
  read_status = fs_read(read_tag.value, read_tag.len, data.get(), data_length);
  if (read_status != 0) {
#ifdef PC_VERSION
    log_read_failure("data", read_status);
#endif
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
    const jclass_index_t class_index, const bool shareable_proxy) {
  static_assert(sizeof(jpackage_ID_t) == sizeof(uint8_t),
                "jpackage_ID_t type size must be same as uint8_t");
  static_assert(sizeof(jclass_index_t) == sizeof(uint16_t),
                "jclass_index_t type size must be same as uint16_t");

  const FieldType instance_type = shareable_proxy
      ? FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT
      : FieldType::FIELD_TYPE_OBJECT;
  uint8_t data[] = {instance_type, package,
                    HIGH_BYTE_SHORT(class_index), LOW_BYTE_SHORT(class_index)};

  write_persistent_tag_or_throw(
      tag, data, static_cast<uint32_t>(sizeof(data) / sizeof(data[0])));
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
  uint32_t array_size = 0;

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
    array_size = static_cast<uint32_t>(header) +
                 static_cast<uint32_t>(array.size()) * array.getEntrySize();
  }

  if (array_size > 0xffffu) {
    throw Exceptions::IOException;
  }

  data = new uint8_t[static_cast<uint16_t>(array_size)];
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

  write_persistent_tag_or_throw(tag, data, array_size);

  // Retain the identity for transient arrays too. Their serialized body only
  // contains clear-event metadata, while their bytes stay in transient RAM;
  // the stable tag lets later persistent-field writes serialize an alias
  // instead of allocating another body identity.
  array.setPersistentTag(tag);

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
  write_persistent_tag_or_throw(tag, data, length);
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
  return FlashMemory_Handler::getPersistentField_ReferenceImpl(tag, heap,
                                                               false, 0);
}

jref_t FlashMemory_Handler::getPersistentField_ReferenceStrict(
    const fs::Tag &tag, Heap &heap) {
  return FlashMemory_Handler::getPersistentField_ReferenceImpl(tag, heap,
                                                               true, 0);
}

uint8_t FlashMemory_Handler::getPersistentField_ReferenceOwner(
    const fs::Tag &tag, const uint8_t default_owner) {
  if (fs_exists(tag.value, tag.len) == 0) return default_owner;
  auto [length, data] = FlashMemory_Handler::getDataInPlaceFromTag(tag);
  if (length == 0 || data == nullptr) throw Exceptions::SecurityException;
  if (data[0] == FieldType::FIELD_TYPE_GLOBAL_AID_OBJECT) return 0xfe;
  if (data[0] != FieldType::FIELD_TYPE_FOREIGN_REFERENCE) {
    return default_owner;
  }
  if (length < 3 || data[1] == 0xfe || data[1] == 0xff || data[2] == 0 ||
      data[2] > TAG_MAX_LENGTH || length != 3u + data[2]) {
    throw Exceptions::SecurityException;
  }
  return data[1];
}

jref_t FlashMemory_Handler::getPersistentField_ReferenceImpl(
    const fs::Tag &tag, Heap &heap, const bool fail_on_read,
    const uint8_t alias_depth) {
  jref_t existing_reference;
  if (heap.getPersistentReference(tag, existing_reference)) {
    return existing_reference;
  }

  // Static reference slots without a stored record have their specified
  // default value, null. Any present record must be decoded successfully by
  // the strict path; it must never turn a read/format error into a null root.
  if (fail_on_read && fs_exists(tag.value, tag.len) == 0) {
    return jref_t();
  }

  uint32_t length = 0;
  const uint8_t *data = nullptr;

  try {
    auto [length_tmp, data_tmp] =
        FlashMemory_Handler::getDataInPlaceFromTag(tag);
    length = length_tmp;
    data = data_tmp;
  } catch (Exceptions e) {
    if (fail_on_read) {
      throw e;
    }
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

  if (type == FieldType::FIELD_TYPE_FOREIGN_REFERENCE) {
    if (length < 3 || data[1] == 0xfe || data[1] == 0xff || data[2] < 6 ||
        data[2] > TAG_MAX_LENGTH || length != 3u + data[2] ||
        (fail_on_read && alias_depth >= kMaxPersistentAliasDepth)) {
      throw Exceptions::SecurityException;
    }
    const uint8_t *target_data = data + 3;
    if (target_data[0] != 0x03 || target_data[2] != data[1] ||
        target_data[3] != 0xff || target_data[4] != 0xfc) {
      throw Exceptions::SecurityException;
    }
    Heap *owner_heap = Context::findRetainedAppletHeap(
        data[1], static_cast<japplet_ID_t>(target_data[1]));
    if (owner_heap == nullptr) throw Exceptions::SecurityException;
    fs::Tag target_tag{};
    target_tag.len = data[2];
    std::memcpy(target_tag.value, target_data, target_tag.len);
    return FlashMemory_Handler::getPersistentField_ReferenceImpl(
        target_tag, *owner_heap, fail_on_read,
        static_cast<uint8_t>(alias_depth + 1));
  }

  if (type == FieldType::FIELD_TYPE_GLOBAL_AID_OBJECT) {
    if (length < 10 || data[4] < 5 || data[4] > 16 ||
        length != static_cast<uint32_t>(5u + data[4])) {
      throw Exceptions::SecurityException;
    }
    const jpackage_ID_t package_id = data[1];
    const jclass_index_t class_index =
        static_cast<jclass_index_t>(BYTES_TO_SHORT(data[2], data[3]));
    return Heap::addGlobalAIDObject(data + 5, data[4], package_id,
                                    class_index);
  }

  if (data[0] == kPersistentObjectAlias) {
    if (fail_on_read && alias_depth >= kMaxPersistentAliasDepth) {
      throw Exceptions::SecurityException;
    }
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    auto [target_length, target_data] =
        FlashMemory_Handler::getDataInPlaceFromTag(target_tag);
    if (target_length < 4 ||
        (target_data[0] != FieldType::FIELD_TYPE_OBJECT &&
         target_data[0] != FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT)) {
      throw Exceptions::SecurityException;
    }
    const jpackage_ID_t package = target_data[1];
    const jclass_index_t claz =
        static_cast<jclass_index_t>(BYTES_TO_SHORT(target_data[2], target_data[3]));
    const bool missing_package_proxy =
        target_data[0] == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT &&
        !FlashMemory_Handler::isPackageExist(package);
    if (heap.getPersistentReference(target_tag, existing_reference)) {
      if (target_data[0] == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT) {
        heap.getInstance(existing_reference)->setShareableProxyPlaceholder();
      }
      return existing_reference;
    }
    const jref_t reference =
        heap.addInstance(JC_Instance(heap, package, claz, target_tag,
                                     missing_package_proxy));
    if (target_data[0] == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT) {
      heap.getInstance(reference)->setShareableProxyPlaceholder();
    }
    return reference;
  }

  if (data[0] == kPersistentArrayAlias) {
    if (fail_on_read && alias_depth >= kMaxPersistentAliasDepth) {
      throw Exceptions::SecurityException;
    }
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    const jref_t target_reference =
        FlashMemory_Handler::getPersistentField_ReferenceImpl(
        target_tag, heap, fail_on_read,
        static_cast<uint8_t>(alias_depth + 1));
    if (fail_on_read &&
        (target_reference.isNullPointer() || !target_reference.isArray())) {
      throw Exceptions::SecurityException;
    }
    return target_reference;
  }

  if (type == FieldType::FIELD_TYPE_UNINITIALIZED) {
#ifdef JCVM_ARRAY_SIZE_CHECK
    if (length != sizeof(uint8_t)) {
      throw Exceptions::SecurityException;
    }
#else
    if (fail_on_read && length != sizeof(uint8_t)) {
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
#else
  if (fail_on_read && length < 3) {
    throw Exceptions::SecurityException;
  }
#endif

  if (fail_on_read) {
    if ((type == FieldType::FIELD_TYPE_OBJECT ||
         type == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT) &&
        length < 4) {
      throw Exceptions::SecurityException;
    }
    if (type == FieldType::FIELD_TYPE_ARRAY_OBJECT && length < 5) {
      throw Exceptions::SecurityException;
    }
    if ((type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BYTE ||
         type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_BOOLEAN ||
         type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_SHORT
#ifdef JCVM_INT_SUPPORTED
         || type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_INT
#endif
         ) && length < 4) {
      throw Exceptions::SecurityException;
    }
    if (type == FieldType::FIELD_TYPE_TRANSIENT_ARRAY_OBJECT && length < 6) {
      throw Exceptions::SecurityException;
    }
  }

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

  case FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT: {
    static_assert(sizeof(jclass_index_t) == sizeof(uint16_t),
                  "jclass_index_t type should be equals to uint16_t");

    const jpackage_ID_t package = static_cast<jpackage_ID_t>(data[1]);
    const jclass_index_t claz =
        static_cast<jclass_index_t>(BYTES_TO_SHORT(data[2], data[3]));
    const bool missing_package_proxy =
        !FlashMemory_Handler::isPackageExist(package);
    const jref_t reference =
        heap.addInstance(JC_Instance(heap, package, claz, tag,
                                     missing_package_proxy));
    heap.getInstance(reference)->setShareableProxyPlaceholder();
    return reference;
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
FieldType FlashMemory_Handler::getPersistentField_TypeImpl(
    const fs::Tag &tag, const uint8_t alias_depth) {
  const auto [length, data] = FlashMemory_Handler::getDataInPlaceFromTag(tag);
  if (length == 0 || data == nullptr) {
    throw Exceptions::SecurityException;
  }
  if (data[0] == kPersistentObjectAlias) {
    return FieldType::FIELD_TYPE_OBJECT;
  }
  if (data[0] == kPersistentArrayAlias) {
    if (alias_depth >= kMaxPersistentAliasDepth) {
      throw Exceptions::SecurityException;
    }
    const fs::Tag target_tag = read_persistent_alias_target(length, data);
    const FieldType target_type =
        FlashMemory_Handler::getPersistentField_TypeImpl(
            target_tag, static_cast<uint8_t>(alias_depth + 1));
    if (!is_persistent_array_type(target_type)) {
      throw Exceptions::SecurityException;
    }
    return target_type;
  }
  if (data[0] == FieldType::FIELD_TYPE_SHAREABLE_PROXY_OBJECT) {
    return FieldType::FIELD_TYPE_OBJECT;
  }
  return static_cast<FieldType>(data[0]);
}

FieldType FlashMemory_Handler::getPersistentField_Type(const fs::Tag &tag) {
  return FlashMemory_Handler::getPersistentField_TypeImpl(tag, 0);
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
  if (!value.isNullPointer() && tag.len >= 19 && deep_trace_count < 64) {
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

  // A volatile Object[] may hold live references owned by another package.
  // Its compact handles are meaningful only in the current runtime and must
  // not be recursively serialized into this package's persistent record.
  // The in-memory field/array remain intact until reset; persisting null makes
  // the reference disappear on restoration instead of resurrecting a stale
  // cross-package handle.
  if (value.isArray()) {
    auto array = heap.getArray(value);
    if (array->hasForeignReferenceEntries()) {
      if (fs_exists(tag.value, tag.len)) {
        const uint8_t uninitialized =
            static_cast<uint8_t>(FieldType::FIELD_TYPE_UNINITIALIZED);
        FlashMemory_Handler::setDataFromTag(tag, sizeof(uninitialized),
                                            &uninitialized);
      }
      return;
    }
  }

  if (value.isGlobalInstance()) {
    uint8_t aid[16]{};
    uint8_t aid_length = 0;
    jpackage_ID_t package_id = 0;
    jclass_index_t class_index = 0;
    if (!Heap::describeGlobalAIDObject(value, aid, aid_length, package_id,
                                       class_index)) {
      throw Exceptions::SecurityException;
    }
    uint8_t data[5 + sizeof(aid)]{};
    data[0] = FieldType::FIELD_TYPE_GLOBAL_AID_OBJECT;
    data[1] = package_id;
    data[2] = static_cast<uint8_t>(class_index >> 8);
    data[3] = static_cast<uint8_t>(class_index);
    data[4] = aid_length;
    std::memcpy(data + 5, aid, aid_length);
    FlashMemory_Handler::setDataFromTag(
        tag, static_cast<uint16_t>(5 + aid_length), data);
    return;
  }

  if (value.isArray()) {
    auto array = heap.getArray(value);
    if (array->isPersistent() || array->hasPersistentTag()) {
      const fs::Tag array_tag = array->getPersistentTag();
      if (tag.len != array_tag.len ||
          std::memcmp(tag.value, array_tag.value, tag.len) != 0) {
        write_persistent_array_alias(tag, array_tag);
        return;
      }
      if (!array->isPersistent()) {
        // A transient array's canonical tag identifies only its persistent
        // clear-event record; its byte contents remain volatile.
        write_persistent_array_alias(tag, array_tag);
        return;
      }
      // Legacy arrays used their first referencing field as their storage
      // tag. Promote them to a stable body tag before the field can be
      // cleared or retargeted while aliases still refer to the same object.
      const fs::Tag body_tag = persistent_array_body_tag(
          tag, allocate_persistent_array_identity());
      FlashMemory_Handler::setPersistentField_Array(body_tag, *array, heap);
      write_persistent_array_alias(tag, body_tag);
      return;
    }
    const fs::Tag body_tag = persistent_array_body_tag(
        tag, allocate_persistent_array_identity());
    FlashMemory_Handler::setPersistentField_Array(body_tag, *array, heap);
    write_persistent_array_alias(tag, body_tag);
    return;
  }

  auto instance = heap.getInstance(value);
  FlashMemory_Handler::setPersistentField_Instance(tag, *instance, heap);
  } catch (Exceptions exception) {
    std::printf("CHOUPI:PERSIST_EXCEPTION id=%u tag_len=%u ref=%04X\r\n",
                static_cast<unsigned>(exception), tag.len, value.compact());
    std::printf("CHOUPI:PERSIST_EXCEPTION_TAG tag=");
    for (uint8_t index = 0; index < tag.len; ++index) {
      std::printf("%02X", tag.value[index]);
    }
    std::printf("\r\n");
    std::fflush(stdout);
    throw;
  }
}

void FlashMemory_Handler::setPersistentForeignReference(
    const fs::Tag &tag, const jref_t value,
    const jpackage_ID_t owner_package, Heap &owner_heap) {
  if (tag.len == 0 || tag.len > TAG_MAX_LENGTH - 6 ||
      value.isNullPointer() || value.isArray() || value.isGlobalArray() ||
      value.isGlobalInstance() || owner_package == 0xfe ||
      owner_package == 0xff) {
    throw Exceptions::SecurityException;
  }
  const auto instance = owner_heap.getInstance(value);
  if (instance->isShareableProxyPlaceholder()) {
    throw Exceptions::SecurityException;
  }
  const auto fields = instance->getFields();
  for (uint16_t index = 0; index < fields->size(); ++index) {
    const jc_field_t &field = fields->at(index);
    if (field.reference_owner != 0xff && field.reference_owner != 0xfe) {
      throw Exceptions::SecurityException;
    }
  }

  fs::Tag target_tag{};
  target_tag.value[0] = 0x03;
  target_tag.value[1] = static_cast<uint8_t>(owner_heap.getOwner());
  target_tag.value[2] = owner_package;
  target_tag.value[3] = 0xff;
  target_tag.value[4] = 0xfc;
  target_tag.value[5] = tag.len;
  std::memcpy(target_tag.value + 6, tag.value, tag.len);
  target_tag.len = static_cast<uint8_t>(6 + tag.len);
  // Keeping this alias in the foreign object's applet/package namespace lets
  // deletion and persistent inventory find it even though the static root is
  // owned by another package.
  FlashMemory_Handler::setPersistentField_Reference(target_tag, value,
                                                     owner_heap);

  std::vector<uint8_t> wrapper(3 + target_tag.len);
  wrapper[0] = static_cast<uint8_t>(FieldType::FIELD_TYPE_FOREIGN_REFERENCE);
  wrapper[1] = owner_package;
  wrapper[2] = target_tag.len;
  std::memcpy(wrapper.data() + 3, target_tag.value, target_tag.len);
  FlashMemory_Handler::setDataFromTag(tag,
                                      static_cast<uint32_t>(wrapper.size()),
                                      wrapper.data());
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
  JC_Instance &mutable_instance = const_cast<JC_Instance &>(instance);
  if (mutable_instance.hasPersistentTag()) {
    const fs::Tag existing_body = mutable_instance.getPersistentTag();
    if (!is_persistent_object_body_tag(existing_body)) {
      // Legacy object records used the first reference field as their body
      // address. Move the body to an identity tag before rewriting that field.
      (void)instance.getFields();
      const fs::Tag stable_body = persistent_migrated_object_body_tag(
          existing_body, allocate_persistent_array_identity());
      mutable_instance.retargetPersistentTag(stable_body);
      try {
        FlashMemory_Handler::setPersistentField_Instance(stable_body,
                                                         instance, heap);
        write_persistent_object_alias(existing_body, stable_body);
        if (!tags_equal(tag, existing_body)) {
          write_persistent_object_alias(tag, stable_body);
        }
      } catch (...) {
        mutable_instance.retargetPersistentTag(existing_body);
        throw;
      }
      return;
    }

    if (fs_exists(existing_body.value, existing_body.len) == 0 &&
        !tags_equal(tag, existing_body)) {
      // A transaction rollback can remove an identity published by the
      // aborted transaction while the live Java object remains reachable.
      if (is_persistent_migrated_object_body_tag(existing_body)) {
        const fs::Tag origin = persistent_object_origin_tag(existing_body);
        if (fs_exists(origin.value, origin.len) != 0 &&
            is_valid_persistent_object_body(origin)) {
          // A migrated legacy body's original location survived rollback.
          // Reattach there and reload the rollback image before retrying the
          // migration, so aliases continue to resolve to the same object.
          mutable_instance.retargetPersistentTag(origin);
          mutable_instance.invalidatePersistentFields();
          FlashMemory_Handler::setPersistentField_Instance(tag, instance,
                                                           heap);
          return;
        }
      }
      mutable_instance.clearPersistentTag();
    } else {
      if (!tags_equal(tag, existing_body)) {
        validate_persistent_object_body(existing_body);
        write_persistent_object_alias(tag, existing_body);
        return;
      }
    }
  }

  if (!mutable_instance.hasPersistentTag()) {
    const fs::Tag stable_body = persistent_object_body_tag(
        tag, allocate_persistent_array_identity());
    mutable_instance.setPersistentTag(stable_body);
    try {
      FlashMemory_Handler::setPersistentField_Instance(stable_body, instance,
                                                       heap);
      write_persistent_object_alias(tag, stable_body);
    } catch (...) {
      mutable_instance.clearPersistentTag();
      throw;
    }
    return;
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
                                           instance.getClassIndex(),
                                           instance.isShareableProxyPlaceholder());

  for (uint16_t idx = 0; idx < fields->size(); idx++) {

    fs::Tag field_tag = FlashMemory_Handler::computeTag(tag, idx);

    jc_field_t field = fields->at(idx);
    switch (field.type) {
    case FieldType::FIELD_TYPE_BYTE:
    case FieldType::FIELD_TYPE_BOOLEAN: {
      const uint8_t data[] = {field.type, static_cast<uint8_t>(field.value)};

      write_persistent_tag_or_throw(
          field_tag, data, sizeof(jbyte_t) + sizeof(uint8_t));

      break;
    }

    case FieldType::FIELD_TYPE_SHORT: {
      const uint8_t data[] = {
          field.type, static_cast<uint8_t>(HIGH_BYTE_SHORT(field.value)),
          static_cast<uint8_t>(LOW_BYTE_SHORT(field.value))};

      write_persistent_tag_or_throw(
          field_tag, data, sizeof(jshort_t) + sizeof(uint8_t));

      break;
    }

#ifdef JCVM_INT_SUPPORTED

    case FieldType::FIELD_TYPE_INT: {
      if (static_cast<std::size_t>(idx) + 1 >= fields->size()) {
        throw Exceptions::SecurityException;
      }
      jc_field_t field_low = fields->at(static_cast<uint16_t>(idx + 1));
      if (field_low.type != FieldType::FIELD_TYPE_INT_LOW) {
        throw Exceptions::SecurityException;
      }
      ++idx;
      jint_t value = signedIntFromBits(SHORTS_TO_INT(
          signedShortFromBits(field.value),
          signedShortFromBits(field_low.value)));
      const uint8_t data[] = {
          field.type,
          static_cast<uint8_t>(HIGH_BYTE_SHORT(INT_2_MSSHORTS(value))),
          static_cast<uint8_t>(LOW_BYTE_SHORT(INT_2_MSSHORTS(value))),
          static_cast<uint8_t>(HIGH_BYTE_SHORT(INT_2_LSSHORTS(value))),
          static_cast<uint8_t>(LOW_BYTE_SHORT(INT_2_LSSHORTS(value))),
      };

      write_persistent_tag_or_throw(
          field_tag, data, sizeof(jint_t) + sizeof(uint8_t));

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

  return signedShortFromBits(BYTES_TO_SHORT(data[1], data[2]));
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

  return signedIntFromBits(BYTES_TO_INT(data[1], data[2], data[3], data[4]));
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
  if (array_flash == nullptr || length < 3) {
    throw Exceptions::SecurityException;
  }

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
    // Persistent arrays have no clear-event byte; their payload starts at
    // offset three. In particular, do not read past a zero-length array's
    // serialized header.
    event = ClearEvent::None;
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

  uint32_t expected_length = 3;
  if (isTransient) {
    ++expected_length;
  }
  if (array_type == JAVA_ARRAY_T_REFERENCE) {
    expected_length += sizeof(jc_cp_offset_t);
  } else if (!isTransient) {
    uint32_t element_width = 0;
    switch (array_type) {
    case JAVA_ARRAY_T_BOOLEAN:
    case JAVA_ARRAY_T_BYTE:
      element_width = sizeof(jbyte_t);
      break;
    case JAVA_ARRAY_T_SHORT:
      element_width = sizeof(jshort_t);
      break;
#ifdef JCVM_INT_SUPPORTED
    case JAVA_ARRAY_T_INT:
      element_width = sizeof(jint_t);
      break;
#endif /* JCVM_INT_SUPPORTED */
    case JAVA_ARRAY_T_REFERENCE:
      break;
    }
    expected_length += static_cast<uint32_t>(size) * element_width;
  }
  if (length != expected_length) {
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
      (array->getType() != JAVA_ARRAY_T_BYTE &&
       array->getType() != JAVA_ARRAY_T_BOOLEAN) ||
      array->size() <= index) {
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

void FlashMemory_Handler::setPersistentField_Array_ByteNonAtomic(
    const fs::Tag &tag, const uint16_t index, const jbyte_t value
#ifdef JCVM_SECURE_HEAP_ACCESS
    , Heap &heap
#endif /* JCVM_SECURE_HEAP_ACCESS */
) {
#ifdef JCVM_SECURE_HEAP_ACCESS
  auto array = FlashMemory_Handler::getPersistentField_Array(tag, heap);
  if (array->isTransientArray() ||
      array->getType() != JAVA_ARRAY_T_BYTE || array->size() <= index) {
    throw Exceptions::SecurityException;
  }
#endif /* JCVM_SECURE_HEAP_ACCESS */

  if (fs_write_1b_at(&(tag.value[0]), tag.len, 3u + index, value)) {
    throw Exceptions::IOException;
  }
  preserve_non_atomic_array_byte_in_snapshot(tag, index, value);
}

void FlashMemory_Handler::clearPersistentField_Array_Byte(
    const fs::Tag &tag, const uint16_t size) {
  FlashMemory_Handler::writePersistentField_Array_Byte(
      tag, std::vector<uint8_t>(size, 0));
}

void FlashMemory_Handler::writePersistentField_Array_Byte(
    const fs::Tag &tag, const std::vector<uint8_t> &contents) {
  auto [length, stored] = FlashMemory_Handler::getDataInPlaceFromTag(tag);
  const uint32_t expected_length =
      3u + static_cast<uint32_t>(contents.size());
  if (length != expected_length || stored[0] != FieldType::FIELD_TYPE_ARRAY_BYTE ||
      BYTES_TO_SHORT(stored[1], stored[2]) != contents.size()) {
    throw Exceptions::SecurityException;
  }

  std::vector<uint8_t> updated(stored, stored + length);
  std::copy(contents.begin(), contents.end(), updated.begin() + 3);
  write_persistent_tag_or_throw(tag, updated.data(), length);
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

  return signedShortFromBits(BYTES_TO_SHORT(value_high, value_low));
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
  uint8_t write_status = fs_write_1b_at(
      &(tag.value[0]), tag.len, byte_offset,
      static_cast<uint8_t>(encoded >> 8));
  if (write_status == 0) {
    write_status = fs_write_1b_at(&(tag.value[0]), tag.len, byte_offset + 1,
                                  static_cast<uint8_t>(encoded));
  }
  if (write_status != 0) {
#ifdef PC_VERSION
    std::fprintf(stderr,
                 "PERSIST_SHORT_WRITE_FAIL status=%u index=%u tag_len=%u tag=",
                 static_cast<unsigned>(write_status),
                 static_cast<unsigned>(index),
                 static_cast<unsigned>(tag.len));
    for (uint8_t tag_index = 0; tag_index < tag.len; ++tag_index) {
      std::fprintf(stderr, "%02X", tag.value[tag_index]);
    }
    std::fprintf(stderr, "\n");
#endif
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
    // A deleted dynamic package must never fall through to the persistent
    // ROM-package tag namespace (0x01, packageID).
    throw Exceptions::IOException;
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
