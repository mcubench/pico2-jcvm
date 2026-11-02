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

#include <cassert> // for check when NDEBUG is undefined.

namespace jcvm {

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
                       const bool arguments_on_caller_stack)
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
{
  jword_t *new_fp, *new_op, *new_tos, *new_eos;

  if (this->frames.empty()) { // the first frame is pushing.
    this->root_return_value_available = false;
    this->root_return_value_is_reference = false;
    this->root_return_value = 0;
    this->root_return_kind = ReturnKind::None;
    this->last_method_return_kind = ReturnKind::None;
    new_fp = &(this->jc_stack[0]);
  } else {

    Frame &previous_frame = this->getCurrentFrame();

    if (arguments_on_caller_stack) {
#ifdef JCRE_STACK_OVERFLOW_PROTECTION
      TRACE_JCVM_DEBUG("PUSH_FRAME nargs=%u available=%d",
                       static_cast<unsigned>(nargs),
                       static_cast<int>(previous_frame.getTOS() -
                                        previous_frame.getOP()));
      if ((previous_frame.getTOS() - nargs) < previous_frame.getOP()) {
        // Stack underflow detected!!!!
        throw Exceptions::StackUnderflowException;
      }
#endif /* JCRE_STACK_OVERFLOW_PROTECTION */
      previous_frame.setTOS(previous_frame.getTOS() - nargs);
    } else if (previous_frame.getTOS() < previous_frame.getOP()) {
      // Native entry may start after its Java operands have been consumed,
      // leaving TOS below the operand-stack base. Place the nested frame at
      // the base so its locals cannot alias the caller's locals.
      previous_frame.setTOS(previous_frame.getOP());
    }
    new_fp = previous_frame.getTOS();
  }

  pc_t new_pc(pc);

#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  if ((new_fp + nargs + max_locals + max_operand_stack) >
      &(this->jc_stack[JCVM_STACK_SIZE - 1])) {
    // Stack overflow detected!!!!
    throw Exceptions::StackOverflowException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  new_op = new_fp + nargs + max_locals;
  new_tos = new_op;
  new_eos = new_tos + max_operand_stack;

  if (nargs > 0) {
    TRACE_JCVM_DEBUG("FRAME arg0=0x%04X arg1=0x%04X arg2=0x%04X arg3=0x%04X",
                     static_cast<unsigned>(new_fp[0]),
                     nargs > 1 ? static_cast<unsigned>(new_fp[1]) : 0,
                     nargs > 2 ? static_cast<unsigned>(new_fp[2]) : 0,
                     nargs > 3 ? static_cast<unsigned>(new_fp[3]) : 0);
  }

  this->frames.emplace_front(new_fp, new_op, new_tos, new_eos, new_pc);

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
  this->root_return_kind = kind;
  this->last_method_return_kind = kind;
}

void Stack::setRootReferenceReturnValue(const jref_t value) noexcept {
  this->root_return_value = value.compact();
  this->root_return_value_available = true;
  this->root_return_value_is_reference = true;
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
  return (jshort_t)current_frame.pop_Value();
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
  return SHORTS_TO_INT(high_word, low_word);
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Popping a reference from the stack
 *
 * @return the top of stack element as a reference value.
 */
jref_t Stack::pop_Reference() {
  return this->pop_ReferenceValue().reference;
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
#if !defined(JCRE_STACK_OVERFLOW_PROTECTION) && !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
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

  return jref_t(*value);
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
  if (this->getValueKind(current_frame.getFP() + local_number) !=
      ValueKind::Primitive) {
    throw Exceptions::SecurityException;
  }
  return (jshort_t)current_frame.readLocal_Value(local_number);
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Reading a local variable as an integer value
 *
 * @return the local variable value at 'local_number' position.
 */
jint_t Stack::readLocal_Int(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  if (this->getValueKind(current_frame.getFP() + local_number) !=
          ValueKind::IntLowWord ||
      this->getValueKind(current_frame.getFP() + local_number + 1) !=
          ValueKind::IntHighWord) {
    throw Exceptions::SecurityException;
  }
  return (jint_t)SHORTS_TO_INT(
      current_frame.readLocal_Value((uint8_t)(local_number + 1)),
      current_frame.readLocal_Value(local_number));
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Reading a local variable as a reference value
 *
 * @return the local variable value at 'local_number' position.
 */
jref_t Stack::readLocal_Reference(const uint8_t local_number) {
  return this->readLocal_ReferenceValue(local_number).reference;
}

Stack::ReferenceValue
Stack::readLocal_ReferenceValue(const uint8_t local_number) {
  Frame &current_frame = this->getCurrentFrame();
  jword_t *local = current_frame.getFP() + local_number;
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
  if (this->getValueKind(current_frame.getFP() + local_number) !=
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
  jword_t *local = current_frame.getFP() + local_number;
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
  jword_t *local = current_frame.getFP() + local_number;
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
  jword_t *local = current_frame.getFP() + local_number;
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
  jword_t *local = current_frame.getFP() + local_number;
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
  jword_t *mnpart, *old_tos;
  Frame &current_frame = this->getCurrentFrame();

#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  // Checking if the operand stack may store enough words to push the result.
  if ((int16_t *)(current_frame.getTOS() + (m + n)) > current_frame.getEOS()) {
    // Stack overflow detected!!!!
    throw Exceptions::StackOverflowException;
  }

  // Checking if the operand stack has enough words to compute the result.
  if ((jword_t *)(current_frame.getTOS() - (m + n)) < current_frame.getOP()) {
    // Stack underflow detected!!!!
    throw Exceptions::StackUnderflowException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  // frame->tos points the next empty word from the stack
  mnpart = (jword_t *)(current_frame.getTOS() - (n + m));
  old_tos = current_frame.getTOS();

  // This loop isn't done when n = 0
  for (; mnpart < old_tos; mnpart++) {
#if false
      // XXX: this implementation is disable because the cost is
      // overweight regarding the checks done by the push_Value function
      current_frame.push_Value(*npart);
#else
    jword_t *destination = current_frame.getTOS();
    *destination = *mnpart;
    this->setValueKind(destination, this->getValueKind(mnpart));
    this->jc_stack_reference_owners[destination - this->jc_stack] =
        this->jc_stack_reference_owners[mnpart - this->jc_stack];
    current_frame.setTOS(current_frame.getTOS() + 1);
#endif
  }
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
