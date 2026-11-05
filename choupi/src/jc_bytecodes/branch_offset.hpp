#ifndef CHOUPI_BRANCH_OFFSET_HPP
#define CHOUPI_BRANCH_OFFSET_HPP

#include <cstddef>
#include <cstdint>

namespace jcvm::branch_offset {

// Branch operands are signed byte displacements measured from the opcode.
// Callers have already advanced past the opcode and operand, so convert back
// to opcode-relative form using signed arithmetic before subtracting widths.
constexpr int64_t after_operand(const int64_t displacement,
                               const std::size_t operand_size) noexcept {
  return displacement - static_cast<int64_t>(operand_size) - 1;
}

} // namespace jcvm::branch_offset

#endif // CHOUPI_BRANCH_OFFSET_HPP
