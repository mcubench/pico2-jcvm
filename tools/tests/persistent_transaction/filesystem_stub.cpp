#include "ffi.h"
#include "context.hpp"
#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/package.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace persistent_transaction_test {
uint16_t instance_field_count = 0;
extern bool track_array_allocations;
extern unsigned int tracked_array_allocations;
extern unsigned int tracked_array_deallocations;
bool dynamic_cap_present[8]{};
const uint8_t dynamic_cap_code[] = {0x01};
} // namespace persistent_transaction_test

extern "C" const uint8_t *choupi_pc_rom_package_data(uint8_t,
                                                       uint32_t *length) {
  if (length != nullptr) *length = 0;
  return nullptr;
}

uint16_t persistent_transaction_test_pending_reason = 0;
extern "C" void choupi_record_transaction_buffer_full(void) {
  persistent_transaction_test_pending_reason = 3;
}

namespace jcvm {
Context &Context::getActiveContext() { throw Exceptions::RuntimeException; }
void Context::setPendingSystemExceptionReason(uint16_t) noexcept {}
Package::Package(jpackage_ID_t package_id) noexcept : ID(package_id) {}
const uint16_t Class_Handler::getInstanceFieldsSize(
    jclass_index_t) const {
  return persistent_transaction_test::instance_field_count;
}
} // namespace jcvm

extern "C" void choupi_vm_record_persistent_restore_failure(
    uint16_t, uint16_t, uint16_t, uint8_t, uint8_t, uint8_t) {}

extern "C" void path_applet_field(uint8_t applet, uint8_t package,
                                   uint8_t class_index, uint8_t field,
                                   uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x03;
  (*tag)[1] = applet;
  (*tag)[2] = package;
  (*tag)[3] = class_index;
  (*tag)[4] = field;
  *length = 5;
}

extern "C" void path_applet_field16(uint8_t applet, uint8_t package,
                                     uint16_t class_index, uint8_t field,
                                     uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x03;
  (*tag)[1] = applet;
  (*tag)[2] = package;
  (*tag)[3] = static_cast<uint8_t>(class_index >> 8);
  (*tag)[4] = static_cast<uint8_t>(class_index);
  (*tag)[5] = field;
  *length = 6;
}

extern "C" void path_static(uint8_t package, uint8_t field,
                             uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x02;
  (*tag)[1] = package;
  (*tag)[2] = field;
  *length = 3;
}

extern "C" void path_static16(uint8_t package, uint16_t static_id,
                               uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x02;
  (*tag)[1] = package;
  (*tag)[2] = static_cast<uint8_t>(static_id >> 8);
  (*tag)[3] = static_cast<uint8_t>(static_id);
  *length = 4;
}

extern "C" void path_package_list(uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x00;
  *length = 1;
}

extern "C" const uint8_t *choupi_cap_store_data_for_package(
    uint8_t package_id, uint32_t *length) {
  if (length == nullptr || package_id < 56 || package_id > 63 ||
      !persistent_transaction_test::dynamic_cap_present[package_id - 56]) {
    return nullptr;
  }
  *length = sizeof(persistent_transaction_test::dynamic_cap_code);
  return persistent_transaction_test::dynamic_cap_code;
}

namespace {
std::map<std::string, std::vector<uint8_t>> files;
bool fail_next_write = false;
bool fail_next_read = false;
bool fail_next_sync = false;
bool stall_after_one_cursor_result = false;
bool cursor_result_returned = false;
int erase_fail_after = -1;
unsigned int sync_count = 0;
unsigned int array_allocation_start = 0;
unsigned int array_deallocation_start = 0;

std::string key_for(const uint8_t *tag, const uint8_t length) {
  return std::string(reinterpret_cast<const char *>(tag), length);
}
} // namespace

extern "C" uint8_t fs_read_1b_at(const uint8_t *tag, uint8_t tag_length,
                                  uint32_t offset, uint8_t *value) {
  const auto found = files.find(key_for(tag, tag_length));
  if (found == files.end() || value == nullptr || offset >= found->second.size())
    return 1;
  *value = found->second[offset];
  return 0;
}

extern "C" uint8_t fs_exists(const uint8_t *tag, const uint8_t tag_length) {
  return files.count(key_for(tag, tag_length)) != 0;
}

extern "C" uint8_t fs_length(const uint8_t *tag, const uint8_t tag_length,
                             uint32_t *length) {
  const auto found = files.find(key_for(tag, tag_length));
  if (found == files.end() || length == nullptr) return 2;
  *length = static_cast<uint32_t>(found->second.size());
  return 0;
}

extern "C" uint8_t fs_read(const uint8_t *tag, const uint8_t tag_length,
                           uint8_t *data, const uint32_t length) {
  if (fail_next_read) {
    fail_next_read = false;
    return 1;
  }
  const auto found = files.find(key_for(tag, tag_length));
  if (found == files.end() || data == nullptr || found->second.size() != length) {
    return 2;
  }
  std::copy(found->second.begin(), found->second.end(), data);
  return 0;
}

extern "C" uint8_t fs_read_inplace(const uint8_t *tag,
                                    const uint8_t tag_length,
                                    const uint8_t **data, uint32_t *length) {
  const auto found = files.find(key_for(tag, tag_length));
  if (found == files.end() || data == nullptr || length == nullptr) return 2;
  *data = found->second.data();
  *length = static_cast<uint32_t>(found->second.size());
  return 0;
}

extern "C" uint8_t fs_write(const uint8_t *tag, const uint8_t tag_length,
                            const uint8_t *data, const uint32_t length) {
  if (fail_next_write) {
    fail_next_write = false;
    return 1;
  }
  if (tag == nullptr || tag_length == 0 || data == nullptr) return 3;
  files[key_for(tag, tag_length)] = std::vector<uint8_t>(data, data + length);
  return 0;
}

extern "C" uint8_t fs_erase(const uint8_t *tag, const uint8_t tag_length) {
  if (erase_fail_after == 0) {
    erase_fail_after = -1;
    return 1;
  }
  if (erase_fail_after > 0) --erase_fail_after;
  return files.erase(key_for(tag, tag_length)) == 1 ? 0 : 2;
}

extern "C" uint8_t fs_write_1b_at(const uint8_t *tag,
                                   const uint8_t tag_length,
                                   const uint32_t offset,
                                   const uint8_t value) {
  if (fail_next_write) {
    fail_next_write = false;
    return 1;
  }
  const auto found = files.find(key_for(tag, tag_length));
  if (found == files.end() || offset >= found->second.size()) return 2;
  found->second[offset] = value;
  return 0;
}

extern "C" uint8_t fs_sync(void) {
  ++sync_count;
  if (fail_next_sync) {
    fail_next_sync = false;
    return 1;
  }
  return 0;
}

extern "C" uint8_t fs_next_tag(const uint8_t *prefix,
                                const uint8_t prefix_length,
                                fs_tag_next_request_t *request) {
  if (request == nullptr || (prefix_length != 0 && prefix == nullptr)) return 2;
  if (stall_after_one_cursor_result && cursor_result_returned) {
    stall_after_one_cursor_result = false;
    cursor_result_returned = false;
    return 0;
  }
  if (request->bucket != 0 || request->offset > files.size()) return 2;
  size_t index = request->offset;
  auto it = files.begin();
  std::advance(it, static_cast<std::ptrdiff_t>(index));
  for (; it != files.end(); ++it, ++index) {
    const auto &tag = it->first;
    if (tag.size() < prefix_length ||
        std::memcmp(tag.data(), prefix, prefix_length) != 0) continue;
    if (index >= UINT16_MAX) return 2;
    request->bucket = 0;
    request->offset = static_cast<uint16_t>(index + 1);
    request->tag[0] = static_cast<uint8_t>(tag.size());
    std::memcpy(request->tag + 1, tag.data(), tag.size());
    if (stall_after_one_cursor_result) cursor_result_returned = true;
    return 0;
  }
  request->bucket = 0;
  request->offset = static_cast<uint16_t>(files.size());
  return 1;
}

namespace persistent_transaction_test {
void force_cursor_stall_after_one_result() {
  stall_after_one_cursor_result = true;
  cursor_result_returned = false;
}

void fail_erase_after(unsigned int successful_erases) {
  erase_fail_after = static_cast<int>(successful_erases);
}

std::vector<uint8_t> read(const uint8_t *tag, const uint8_t length) {
  const auto found = files.find(key_for(tag, length));
  return found == files.end() ? std::vector<uint8_t>{} : found->second;
}

void seed(const uint8_t *tag, const uint8_t tag_length,
          std::vector<uint8_t> value) {
  files[key_for(tag, tag_length)] = std::move(value);
}

void fail_one_write() { fail_next_write = true; }
void fail_one_read() { fail_next_read = true; }
void fail_one_sync() { fail_next_sync = true; }
unsigned int sync_count_value() { return sync_count; }
void start_array_allocation_tracking() {
  array_allocation_start = persistent_transaction_test::tracked_array_allocations;
  array_deallocation_start =
      persistent_transaction_test::tracked_array_deallocations;
  persistent_transaction_test::track_array_allocations = true;
}
std::pair<unsigned int, unsigned int> stop_array_allocation_tracking() {
  persistent_transaction_test::track_array_allocations = false;
  return {persistent_transaction_test::tracked_array_allocations -
              array_allocation_start,
          persistent_transaction_test::tracked_array_deallocations -
              array_deallocation_start};
}
} // namespace persistent_transaction_test
