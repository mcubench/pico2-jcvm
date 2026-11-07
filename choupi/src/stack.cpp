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

#include "stack.hpp"

#include "debug.hpp"
#include "jc_utils.hpp"

#include <array>
#include <cassert> // for check when NDEBUG is undefined.
#ifdef PC_VERSION
#include <cstdio>
#include <cstdlib>
#endif

namespace jcvm {

size_t Stack::validatedLocalIndex(const Frame &frame,
                                  const uint8_t local_number,
                                  const size_t width) const {
  const uintptr_t stack_begin = reinterpret_cast<uintptr_t>(this->jc_stack);
  const uintptr_t stack_end =
      stack_begin + sizeof(this->jc_stack);
  const uintptr_t frame_begin = reinterpret_cast<uintptr_t>(frame.getFP());
  const uintptr_t locals_end = reinterpret_cast<uintptr_t>(frame.getOP());
  if (frame_begin < stack_begin || frame_begin >= stack_end ||
      locals_end < frame_begin || locals_end > stack_end ||
      (frame_begin - stack_begin) % sizeof(jword_t) != 0 ||
      (locals_end - stack_begin) % sizeof(jword_t) != 0) {
    throw Exceptions::StackOverflowException;
  }

  const size_t frame_index =
      static_cast<size_t>((frame_begin - stack_begin) / sizeof(jword_t));
  const size_t local_count =
      static_cast<size_t>((locals_end - frame_begin) / sizeof(jword_t));
  const size_t local_offset = local_number;
  if (local_offset > local_count || width > local_count - local_offset ||
      frame_index > JCVM_STACK_SIZE ||
      local_offset > JCVM_STACK_SIZE - frame_index ||
      width > JCVM_STACK_SIZE - frame_index - local_offset) {
    throw Exceptions::StackOverflowException;
  }
  return frame_index + local_offset;
}

/**
 * Pushing a new frame regarding the invoked method header.
 *
 * @param[nargs] number of Java function parameters.
 * @param[max_locals] number of Java function locals elements without
 * parameters.
 * @param[max_operand_stack] number of max. operand stack element required
 *                           to execute this function.
 * @return void.
 */
void Stack::push_Frame(const uint8_t nargs, const uint8_t max_locals,
                       const uint8_t max_operand_stack, const uint8_t *pc,
                       const uint8_t *method_begin,
                       const uint8_t *method_end,
                       const bool arguments_on_caller_stack) {
  jword_t *new_fp, *new_op, *new_tos, *new_eos;
  Frame *previous_frame = nullptr;
  size_t new_fp_index = 0;
  bool adjust_previous_tos = false;

  if (!this->frames.empty()) {
    previous_frame = &this->getCurrentFrame();
    const uintptr_t stack_begin = reinterpret_cast<uintptr_t>(this->jc_stack);
    const uintptr_t stack_end = stack_begin + sizeof(this->jc_stack);
    const auto checked_stack_index = [&](jword_t *pointer) -> size_t {
      const uintptr_t address = reinterpret_cast<uintptr_t>(pointer);
      if (address < stack_begin || address > stack_end ||
          (address - stack_begin) % sizeof(jword_t) != 0) {
        throw Exceptions::StackOverflowException;
      }
      return static_cast<size_t>((address - stack_begin) / sizeof(jword_t));
    };
    const size_t op_index = checked_stack_index(previous_frame->getOP());
    const size_t tos_index = checked_stack_index(previous_frame->getTOS());

    if (arguments_on_caller_stack) {
      if (tos_index < op_index || nargs > tos_index - op_index) {
        throw Exceptions::StackUnderflowException;
      }
      new_fp_index = tos_index - nargs;
      adjust_previous_tos = true;
    } else if (tos_index < op_index) {
      // Native entry may start after its Java operands were consumed.
      new_fp_index = op_index;
      adjust_previous_tos = true;
    } else {
      new_fp_index = tos_index;
    }
  }

  const size_t frame_words = static_cast<size_t>(nargs) + max_locals +
                             max_operand_stack;
  if (new_fp_index > JCVM_STACK_SIZE ||
      frame_words > JCVM_STACK_SIZE - new_fp_index) {
    throw Exceptions::StackOverflowException;
  }

  new_fp = this->jc_stack + new_fp_index;
  new_op = new_fp + nargs + max_locals;
  new_tos = new_op;
  new_eos = new_tos + max_operand_stack;

  pc_t new_pc(pc, method_begin, method_end);

  if (nargs > 0) {
    TRACE_JCVM_DEBUG("FRAME arg0=0x%04X arg1=0x%04X arg2=0x%04X arg3=0x%04X",
                     static_cast<unsigned>(new_fp[0]),
                     nargs > 1 ? static_cast<unsigned>(new_fp[1]) : 0,
                     nargs > 2 ? static_cast<unsigned>(new_fp[2]) : 0,
                     nargs > 3 ? static_cast<unsigned>(new_fp[3]) : 0);
  }

  this->frames.emplace_front(new_fp, new_op, new_tos, new_eos, new_pc);

  if (previous_frame != nullptr && adjust_previous_tos) {
    previous_frame->setTOS(this->jc_stack + new_fp_index);
  } else if (previous_frame == nullptr) { // the first frame is pushing.
    this->root_return_value_available = false;
    this->root_return_value_is_reference = false;
    this->root_return_value = 0;
    this->root_reference_return_owner = kCurrentReferenceOwner;
    this->root_return_kind = ReturnKind::None;
    this->last_method_return_kind = ReturnKind::None;
  }

  // cleaning the local variables area
  for (auto word = (new_fp + nargs); word < new_op; word++) {
    *word = 0;
    this->setValueKind(word, ValueKind::Unknown);
    this->jc_stack_reference_owners[word - this->jc_stack] =
        kCurrentReferenceOwner;
  }
}

/**
 * Popping the current frame.
 */
void Stack::pop_Frame() {
  if (this->frames.empty()) {
    throw Exceptions::SecurityException;
  }

  this->frames.pop_front();
  return;
}

/**
 * The stack is empty? (no pushed Java Card frame)
 */
bool Stack::empty() noexcept { return this->frames.empty(); }

bool Stack::isRootFrame() const noexcept { return this->frames.size() == 1; }

void Stack::setRootReturnValue(const uint32_t value) noexcept {
  this->setRootReturnValue(value, ReturnKind::Short);
}

void Stack::setRootReturnValue(const uint32_t value,
                               const ReturnKind kind) noexcept {
  this->root_return_value = value;
  this->root_return_value_available = true;
  this->root_return_value_is_reference = false;
  this->root_reference_return_owner = kCurrentReferenceOwner;
  this->root_return_kind = kind;
  this->last_method_return_kind = kind;
}

void Stack::setRootReferenceReturnValue(const jref_t value) noexcept {
  this->setRootReferenceReturnValue(
      ReferenceValue{value, kCurrentReferenceOwner});
}

void Stack::setRootReferenceReturnValue(const ReferenceValue value) noexcept {
  this->root_return_value = value.reference.compact();
  this->root_return_value_available = true;
  this->root_return_value_is_reference = true;
  this->root_reference_return_owner = value.owner;
  this->root_return_kind = ReturnKind::Reference;
  this->last_method_return_kind = ReturnKind::Reference;
}

bool Stack::hasRootReturnValue() const noexcept {
  return this->root_return_value_available;
}

uint32_t Stack::getRootReturnValue() const noexcept {
  return this->root_return_value;
}

bool Stack::getRootReferenceReturnValue(jref_t &value) const noexcept {
  if (!this->root_return_value_available ||
      !this->root_return_value_is_reference) {
    return false;
  }
  value = jref_t(static_cast<uint16_t>(this->root_return_value));
  return true;
}

bool Stack::getRootReferenceReturnValue(ReferenceValue &value) const noexcept {
  if (!this->root_return_value_available ||
      !this->root_return_value_is_reference) {
    return false;
  }
  value = {jref_t(static_cast<uint16_t>(this->root_return_value)),
           this->root_reference_return_owner};
  return true;
}

/**
 * Pushing a byte to the operand stack
 *
 * @param[value] the byte value to push.
 */
void Stack::push_Byte(const jbyte_t value) noexcept(
    noexcept(std::declval<Frame &>().push_Value(value))) {
  // The Java-language, as the C-language, propagates the sign when a cast
  // arrives. For example, the value 0xFF in byte is casted to the value
  // 0xFFFF in short, and so on.
  Frame &frame = this->getCurrentFrame();
  jword_t *slot = frame.getTOS();
  frame.push_Value((jword_t)value);
  this->setValueKind(slot, ValueKind::Primitive);
}

/**
 * Pushing a short to the operand the stack
 *
 * @param[value] the short value to push.
 */
void Stack::push_Short(const jshort_t value) noexcept(
    noexcept(std::declval<Frame &>().push_Value(value))) {
  Frame &frame = this->getCurrentFrame();
  jword_t *slot = frame.getTOS();
  frame.push_Value((jword_t)value);
  this->setValueKind(slot, ValueKind::Primitive);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Pushing an integer to the operand the stack
 *
 * @param[value] the integer value to push.
 */
void Stack::push_Int(const jint_t value) noexcept(
    noexcept(std::declval<Frame &>().push_Value(value))) {
  Frame &current_frame = this->getCurrentFrame();
  jword_t *low_word = current_frame.getTOS();
  current_frame.push_Value((jword_t)INT_2_LSSHORTS(value));
  this->setValueKind(low_word, ValueKind::IntLowWord);
  jword_t *high_word = current_frame.getTOS();
  current_frame.push_Value((jword_t)INT_2_MSSHORTS(value));
  this->setValueKind(high_word, ValueKind::IntHighWord);
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Pushing a reference to the operand the stack
 *
 * @param[value] the reference value to push.
 */
void Stack::push_Reference(const jref_t value, const uint8_t owner) noexcept(
    noexcept(std::declval<Frame &>().push_Value(value.compact()))) {
  Frame &current_frame = this->getCurrentFrame();
  jword_t *slot = current_frame.getTOS();
  current_frame.push_Value(value.compact());
  this->setValueKind(slot, ValueKind::Reference);
  const uint8_t effective_owner =
      owner == kCurrentReferenceOwner &&
              (value.isGlobalArray() || value.isGlobalInstance() ||
               value.isNullPointer())
          ? kGlobalReferenceOwner
          : owner;
  this->jc_stack_reference_owners[slot - this->jc_stack] = effective_owner;
}

/**
 * Pushing a return address element to the operand the stack
 *
 * @param [value] the reference value to push.
 */
void Stack::push_ReturnAddress(const jreturnaddress_t value) noexcept(
    noexcept(std::declval<Frame &>().push_Value(value))) {
  Frame &current_frame = this->getCurrentFrame();
  jword_t *slot = current_frame.getTOS();
  current_frame.push_Value(value);
  this->setValueKind(slot, ValueKind::ReturnAddress);
}

/**
 * Popping to the trash, an untyped element.
 */
void Stack::pop() noexcept(noexcept(std::declval<Frame &>().pop_Value())) {
  Frame &current_frame = this->getCurrentFrame();
  current_frame.pop_Value(); // The return value is ignored.
}

/**
 * Popping a byte from the stack
 *
 * @return the top of stack element as a short value.
 */
jbyte_t Stack::pop_Byte() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  if (this->getValueKind(current_frame.getTOS() - 1) != ValueKind::Primitive) {
    throw Exceptions::SecurityException;
  }
  return (jbyte_t)current_frame.pop_Value();
}

/**
 * Popping a short from the stack
 *
 * @return the top of stack element as a short value.
 */
jshort_t Stack::pop_Short() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  if (this->getValueKind(current_frame.getTOS() - 1) != ValueKind::Primitive) {
    throw Exceptions::SecurityException;
  }
  return signedShortFromBits(current_frame.pop_Value());
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Popping an integer from the stack
 *
 * @return the top of stack element as an integer value.
 */
jint_t Stack::pop_Int() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() - current_frame.getOP() < 2) {
    throw Exceptions::StackUnderflowException;
  }
  if (this->getValueKind(current_frame.getTOS() - 1) !=
          ValueKind::IntHighWord ||
      this->getValueKind(current_frame.getTOS() - 2) !=
          ValueKind::IntLowWord) {
    throw Exceptions::SecurityException;
  }
  const jword_t high_word = current_frame.pop_Value();
  const jword_t low_word = current_frame.pop_Value();
  return signedIntFromBits(SHORTS_TO_INT(high_word, low_word));
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Popping a reference from the stack
 *
 * @return the top of stack element as a reference value.
 */
jref_t Stack::pop_Reference() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  jword_t *slot = current_frame.getTOS() - 1;
  if (this->getValueKind(slot) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  const uint8_t owner =
      this->jc_stack_reference_owners[slot - this->jc_stack];
  if (owner != kCurrentReferenceOwner && owner != kGlobalReferenceOwner) {
    throw Exceptions::SecurityException;
  }
  return jref_t(current_frame.pop_Value());
}

jref_t Stack::pop_ReferenceFromPackage(const jpackage_ID_t package_id) {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  jword_t *slot = current_frame.getTOS() - 1;
  if (this->getValueKind(slot) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  const uint8_t owner =
      this->jc_stack_reference_owners[slot - this->jc_stack];
  if (owner != kCurrentReferenceOwner && owner != kGlobalReferenceOwner &&
      owner != package_id) {
    throw Exceptions::SecurityException;
  }
  return jref_t(current_frame.pop_Value());
}

Stack::ReferenceValue Stack::pop_ReferenceValue() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  if (this->getValueKind(current_frame.getTOS() - 1) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  jword_t *slot = current_frame.getTOS() - 1;
  const uint8_t owner =
      this->jc_stack_reference_owners[slot - this->jc_stack];
  return {jref_t(current_frame.pop_Value()), owner};
}

bool Stack::referencesEqual(const ReferenceValue left,
                            const ReferenceValue right,
                            const jpackage_ID_t current_package_id) noexcept {
  const auto normalized_owner = [current_package_id](const ReferenceValue value) {
    if (value.reference.isNullPointer() || value.reference.isGlobalArray() ||
        value.reference.isGlobalInstance()) {
      return kGlobalReferenceOwner;
    }
    return value.owner == kCurrentReferenceOwner ? current_package_id
                                                  : value.owner;
  };
  const uint8_t left_owner = normalized_owner(left);
  const uint8_t right_owner = normalized_owner(right);
  const bool same_handle = left.reference == right.reference;
  if (same_handle && left_owner != right_owner) {
#ifdef PC_VERSION
    if (std::getenv("CHOUPI_TRACE_REFERENCE_COLLISIONS") != nullptr) {
      std::fprintf(stderr,
                   "JCVM:REFERENCE_HANDLE_COLLISION handle=%04X "
                   "left_owner=%u right_owner=%u current_package=%u\n",
                   left.reference.compact(), static_cast<unsigned>(left_owner),
                   static_cast<unsigned>(right_owner),
                   static_cast<unsigned>(current_package_id));
    }
#endif
  }
  return same_handle && left_owner == right_owner;
}

/**
 * Popping a return address element from the stack
 *
 * @return the top of stack element as a return address value.
 */
jreturnaddress_t Stack::pop_ReturnAddress() {
  Frame &current_frame = this->getCurrentFrame();
  if (current_frame.getTOS() <= current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
  if (this->getValueKind(current_frame.getTOS() - 1) !=
      ValueKind::ReturnAddress) {
    throw Exceptions::SecurityException;
  }
  return (jreturnaddress_t)current_frame.pop_Value();
}

/*
 * Get a pushing element which as not the last pushed element. This function
 * returns the n-th pushed word without popped it.
 *
 * @param[n] the n-th pushed element to return.
 * @return the n-th pushed element to return.
 */
jref_t Stack::get_Pushed_Element(uint16_t n)
{
  assert(n > 0);

  Frame &current_frame = this->getCurrentFrame();
  jword_t *value = current_frame.getTOS() - n;

#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  if (value < current_frame.getOP()) {
    // Stack underflow detected!!!!
    throw Exceptions::StackUnderflowException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  if (this->getValueKind(value) == ValueKind::Reference) {
    const uint8_t owner =
        this->jc_stack_reference_owners[value - this->jc_stack];
    if (owner != kCurrentReferenceOwner && owner != kGlobalReferenceOwner) {
      throw Exceptions::SecurityException;
    }
  }

  return jref_t(*value);
}

jref_t Stack::get_Pushed_Reference(const uint16_t n) {
  const ReferenceValue value = this->get_Pushed_ReferenceValue(n);
  if (value.owner != kCurrentReferenceOwner &&
      value.owner != kGlobalReferenceOwner) {
    throw Exceptions::SecurityException;
  }
  return value.reference;
}

void Stack::set_Pushed_Element(const uint16_t n, const jref_t value,
                               const uint8_t owner)
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
{
  assert(n > 0);

  Frame &current_frame = this->getCurrentFrame();
  jword_t *word = current_frame.getTOS() - n;

#ifdef JCRE_STACK_OVERFLOW_PROTECTION
  if (word < current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  *word = value.compact();
  this->setValueKind(word, ValueKind::Reference);
  const uint8_t effective_owner =
      owner == kCurrentReferenceOwner &&
              (value.isGlobalArray() || value.isGlobalInstance() ||
               value.isNullPointer())
          ? kGlobalReferenceOwner
          : owner;
  this->jc_stack_reference_owners[word - this->jc_stack] = effective_owner;
}

uint8_t Stack::get_Pushed_ReferenceOwner(const uint16_t n) {
  assert(n > 0);
  Frame &current_frame = this->getCurrentFrame();
  jword_t *word = current_frame.getTOS() - n;
#ifdef JCRE_STACK_OVERFLOW_PROTECTION
  if (word < current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
#endif
  if (this->getValueKind(word) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  return this->jc_stack_reference_owners[word - this->jc_stack];
}

Stack::ReferenceValue Stack::get_Pushed_ReferenceValue(const uint16_t n) {
  assert(n > 0);
  Frame &current_frame = this->getCurrentFrame();
  jword_t *word = current_frame.getTOS() - n;
#ifdef JCRE_STACK_OVERFLOW_PROTECTION
  if (word < current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
#endif
  if (this->getValueKind(word) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  return {jref_t(*word),
          this->jc_stack_reference_owners[word - this->jc_stack]};
}

Stack::ValueKind Stack::get_Pushed_ValueKind(const uint16_t n)
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
{
  assert(n > 0);
  Frame &current_frame = this->getCurrentFrame();
  jword_t *word = current_frame.getTOS() - n;
#ifdef JCRE_STACK_OVERFLOW_PROTECTION
  if (word < current_frame.getOP()) {
    throw Exceptions::StackUnderflowException;
  }
#endif
  return this->getValueKind(word);
}

/**
 * Reading a local variable as a short value
 *
 * @return the local variable value at 'local_number' position.
 */
jshort_t Stack::readLocal_Short(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  if (this->getValueKind(this->jc_stack + local_index) !=
      ValueKind::Primitive) {
    throw Exceptions::SecurityException;
  }
  return signedShortFromBits(current_frame.readLocal_Value(local_number));
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Reading a local variable as an integer value
 *
 * @return the local variable value at 'local_number' position.
 */
jint_t Stack::readLocal_Int(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 2);
  if (this->getValueKind(this->jc_stack + local_index) !=
          ValueKind::IntLowWord ||
      this->getValueKind(this->jc_stack + local_index + 1) !=
          ValueKind::IntHighWord) {
    throw Exceptions::SecurityException;
  }
  return signedIntFromBits(SHORTS_TO_INT(
      current_frame.readLocal_Value((uint8_t)(local_number + 1)),
      current_frame.readLocal_Value(local_number)));
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Reading a local variable as a reference value
 *
 * @return the local variable value at 'local_number' position.
 */
jref_t Stack::readLocal_Reference(const uint8_t local_number) {
  const ReferenceValue value = this->readLocal_ReferenceValue(local_number);
  if (value.owner != kCurrentReferenceOwner &&
      value.owner != kGlobalReferenceOwner) {
    throw Exceptions::SecurityException;
  }
  return value.reference;
}

Stack::ReferenceValue
Stack::readLocal_ReferenceValue(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  jword_t *local = this->jc_stack + local_index;
  if (this->getValueKind(local) != ValueKind::Reference) {
    throw Exceptions::SecurityException;
  }
  return {jref_t(current_frame.readLocal_Value(local_number)),
          this->jc_stack_reference_owners[local - this->jc_stack]};
}

/**
 * Reading a local variable as a return address value
 *
 * @return the local variable value at 'local_number' position.
 */
jreturnaddress_t Stack::readLocal_ReturnAddress(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  if (this->getValueKind(this->jc_stack + local_index) !=
      ValueKind::ReturnAddress) {
    throw Exceptions::SecurityException;
  }
  return (jreturnaddress_t)current_frame.readLocal_Value(local_number);
}

/**
 * Writing a local variable as a short value
 *
 * @param[value] value to save.
 * @param[local_number] the local variable position.
 */
void Stack::writeLocal_Short(const uint8_t local_number, const jshort_t value) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  jword_t *local = this->jc_stack + local_index;
  current_frame.writeLocal_Value(local_number, value);
  this->setValueKind(local, ValueKind::Primitive);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Writing a local variable as an integer value
 *
 * @param[value] value to save.
 * @param[local_number] the local variable position.
 */
void Stack::writeLocal_Int(const uint8_t local_number, const jint_t value) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 2);
  jword_t *local = this->jc_stack + local_index;
  current_frame.writeLocal_Value(local_number, INT_2_LSSHORTS(value));
  current_frame.writeLocal_Value((uint8_t)(local_number + 1),
                                 INT_2_MSSHORTS(value));
  this->setValueKind(local, ValueKind::IntLowWord);
  this->setValueKind(local + 1, ValueKind::IntHighWord);
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Writing a local variable as a reference
 *
 * @param[value] value to save.
 * @param[local_number] the local variable position.
 */
void Stack::writeLocal_Reference(const uint8_t local_number,
                                 const jref_t value, const uint8_t owner) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  jword_t *local = this->jc_stack + local_index;
  current_frame.writeLocal_Value(local_number, value.compact());
  this->setValueKind(local, ValueKind::Reference);
  const uint8_t effective_owner =
      owner == kCurrentReferenceOwner &&
              (value.isGlobalArray() || value.isGlobalInstance() ||
               value.isNullPointer())
          ? kGlobalReferenceOwner
          : owner;
  this->jc_stack_reference_owners[local - this->jc_stack] = effective_owner;
}

/**
 * Writing a return address variable as a reference
 *
 * @param[value] value to save.
 * @param[local_number] the local variable position.
 */
void Stack::writeLocal_ReturnAddress(const uint8_t local_number,
                                     const jreturnaddress_t value) {
  Frame &current_frame = this->getCurrentFrame();
  const size_t local_index =
      this->validatedLocalIndex(current_frame, local_number, 1);
  jword_t *local = this->jc_stack + local_index;
  current_frame.writeLocal_Value(local_number, value);
  this->setValueKind(local, ValueKind::ReturnAddress);
}

/**
 * Duplicate top operand stack words and insert below
 *
 * Stack:
 *   ..., wordN, ..., wordM, ..., word1 ->
 *                  ..., wordM, ..., word1, wordN, ..., wordM, ..., word1
 *
 * Description:
 *
 *   For positive values of n, the top m words on the operand stack are
 *   duplicated and the copied words are inserted n words down in the
 *   operand stack. When n equals 0, the top m words are copied and placed
 *   on top of the stack.
 *
 *   The dup_x instruction must not be used unless the ranges of words 1
 *   through m and words m+1 through n each contain either a 16-bit data
 *   type, two 16-bit data types, a 32-bit data type, a 16-bit data type and
 *   a 32-bit data type (in either order), or two 32-bit data types.
 *
 * Notes:
 *
 *   Except for restrictions preserving the integrity of 32-bit data types,
 *   the dup instruction operates on untyped words, ignoring the types of
 *   data they contain.
 *
 * (source: A part of the Java Card Virtual Machine 3.0.5 Specification,
 *  dup_x instruction)
 */
void Stack::dup(const uint8_t m, const uint8_t n) {
  Frame &current_frame = this->getCurrentFrame();
#ifdef JCVM_INT_SUPPORTED
  constexpr uint8_t maximum_m = 4;
  constexpr uint8_t maximum_extra_n = 4;
#else
  constexpr uint8_t maximum_m = 2;
  constexpr uint8_t maximum_extra_n = 2;
#endif
  if (m == 0 || m > maximum_m ||
      (n != 0 && (n < m || n > static_cast<uint8_t>(m + maximum_extra_n)))) {
    throw Exceptions::RuntimeException;
  }

  jword_t *const old_tos = current_frame.getTOS();
  const std::ptrdiff_t available = old_tos - current_frame.getOP();
  const std::ptrdiff_t free = current_frame.getEOS() - old_tos;
  const std::ptrdiff_t required = n == 0 ? m : n;
  if (available < required) {
    throw Exceptions::StackUnderflowException;
  }
  if (free < m) {
    throw Exceptions::StackOverflowException;
  }

  struct WordCopy {
    jword_t value;
    ValueKind kind;
    uint8_t reference_owner;
  };
  std::array<WordCopy, maximum_m> copied{};
  for (uint8_t index = 0; index < m; ++index) {
    jword_t *const source = old_tos - m + index;
    copied[index] = {
        *source,
        this->getValueKind(source),
        this->jc_stack_reference_owners[source - this->jc_stack]};
  }

  if (n != 0) {
    // Shift the n existing words up by m slots from top to bottom. This
    // preserves their order and opens the insertion range.
    for (uint8_t depth = 1; depth <= n; ++depth) {
      jword_t *const source = old_tos - depth;
      jword_t *const destination = source + m;
      *destination = *source;
      this->setValueKind(destination, this->getValueKind(source));
      this->jc_stack_reference_owners[destination - this->jc_stack] =
          this->jc_stack_reference_owners[source - this->jc_stack];
    }
  }

  jword_t *const insertion = old_tos - n;
  for (uint8_t index = 0; index < m; ++index) {
    jword_t *const destination = insertion + index;
    *destination = copied[index].value;
    this->setValueKind(destination, copied[index].kind);
    this->jc_stack_reference_owners[destination - this->jc_stack] =
        copied[index].reference_owner;
  }
  current_frame.setTOS(old_tos + m);
}

/**
 * Swap top two operand stack words
 *
 * Stack:
 *   ..., wordM+N, ..., wordM+1, wordM, ..., word1 ->
 *                     ..., wordM, ..., word1, wordM+N, ..., wordM+1
 *
 * Description:
 *
 *  Permissible values for both m and n are 1 and 2. (The values of n and m
 *  are checked in the bc_stack.c file).
 *
 *  The top m words on the operand stack are swapped with the n words
 *  immediately below.
 *
 *  The swap_x instruction must not be used unless the ranges of words 1
 *  through m and words m+1 through n each contain either a 16-bit data
 *  type, two 16-bit data types, a 32-bit data type, a 16-bit data type and
 *  a 32-bit data type (in either order), or two 32-bit data types.
 *
 * Notes:
 *
 *   Except for restrictions preserving the integrity of 32-bit data types,
 *   the swap instruction operates on untyped words, ignoring the types of
 *   data they contain.
 *
 * (source: A part of the Java Card Virtual Machine 3.0.5 Specification,
 *  swap_x instruction)
 */
void Stack::swap(const uint8_t m, const uint8_t n) {
  jword_t tmp[4];
  ValueKind tmp_kinds[4];
  uint8_t tmp_reference_owners[4];
  jword_t *bar;
  uint8_t foo, tmp_index;
  Frame &current_frame = this->getCurrentFrame();

#ifdef JCVM_INT_SUPPORTED

  /*
   * Permissible values for both m and n are 1 and 2.
   */
  if ((m == 0) || (m > 2) || (n == 0) || (n > 2))
#else  /* !JCVM_INT_SUPPORTED */

  /*
   * If a virtual machine does not support the int data type, the only
   * permissible value for both m and n is 1.
   */
  if ((m != 1) || (n != 1))
#endif /* JCVM_INT_SUPPORTED */
  {
    throw Exceptions::RuntimeException;
  }

#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  // Checking if the operand stack has enough words to swap.
  if ((jword_t *)(current_frame.getTOS() - (m + n)) < current_frame.getOP()) {
    // Stack overflow detected!!!!
    throw Exceptions::SecurityException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  tmp_index = 0;

  // copy m word(s) from the stack to tmp array.
  for (foo = m; foo > 0; foo--) {
    jword_t *word = current_frame.getTOS() - foo;
    tmp[tmp_index] = *word;
    tmp_kinds[tmp_index] = this->getValueKind(word);
    tmp_reference_owners[tmp_index] =
        this->jc_stack_reference_owners[word - this->jc_stack];
    tmp_index++;
  }

  // copy n word(s) from the stack to tmp array.
  for (foo = (uint8_t)(n + m); foo > m; foo--) {
    jword_t *word = current_frame.getTOS() - foo;
    tmp[tmp_index] = *word;
    tmp_kinds[tmp_index] = this->getValueKind(word);
    tmp_reference_owners[tmp_index] =
        this->jc_stack_reference_owners[word - this->jc_stack];
    tmp_index++;
  }

  bar = (int16_t *)(current_frame.getTOS() - (n + m));

  for (foo = 0; foo < n + m; foo++) {
    *bar = tmp[foo];
    this->setValueKind(bar, tmp_kinds[foo]);
    this->jc_stack_reference_owners[bar - this->jc_stack] =
        tmp_reference_owners[foo];
    bar++;
  }

  return;
}

/**
 * Get the current PC value
 *
 * @return current PC value
 */
pc_t &Stack::getPC() {
  Frame &current_frame = this->getCurrentFrame();
  return current_frame.getPC();
}

/**
 * Save PC value for jsr instruction.
 *
 * @return the saved pc index.
 */
uint8_t Stack::savePC() {
  Frame &current_frame = this->getCurrentFrame();
  return current_frame.savePC();
}

pc_t &Stack::restorePC(const uint8_t index) {
  Frame &current_frame = this->getCurrentFrame();
  return current_frame.restorePC(index);
}

/**
 * Get the current frame.
 *
 * @return the current frame.
 */
Frame &Stack::getCurrentFrame() { return *(this->frames.begin()); }

} // namespace jcvm
