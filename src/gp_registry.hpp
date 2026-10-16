#ifndef PICO2_JCVM_GP_REGISTRY_HPP
#define PICO2_JCVM_GP_REGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gp {

struct AidSearch {
  const uint8_t *aid = nullptr;
  uint8_t length = 0;
};

// Parse the AID search qualifier from the BER-TLV GET STATUS data field.
// Unknown well-formed search qualifiers are skipped as allowed by GP.
inline bool parse_aid_search(const uint8_t *data, size_t data_length,
                             AidSearch &search) noexcept {
  search = {};
  if (data == nullptr) return false;

  size_t cursor = 0;
  bool found_aid = false;
  while (cursor < data_length) {
    const size_t tag_start = cursor;
    const uint8_t first_tag_byte = data[cursor++];
    uint32_t tag = first_tag_byte;
    if ((first_tag_byte & 0x1f) == 0x1f) {
      uint8_t tag_bytes = 1;
      uint8_t next_tag_byte = 0;
      do {
        if (cursor >= data_length || tag_bytes == 3) return false;
        next_tag_byte = data[cursor++];
        tag = (tag << 8) | next_tag_byte;
        ++tag_bytes;
      } while ((next_tag_byte & 0x80) != 0);
    }

    if (cursor >= data_length) return false;
    const uint8_t first_length_byte = data[cursor++];
    size_t value_length = 0;
    if ((first_length_byte & 0x80) == 0) {
      value_length = first_length_byte;
    } else {
      const uint8_t length_bytes = first_length_byte & 0x7f;
      if (length_bytes == 0 || length_bytes > 2 ||
          length_bytes > data_length - cursor) {
        return false;
      }
      for (uint8_t index = 0; index < length_bytes; ++index) {
        value_length = (value_length << 8) | data[cursor++];
      }
    }
    if (value_length > data_length - cursor) return false;

    if (tag == 0x4f) {
      if (tag_start != 0 || found_aid || value_length > 16 ||
          (value_length != 0 && value_length < 5)) {
        return false;
      }
      search.aid = data + cursor;
      search.length = static_cast<uint8_t>(value_length);
      found_aid = true;
    }
    cursor += value_length;
    if (cursor <= tag_start) return false;
  }
  return found_aid;
}

inline bool aid_matches_search(const AidSearch &search, const uint8_t *aid,
                               uint8_t aid_length) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16 ||
      search.aid == nullptr) {
    return false;
  }
  if (search.length == 0) return true;
  if (search.length == 5) {
    return aid_length >= 5 && std::memcmp(search.aid, aid, 5) == 0;
  }
  return search.length == aid_length &&
         std::memcmp(search.aid, aid, aid_length) == 0;
}

// Append one applet registry template (E3) in GET STATUS response format 1.
// It contains the instance AID (4F), Executable Load File AID (C4), life-cycle
// state (9F70), and privileges (C5).
inline bool append_registry_entry(uint8_t *output, size_t capacity,
                                  size_t &output_length,
                                  const uint8_t *instance_aid,
                                  uint8_t instance_aid_length,
                                  const uint8_t *package_aid,
                                  uint8_t package_aid_length,
                                  uint8_t life_cycle_state) noexcept {
  output_length = 0;
  if (output == nullptr || instance_aid == nullptr || package_aid == nullptr ||
      instance_aid_length < 5 || instance_aid_length > 16 ||
      package_aid_length < 5 || package_aid_length > 16) {
    return false;
  }

  const size_t value_length = static_cast<size_t>(instance_aid_length) +
                              package_aid_length + 11;
  const size_t entry_length = value_length + 2;
  if (value_length > 0x7f || entry_length > capacity) return false;

  size_t cursor = 0;
  output[cursor++] = 0xe3;
  output[cursor++] = static_cast<uint8_t>(value_length);
  output[cursor++] = 0x4f;
  output[cursor++] = instance_aid_length;
  std::memcpy(output + cursor, instance_aid, instance_aid_length);
  cursor += instance_aid_length;
  output[cursor++] = 0xc4;
  output[cursor++] = package_aid_length;
  std::memcpy(output + cursor, package_aid, package_aid_length);
  cursor += package_aid_length;
  output[cursor++] = 0x9f;
  output[cursor++] = 0x70;
  output[cursor++] = 0x01;
  output[cursor++] = life_cycle_state;
  output[cursor++] = 0xc5;
  output[cursor++] = 0x01;
  output[cursor++] = 0x00;

  if (cursor != entry_length) return false;
  output_length = cursor;
  return true;
}

// The ISD is selected explicitly by GET STATUS P1=80, so its AID search
// qualifier is ignored as required by GlobalPlatform Card Specification 2.2.1.
inline bool append_isd_registry_entry(uint8_t *output, size_t capacity,
                                      size_t &output_length) noexcept {
  output_length = 0;
  constexpr uint8_t entry[] = {
      0xe3, 0x11, 0x4f, 0x08, 0xa0, 0x00, 0x00, 0x01, 0x51, 0x00,
      0x00, 0x00, 0x9f, 0x70, 0x01, 0x01, 0xc5, 0x01, 0x00};
  if (output == nullptr || capacity < sizeof(entry)) return false;
  std::memcpy(output, entry, sizeof(entry));
  output_length = sizeof(entry);
  return true;
}

// Append an Executable Load File E3 entry. When include_modules is true, the
// provided module AID is emitted as tag 84.
inline bool append_load_file_entry(uint8_t *output, size_t capacity,
                                   size_t &output_length,
                                   const uint8_t *package_aid,
                                   uint8_t package_aid_length,
                                   const uint8_t *module_aid,
                                   uint8_t module_aid_length,
                                   bool include_modules) noexcept {
  output_length = 0;
  if (output == nullptr || package_aid == nullptr ||
      package_aid_length < 5 || package_aid_length > 16 ||
      (include_modules &&
       (module_aid == nullptr || module_aid_length < 5 ||
        module_aid_length > 16))) {
    return false;
  }

  const size_t value_length = static_cast<size_t>(package_aid_length) + 6 +
                              (include_modules ? module_aid_length + 2 : 0);
  const size_t entry_length = value_length + 2;
  if (value_length > 0x7f || entry_length > capacity) return false;

  size_t cursor = 0;
  output[cursor++] = 0xe3;
  output[cursor++] = static_cast<uint8_t>(value_length);
  output[cursor++] = 0x4f;
  output[cursor++] = package_aid_length;
  std::memcpy(output + cursor, package_aid, package_aid_length);
  cursor += package_aid_length;
  output[cursor++] = 0x9f;
  output[cursor++] = 0x70;
  output[cursor++] = 0x01;
  output[cursor++] = 0x01; // Executable Load File is LOADED.
  if (include_modules) {
    output[cursor++] = 0x84;
    output[cursor++] = module_aid_length;
    std::memcpy(output + cursor, module_aid, module_aid_length);
    cursor += module_aid_length;
  }

  if (cursor != entry_length) return false;
  output_length = cursor;
  return true;
}

} // namespace gp

#endif
