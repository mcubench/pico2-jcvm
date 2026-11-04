#ifndef CHOUPI_CAP_INSTRUCTION_BOUNDARIES_HPP
#define CHOUPI_CAP_INSTRUCTION_BOUNDARIES_HPP

#include <cstddef>
#include <cstdint>

namespace jcvm::cap_instruction_boundaries {

inline bool has_bytes(const std::size_t size, const std::size_t offset,
                      const std::size_t count) noexcept {
  return offset <= size && count <= size - offset;
}

inline uint16_t read_u16(const uint8_t *code,
                         const std::size_t offset) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(code[offset]) << 8) |
                               code[offset + 1]);
}

inline int16_t read_s16(const uint8_t *code,
                        const std::size_t offset) noexcept {
  const uint16_t bits = read_u16(code, offset);
  const int32_t value = (bits & 0x8000u) != 0
                            ? static_cast<int32_t>(bits) - 0x10000
                            : static_cast<int32_t>(bits);
  return static_cast<int16_t>(value);
}

inline int32_t read_s32(const uint8_t *code,
                        const std::size_t offset) noexcept {
  const uint32_t bits = (static_cast<uint32_t>(code[offset]) << 24) |
                        (static_cast<uint32_t>(code[offset + 1]) << 16) |
                        (static_cast<uint32_t>(code[offset + 2]) << 8) |
                        static_cast<uint32_t>(code[offset + 3]);
  const int64_t value = (bits & 0x80000000u) != 0
                            ? static_cast<int64_t>(bits) - 0x100000000LL
                            : static_cast<int64_t>(bits);
  return static_cast<int32_t>(value);
}

// Return the complete encoded width of a Java Card VM instruction. Unknown
// opcodes and malformed variable-size switch instructions fail closed.
inline bool instruction_width(const uint8_t *code, const std::size_t size,
                              const std::size_t offset,
                              std::size_t &width) noexcept {
  if (code == nullptr || !has_bytes(size, offset, 1)) return false;
  const uint8_t opcode = code[offset];

  if (opcode <= 0x0f || (opcode >= 0x18 && opcode <= 0x27) ||
      (opcode >= 0x2b && opcode <= 0x3e) ||
      (opcode >= 0x41 && opcode <= 0x58) ||
      (opcode >= 0x5b && opcode <= 0x5f) ||
      (opcode >= 0x77 && opcode <= 0x7a) || opcode == 0x92 ||
      opcode == 0x93) {
    width = 1;
    return true;
  }

  if (opcode == 0x10 || opcode == 0x12 ||
      (opcode >= 0x15 && opcode <= 0x17) ||
      (opcode >= 0x28 && opcode <= 0x2a) || opcode == 0x3f ||
      opcode == 0x40 || opcode == 0x59 || opcode == 0x5a ||
      (opcode >= 0x60 && opcode <= 0x72) || opcode == 0x90 ||
      (opcode >= 0xad && opcode <= 0xb0) ||
      (opcode >= 0xb5 && opcode <= 0xb8)) {
    width = (opcode == 0x59 || opcode == 0x5a) ? 3 : 2;
    return has_bytes(size, offset, width);
  }

  if (opcode == 0x11 || opcode == 0x13 ||
      (opcode >= 0x7b && opcode <= 0x8d) || opcode == 0x8f ||
      opcode == 0x91 || (opcode >= 0xa9 && opcode <= 0xac) ||
      (opcode >= 0xb1 && opcode <= 0xb4)) {
    width = (opcode >= 0x83 && opcode <= 0x8a) ? 2 : 3;
    return has_bytes(size, offset, width);
  }

  if (opcode == 0x14) {
    width = 5;
    return has_bytes(size, offset, width);
  }
  if (opcode == 0x8e) {
    width = 5;
    return has_bytes(size, offset, width);
  }
  if (opcode == 0x94 || opcode == 0x95) {
    width = 4; // atype byte followed by a constant-pool index.
    return has_bytes(size, offset, width);
  }
  if (opcode == 0x96 || opcode == 0x97) {
    width = 4;
    return has_bytes(size, offset, width);
  }
  if (opcode >= 0x98 && opcode <= 0xa8) {
    width = 3;
    return has_bytes(size, offset, width);
  }

  if (opcode == 0x73) {
    if (!has_bytes(size, offset, 7)) return false;
    const int16_t low = read_s16(code, offset + 3);
    const int16_t high = read_s16(code, offset + 5);
    if (low > high) return false;
    const std::size_t entries =
        static_cast<std::size_t>(static_cast<int32_t>(high) - low) + 1;
    if (entries > (size - offset - 7) / 2) return false;
    width = 7 + entries * 2;
    return true;
  }
  if (opcode == 0x74) {
    if (!has_bytes(size, offset, 11)) return false;
    const int32_t low = read_s32(code, offset + 3);
    const int32_t high = read_s32(code, offset + 7);
    if (low > high) return false;
    const uint64_t entries =
        static_cast<uint64_t>(static_cast<int64_t>(high) - low) + 1;
    if (entries > (size - offset - 11) / 2) return false;
    width = 11 + static_cast<std::size_t>(entries) * 2;
    return true;
  }
  if (opcode == 0x75 || opcode == 0x76) {
    if (!has_bytes(size, offset, 5)) return false;
    const std::size_t pair_width = opcode == 0x75 ? 4 : 6;
    const std::size_t pairs = read_u16(code, offset + 3);
    if (pairs > (size - offset - 5) / pair_width) return false;
    width = 5 + pairs * pair_width;
    return true;
  }

  return false;
}

inline bool is_instruction_start(const uint8_t *code, const std::size_t size,
                                 const std::size_t target) noexcept {
  if (code == nullptr || target >= size) return false;
  std::size_t offset = 0;
  while (offset < target) {
    std::size_t width = 0;
    if (!instruction_width(code, size, offset, width) || width == 0 ||
        width > target - offset) {
      return false;
    }
    offset += width;
  }
  return offset == target;
}

inline bool check_target(const uint8_t *code, const std::size_t size,
                         const std::size_t origin,
                         const int64_t displacement) noexcept {
  const int64_t target = static_cast<int64_t>(origin) + displacement;
  return target >= 0 && static_cast<uint64_t>(target) < size &&
         is_instruction_start(code, size, static_cast<std::size_t>(target));
}

inline bool validate_switch_targets(const uint8_t *code,
                                    const std::size_t size,
                                    const std::size_t origin,
                                    const uint8_t opcode,
                                    const std::size_t width) noexcept {
  if (opcode == 0x73) {
    if (!check_target(code, size, origin, read_s16(code, origin + 1)))
      return false;
    const int16_t low = read_s16(code, origin + 3);
    const int16_t high = read_s16(code, origin + 5);
    for (std::size_t index = 0;
         index < static_cast<std::size_t>(static_cast<int32_t>(high) - low) + 1;
         ++index) {
      if (!check_target(code, size, origin,
                        read_s16(code, origin + 7 + index * 2))) {
        return false;
      }
    }
    return true;
  }
  if (opcode == 0x74) {
    if (!check_target(code, size, origin, read_s16(code, origin + 1)))
      return false;
    const int32_t low = read_s32(code, origin + 3);
    const int32_t high = read_s32(code, origin + 7);
    const std::size_t entries =
        static_cast<std::size_t>(static_cast<int64_t>(high) - low) + 1;
    for (std::size_t index = 0; index < entries; ++index) {
      if (!check_target(code, size, origin,
                        read_s16(code, origin + 11 + index * 2))) {
        return false;
      }
    }
    return true;
  }
  const std::size_t pair_width = opcode == 0x75 ? 4 : 6;
  const std::size_t pairs = read_u16(code, origin + 3);
  if (!check_target(code, size, origin, read_s16(code, origin + 1)))
    return false;
  for (std::size_t index = 0; index < pairs; ++index) {
    const std::size_t pair = origin + 5 + index * pair_width;
    const std::size_t target_offset = pair + pair_width - 2;
    if (!check_target(code, size, origin,
                      read_s16(code, target_offset))) {
      return false;
    }
  }
  return width >= 5;
}

inline bool validate_control_flow(const uint8_t *code,
                                  const std::size_t size) noexcept {
  if (code == nullptr || size == 0) return false;
  std::size_t offset = 0;
  while (offset < size) {
    std::size_t width = 0;
    if (!instruction_width(code, size, offset, width)) return false;
    const uint8_t opcode = code[offset];
    if ((opcode >= 0x60 && opcode <= 0x71) ||
        (opcode >= 0x98 && opcode <= 0xa8)) {
      const int64_t displacement = opcode <= 0x71
          ? static_cast<int8_t>(code[offset + 1])
          : read_s16(code, offset + 1);
      if (!check_target(code, size, offset, displacement)) return false;
    } else if (opcode >= 0x73 && opcode <= 0x76) {
      if (!validate_switch_targets(code, size, offset, opcode, width))
        return false;
    }
    offset += width;
  }
  return offset == size;
}

} // namespace jcvm::cap_instruction_boundaries

#endif // CHOUPI_CAP_INSTRUCTION_BOUNDARIES_HPP
