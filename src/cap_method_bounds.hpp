#ifndef CHOUPI_CAP_METHOD_BOUNDS_HPP
#define CHOUPI_CAP_METHOD_BOUNDS_HPP

#include <cstddef>
#include <cstdint>

namespace jcvm::cap_method_bounds {

constexpr std::size_t header_size(uint8_t first_header_byte) noexcept {
  // extended_method_header_info is four bytes in JCVMS 3.0.5 §6.10.3:
  // flags/padding, max_stack, nargs, and max_locals. Normal headers are two.
  return (first_header_byte & 0x80u) != 0 ? 4u : 2u;
}

constexpr bool header_fits(std::size_t method_bytes, std::size_t offset,
                           uint8_t first_header_byte) noexcept {
  if (offset >= method_bytes) return false;
  return header_size(first_header_byte) <= method_bytes - offset;
}

} // namespace jcvm::cap_method_bounds

#endif // CHOUPI_CAP_METHOD_BOUNDS_HPP
