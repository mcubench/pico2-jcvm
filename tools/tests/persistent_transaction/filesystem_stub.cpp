#include "ffi.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace persistent_transaction_test {
extern bool track_array_allocations;
extern unsigned int tracked_array_allocations;
extern unsigned int tracked_array_deallocations;
bool dynamic_cap_present[8]{};
const uint8_t dynamic_cap_code[] = {0x01};
} // namespace persistent_transaction_test

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
  return files.erase(key_for(tag, tag_length)) == 1 ? 0 : 2;
}

extern "C" uint8_t fs_sync(void) {
  ++sync_count;
  if (fail_next_sync) {
    fail_next_sync = false;
    return 1;
  }
  return 0;
}

namespace persistent_transaction_test {
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
