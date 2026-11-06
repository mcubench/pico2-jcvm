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

#ifndef _PC_T_HPP
#define _PC_T_HPP

#include "../jc_config.h"
#include "../exceptions.hpp"
#include "../jc_utils.hpp"
#include "../types.hpp"
#include "../../../src/cap_instruction_boundaries.hpp"

#include <cstddef>
#include <cstdint>

namespace jcvm {
class pc_t {
private:
  const uint8_t *value;
  const uint8_t *begin;
  const uint8_t *end;

  uintptr_t address(const uint8_t *pointer) const noexcept {
    return reinterpret_cast<uintptr_t>(pointer);
  }

  bool contains(const uint8_t *pointer, const bool allow_end) const noexcept {
    const uintptr_t start = address(this->begin);
    const uintptr_t limit = address(this->end);
    const uintptr_t candidate = address(pointer);
    return this->begin != nullptr && this->end != nullptr &&
           start < limit && candidate >= start &&
           (allow_end ? candidate <= limit : candidate < limit);
  }

  bool offset_target(const uint8_t *origin, const int64_t offset,
                     uintptr_t &target) const noexcept {
    if (!this->contains(origin, true)) return false;
    target = this->address(origin);
    if (offset >= 0) {
      const uint64_t distance = static_cast<uint64_t>(offset);
      if (distance > this->address(this->end) - target) return false;
      target += static_cast<uintptr_t>(distance);
    } else {
      const uint64_t distance =
          static_cast<uint64_t>(-(offset + 1)) + static_cast<uint64_t>(1);
      if (distance > target - this->address(this->begin)) return false;
      target -= static_cast<uintptr_t>(distance);
    }
    return target >= this->address(this->begin) &&
           target < this->address(this->end);
  }

  void require_readable(const size_t length) const {
    if (!this->contains(this->value, false) ||
        length > address(this->end) - address(this->value)) {
      throw Exceptions::SecurityException;
    }
  }

public:
  pc_t(const uint8_t *value, const uint8_t *begin, const uint8_t *end)
      : value(value), begin(begin), end(end) {
    if (!this->contains(value, false)) {
      throw Exceptions::SecurityException;
    }
  }

  /**
   * Sets the new PC value
   *
   * @param[value] new PC value
   */
  void setValue(pc_t pc) {
    if (this->begin != pc.begin || this->end != pc.end ||
        !this->contains(pc.value, false)) {
      throw Exceptions::SecurityException;
    }
    this->value = pc.value;
  }

  /**
   * Sets the new PC value
   *
   * @param[value] new PC value
   */
  void setValue(const uint8_t *value) {
    if (!this->contains(value, false)) {
      throw Exceptions::SecurityException;
    }
    this->value = value;
  }

  /// Set the PC to origin + offset without forming an out-of-range pointer.
  void setFromOffset(const uint8_t *origin, const int64_t offset) {
    uintptr_t target = 0;
    if (!this->offset_target(origin, offset, target))
      throw Exceptions::SecurityException;
    this->value = reinterpret_cast<const uint8_t *>(target);
  }

  /// Set a branch target, optionally requiring the target to start an opcode.
  void setBranchFromOffset(const uint8_t *origin, const int64_t offset,
                           const bool require_instruction_start) {
    uintptr_t target = 0;
    if (!this->offset_target(origin, offset, target))
      throw Exceptions::SecurityException;
    if (require_instruction_start) {
      const std::size_t code_size = static_cast<std::size_t>(
          this->address(this->end) - this->address(this->begin));
      const std::size_t target_offset = static_cast<std::size_t>(
          target - this->address(this->begin));
      if (!cap_instruction_boundaries::is_instruction_start(
              this->begin, code_size, target_offset)) {
        throw Exceptions::SecurityException;
      }
    }
    this->value = reinterpret_cast<const uint8_t *>(target);
  }

  /// Check an instruction boundary inside this method frame.
  bool isInstructionBoundary(const uint8_t *target,
                             const bool allow_end) const noexcept {
    if (!this->contains(target, allow_end)) return false;
    const std::size_t code_size = static_cast<std::size_t>(
        this->address(this->end) - this->address(this->begin));
    const std::size_t target_offset = static_cast<std::size_t>(
        this->address(target) - this->address(this->begin));
    if (allow_end && target_offset == code_size) return true;
    return cap_instruction_boundaries::is_instruction_start(
        this->begin, code_size, target_offset);
  }

  /// Check whether an address belongs to this method frame.
  bool containsAddress(const uint8_t *target,
                       const bool allow_end) const noexcept {
    return this->contains(target, allow_end);
  }

  /// Branch relative to the current PC value, with optional opcode alignment.
  void updateBranchFromOffset(const int64_t offset,
                              const bool require_instruction_start) {
    this->setBranchFromOffset(this->value, offset,
                              require_instruction_start);
  }

  /// Check an offset range without forming a potentially out-of-range pointer.
  bool canReadAtOffset(const uint8_t *origin, const int64_t offset,
                       const size_t length) const noexcept {
    if (!this->contains(origin, true)) return false;
    uintptr_t target = this->address(origin);
    if (offset >= 0) {
      const uint64_t distance = static_cast<uint64_t>(offset);
      if (distance > this->address(this->end) - target) return false;
      target += static_cast<uintptr_t>(distance);
    } else {
      const uint64_t distance =
          static_cast<uint64_t>(-(offset + 1)) + static_cast<uint64_t>(1);
      if (distance > target - this->address(this->begin)) return false;
      target -= static_cast<uintptr_t>(distance);
    }
    return target < this->address(this->end) &&
           length <= this->address(this->end) - target;
  }

  /**
   * Returns the PC value.
   */
  const uint8_t *getValue() const noexcept { return this->value; }

  /**
   * Gets next byte value and increment the PC value.
   */
  jbyte_t getNextByte() {
    this->require_readable(1);
    jbyte_t ret = static_cast<jbyte_t>(*this->value);
    ++this->value;
    return ret;
  }

  /**
   * Gets next 2-byte value and increment the PC value.
   */
  jshort_t getNextShort() {
    this->require_readable(2);
    jbyte_t msb = static_cast<jbyte_t>(this->value[0]);
    jbyte_t lsb = static_cast<jbyte_t>(this->value[1]);
    this->value += 2;
    return signedShortFromBits(BYTES_TO_SHORT(msb, lsb));
  }

#ifdef JCVM_INT_SUPPORTED
  /**
   * Gets next 4-byte value and increment the PC value.
   */
  jint_t getNextInt() {
    this->require_readable(4);
    jbyte_t msb0 = static_cast<jbyte_t>(this->value[0]);
    jbyte_t lsb0 = static_cast<jbyte_t>(this->value[1]);
    jbyte_t msb1 = static_cast<jbyte_t>(this->value[2]);
    jbyte_t lsb1 = static_cast<jbyte_t>(this->value[3]);
    this->value += 4;
    return signedIntFromBits(BYTES_TO_INT(msb0, lsb0, msb1, lsb1));
  }
#endif /* JCVM_INT_SUPPORTED */

  /**
   * Update the PC value from an relative offset
   *
   * @param [offset] relative offset used to update the PC value
   */
  void updateFromOffset(const int64_t offset) {
    this->setFromOffset(this->value, offset);
  }
};

} // namespace jcvm

#endif /* _PC_T_HPP */
