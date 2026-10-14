#include "ffi.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace {
std::map<std::string, std::vector<uint8_t>> files;
bool fail_next_write = false;

std::string key_for(const uint8_t *tag, const uint8_t length) {
  return std::string(reinterpret_cast<const char *>(tag), length);
}
} // namespace

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
} // namespace persistent_transaction_test
