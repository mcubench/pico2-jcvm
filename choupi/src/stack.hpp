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

#ifndef _STACK_HPP
#define _STACK_HPP

#include "exceptions.hpp"
#include "frame.hpp"
#include "jc_config.h"
#include "jc_types/jref_t.hpp"
#include "types.hpp"
#include <cstddef>

// TODO: Move the PC to the context.hpp file (in the packagesID list)

namespace jcvm {

/**
 * Checks stack manipulated types size
 */
static_assert(sizeof(jbool_t) == sizeof(jbyte_t),
              "jbool_t class has a wrong size.");
static_assert(sizeof(jbyte_t) == (sizeof(jword_t) / 2),
              "jbyte_t class has a wrong size.");
static_assert(sizeof(jshort_t) == sizeof(jword_t),
              "jshort_t class has a wrong size.");
#ifdef JCVM_INT_SUPPORTED
static_assert(sizeof(jint_t) == (2 * sizeof(jword_t)),
              "jint_t class has a wrong size.");
#endif /* JCVM_INT_SUPPORTED */
static_assert(sizeof(jreturnaddress_t) == sizeof(jword_t),
              "jint_t class has a wrong size.");
static_assert(sizeof(std::declval<jref_t &>().compact()) == sizeof(jword_t),
              "jref_t class has a wrong size.");

class Stack {
public:
  enum class ReturnKind : uint8_t { None, Void, Short, Int, Reference };
  // Package provenance is carried beside a compact reference while values
  // move through operand words and locals. Heap-local handles alone do not
  // identify their owner across Java Card contexts.
  static constexpr uint8_t kCurrentReferenceOwner = 0xFF;
  static constexpr uint8_t kGlobalReferenceOwner = 0xFE;
  struct ReferenceValue {
    jref_t reference;
    uint8_t owner;
  };
  /// Compare opaque reference identity without resolving a handle in a heap.
  static bool referencesEqual(ReferenceValue left, ReferenceValue right,
                              jpackage_ID_t current_package_id) noexcept;
  // Runtime type category for operand words. This compact tag is used by
  // cross-context calls when optional CAP Descriptor metadata is unavailable.
  enum class ValueKind : uint8_t {
    Unknown,
    Primitive,
    Reference,
    IntLowWord,
    IntHighWord,
    ReturnAddress
  };

private:
  static constexpr size_t value_kind_bits = 3;
  static constexpr size_t value_kind_bytes =
      (JCVM_STACK_SIZE * value_kind_bits + 7) / 8;
  jword_t jc_stack[JCVM_STACK_SIZE];
  uint8_t jc_stack_value_kinds[value_kind_bytes]{};
  uint8_t jc_stack_reference_owners[JCVM_STACK_SIZE]{};
  List<Frame> frames;
  bool root_return_value_available = false;
  bool root_return_value_is_reference = false;
  uint32_t root_return_value = 0;
  uint8_t root_reference_return_owner = kCurrentReferenceOwner;
  ReturnKind root_return_kind = ReturnKind::None;
  ReturnKind last_method_return_kind = ReturnKind::None;

  ValueKind getValueKind(const jword_t *word) const noexcept {
    const size_t index = static_cast<size_t>(word - jc_stack);
    const size_t bit_index = index * value_kind_bits;
    const size_t byte_index = bit_index / 8u;
    const uint8_t shift = static_cast<uint8_t>(bit_index & 7u);
    uint16_t packed = jc_stack_value_kinds[byte_index];
    if (byte_index + 1 < value_kind_bytes) {
      packed |= static_cast<uint16_t>(jc_stack_value_kinds[byte_index + 1])
                << 8;
    }
    return static_cast<ValueKind>((packed >> shift) & 0x07u);
  }
  void setValueKind(jword_t *word, const ValueKind kind) noexcept {
    const size_t index = static_cast<size_t>(word - jc_stack);
    const size_t bit_index = index * value_kind_bits;
    const size_t byte_index = bit_index / 8u;
    const uint8_t shift = static_cast<uint8_t>(bit_index & 7u);
    uint16_t packed = jc_stack_value_kinds[byte_index];
    if (byte_index + 1 < value_kind_bytes) {
      packed |= static_cast<uint16_t>(jc_stack_value_kinds[byte_index + 1])
                << 8;
    }
    constexpr uint16_t mask = 0x07u;
    packed = static_cast<uint16_t>(
        (packed & static_cast<uint16_t>(~(mask << shift))) |
        (static_cast<uint16_t>(kind) << shift));
    jc_stack_value_kinds[byte_index] = static_cast<uint8_t>(packed);
    if (byte_index + 1 < value_kind_bytes) {
      jc_stack_value_kinds[byte_index + 1] =
          static_cast<uint8_t>(packed >> 8);
    }
  }

public:
  // Pushing a new frame regarding the invoked method header.
  void push_Frame(const uint8_t nargs, const uint8_t max_locals,
                  const uint8_t max_operand_stack, const uint8_t *pc,
                  const uint8_t *method_begin, const uint8_t *method_end,
                  const bool arguments_on_caller_stack = true);

  // Popping the current frame without return value
  void pop_Frame();
  /// Discard frames and any saved root result after an aborted interpretation.
  void unwind() noexcept {
    this->frames.clear();
    this->root_return_value_available = false;
    this->root_return_value_is_reference = false;
    this->root_return_value = 0;
    this->root_reference_return_owner = kCurrentReferenceOwner;
    this->root_return_kind = ReturnKind::None;
    this->last_method_return_kind = ReturnKind::None;
  }
  /// Discard only frames above an existing interpreter caller.
  void unwindToDepth(const uint16_t depth) noexcept {
    while (this->frames.size() > depth) {
      this->frames.pop_front();
    }
    if (depth == 0) {
      this->root_return_value_available = false;
      this->root_return_value_is_reference = false;
      this->root_return_value = 0;
      this->root_reference_return_owner = kCurrentReferenceOwner;
      this->root_return_kind = ReturnKind::None;
      this->last_method_return_kind = ReturnKind::None;
    }
  }
  uint16_t frameDepth() const noexcept {
    return static_cast<uint16_t>(this->frames.size());
  }
  // The stack is empty? (no pushed Java Card frame)
  bool empty() noexcept;
  /// Whether the current frame is the top-level interpreter entry.
  bool isRootFrame() const noexcept;
  /// Save a typed method result when the top-level entry returns.
  void setRootReturnValue(uint32_t value) noexcept;
  void setRootReturnValue(uint32_t value, ReturnKind kind) noexcept;
  /// Save a reference result so root enumeration preserves its heap handle.
  void setRootReferenceReturnValue(jref_t value) noexcept;
  void setRootReferenceReturnValue(ReferenceValue value) noexcept;
  void recordMethodReturn(ReturnKind kind) noexcept {
    this->last_method_return_kind = kind;
  }
  ReturnKind getRootReturnKind() const noexcept { return root_return_kind; }
  ReturnKind getLastMethodReturnKind() const noexcept {
    return last_method_return_kind;
  }
  bool hasRootReturnValue() const noexcept;
  uint32_t getRootReturnValue() const noexcept;
  bool getRootReferenceReturnValue(jref_t &value) const noexcept;
  bool getRootReferenceReturnValue(ReferenceValue &value) const noexcept;

  // Pushing a byte to the operand stack
  void push_Byte(const jbyte_t value) noexcept(
      noexcept(std::declval<Frame &>().push_Value(value)));
  // Pushing a short to the operand the stack
  void push_Short(const jshort_t value) noexcept(
      noexcept(std::declval<Frame &>().push_Value(value)));
  // Pushing an integer to the operand the stack
#ifdef JCVM_INT_SUPPORTED
  void push_Int(const jint_t value) noexcept(
      noexcept(std::declval<Frame &>().push_Value(value)));
#endif /* JCVM_INT_SUPPORTED */
  // Pushing a reference to the operand the stack
  void push_Reference(
      const jref_t value,
      const uint8_t owner = kCurrentReferenceOwner) noexcept(
      noexcept(std::declval<Frame &>().push_Value(value.compact())));
  // Pushing a return address element to the operand the stack
  void push_ReturnAddress(const jreturnaddress_t value) noexcept(
      noexcept(std::declval<Frame &>().push_Value(value)));

  // Popping to the trash, an untyped element
  void pop() noexcept(noexcept(std::declval<Frame &>().pop_Value()));
  // Popping a byte from the stack
  jbyte_t pop_Byte();
  // Popping a short from the stack
  jshort_t pop_Short();
#ifdef JCVM_INT_SUPPORTED
  // Popping an integer from the stack
  jint_t pop_Int();
#endif /* JCVM_INT_SUPPORTED */
  // Popping a reference from the stack
  jref_t pop_Reference();
  /// Pop a reference only when it is local to or global for this package.
  jref_t pop_ReferenceFromPackage(jpackage_ID_t package_id);
  /// Pop a reference while retaining its package provenance.
  ReferenceValue pop_ReferenceValue();
  // Popping a return address element from the stack
  jreturnaddress_t pop_ReturnAddress();
  // Get a pushing element which as not the last pushed element. This function
  // returns the n-th pushed word without popped it.
  // This accessor can reject a foreign reference with SecurityException even
  // when optional stack bounds checks are disabled.
  jref_t get_Pushed_Element(uint16_t n);
  /// Read a reference operand, rejecting non-reference and foreign-owner words.
  jref_t get_Pushed_Reference(uint16_t n);
  void set_Pushed_Element(
      uint16_t n, jref_t value,
      uint8_t owner = kCurrentReferenceOwner)
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
      noexcept
#endif
      ;

  /// Return the runtime type category for the n-th pushed operand word.
  ValueKind get_Pushed_ValueKind(uint16_t n)
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
      noexcept
#endif
      ;
  /// Return the owner tag for the n-th pushed reference word.
  uint8_t get_Pushed_ReferenceOwner(uint16_t n);
  /// Read a reference operand and its owner without dropping provenance.
  ReferenceValue get_Pushed_ReferenceValue(uint16_t n);

  /// Reading a local variable as a short value
  int16_t readLocal_Short(const uint8_t local_number);
  // /Reading a local variable as an integer value
#ifdef JCVM_INT_SUPPORTED
  int32_t readLocal_Int(const uint8_t local_number);
#endif /* JCVM_INT_SUPPORTED */
  /// Reading a local variable as a reference
  jref_t readLocal_Reference(const uint8_t local_number);
  /// Read a local reference and retain its package provenance.
  ReferenceValue readLocal_ReferenceValue(const uint8_t local_number);
  /// Reading a local variable as a return address element
  jreturnaddress_t readLocal_ReturnAddress(const uint8_t local_number);

  /// Saving a short to a specific local variable
  void writeLocal_Short(const uint8_t local_number, const jshort_t value);
#ifdef JCVM_INT_SUPPORTED
  /// Saving an integer to a specific local variable
  void writeLocal_Int(const uint8_t local_number, const jint_t value);
#endif /* JCVM_INT_SUPPORTED */
  /// Saving a reference to a specific local variable
  void writeLocal_Reference(
      const uint8_t local_number, const jref_t value,
      const uint8_t owner = kCurrentReferenceOwner);
  /// Saving a return address element to a specific local variable
  void writeLocal_ReturnAddress(const uint8_t local_number,
                                const jreturnaddress_t value);

  /// Duplicate top operand stack words and insert below
  void dup(const uint8_t m, const uint8_t n);

  /// Swap top two operand stack words
  void swap(const uint8_t m, const uint8_t n);

  ///  Get the current PC value
  pc_t &getPC();
  /// Save PC value for jsr instruction
  uint8_t savePC();
  /// Restore PC value for ret instruction
  pc_t &restorePC(const uint8_t index);
  /// Get the current frame
  Frame &getCurrentFrame();
};

} // namespace jcvm

#endif /* _STACK_HPP */
