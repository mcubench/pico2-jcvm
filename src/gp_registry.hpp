#ifndef PICO2_JCVM_GP_REGISTRY_HPP
#define PICO2_JCVM_GP_REGISTRY_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace gp {

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

} // namespace gp

#endif
