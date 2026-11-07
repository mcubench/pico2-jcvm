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

#include "frame.hpp"

namespace jcvm {
/**
 * Pushing a value on the operand stack
 *
 * @param[value] the byte value to push.
 */
void Frame::push_Value(const jword_t value)
#ifndef JCRE_STACK_OVERFLOW_PROTECTION
    noexcept
#endif
{
#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  if (this->tos >= this->eos) {
    // Stack overflow detected!!!!
    throw Exceptions::StackOverflowException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  *(this->tos) = value;
  this->tos++; // a value is pushed
}

/**
 * Popping a value from the stack
 *
 * @return the top of stack element as a short value.
 */
jword_t Frame::pop_Value()
#ifndef JCRE_STACK_OVERFLOW_PROTECTION
    noexcept
#endif
{
  this->tos--;

#ifdef JCRE_STACK_OVERFLOW_PROTECTION

  if (this->tos < this->op) {
    // Stack underflow detected!!!!
    throw Exceptions::StackUnderflowException;
  }

#endif /* JCRE_STACK_OVERFLOW_PROTECTION */

  return *(this->tos);
}

/**
 * Reading a local variable as a value
 *
 * @return the local variable value at 'local_number' position.
 */
jword_t Frame::readLocal_Value(const uint8_t local_number) {
  const uintptr_t fp_address = reinterpret_cast<uintptr_t>(this->fp);
  const uintptr_t op_address = reinterpret_cast<uintptr_t>(this->op);
  if (op_address < fp_address ||
      (op_address - fp_address) % sizeof(jword_t) != 0 ||
      local_number >= (op_address - fp_address) / sizeof(jword_t)) {
    throw Exceptions::StackOverflowException;
  }
  jword_t *local = this->fp + local_number;

  return *local;
}

/**
 * Writing to local variable
 *
 * @param [value] value to save.
 * @param [local_number] the local variable position.
 */
void Frame::writeLocal_Value(const uint8_t local_number, const int16_t value) {
  const uintptr_t fp_address = reinterpret_cast<uintptr_t>(this->fp);
  const uintptr_t op_address = reinterpret_cast<uintptr_t>(this->op);
  if (op_address < fp_address ||
      (op_address - fp_address) % sizeof(jword_t) != 0 ||
      local_number >= (op_address - fp_address) / sizeof(jword_t)) {
    throw Exceptions::StackOverflowException;
  }
  jword_t *local = this->fp + local_number;

  *(local) = (jword_t)value;
}

/**
 * Return frame base pointer.
 *
 * @return the current frame base pointer.
 */
jword_t *Frame::getFP() const noexcept { return this->fp; }

/**
 * Return operand stack base pointer.
 *
 * @return the current operand stack base pointer.
 */
jword_t *Frame::getOP() const noexcept { return this->op; }

/**
 * Return top of operand stack.
 *
 * @return the current top of operand stack.
 */
jword_t *Frame::getTOS() const noexcept { return this->tos; }

/**
 * Return end of operand stack.
 *
 * @return the current end of operand stack.
 */
jword_t *Frame::getEOS() const noexcept { return this->eos; }

/**
 * Return method program counter.
 *
 * @return the current method program counter.
 */
pc_t &Frame::getPC() noexcept { return this->pc; }

/**
 * Set frame base pointer.
 *
 * @param[fp] the new frame base pointer.
 */
void Frame::setFP(jword_t *fp) noexcept { this->fp = fp; }

/**
 * Set operand stack base pointer.
 *
 * @param[op] the new operand stack base pointer.
 */
void Frame::setOP(jword_t *op) noexcept { this->op = op; }

/**
 * Set top of operand stack pointer.
 *
 * @param[tos] the new top of operand stack pointer.
 */
void Frame::setTOS(jword_t *tos) noexcept { this->tos = tos; }

/**
 * Set end of operand stack pointer.
 *
 * @param[eos] the new end of operand stack pointer.
 */
void Frame::setEOS(jword_t *eos) noexcept { this->eos = eos; }

/**
 * Set method program counter pointer.
 *
 * @param[pc] the new method program counter pointer.
 */
void Frame::setPC(pc_t &pc) noexcept { this->pc = pc; }

/** Return the address of the current opcode. */
const uint8_t *Frame::getInstructionPC() const noexcept {
  return this->instruction_pc;
}

/** Save the opcode address before the interpreter advances the PC. */
void Frame::setInstructionPC(const uint8_t *instruction_pc) noexcept {
  this->instruction_pc = instruction_pc;
}

/** Clear the current operand stack. */
void Frame::clearOperands() noexcept { this->tos = this->op; }

/**
 * Save PC value for jsr instruction.
 *
 * @return the pc index.
 */
uint8_t Frame::savePC() {
  for (uint16_t index = 0; index < this->old_pcs.size(); ++index) {
    old_pc_t &saved = this->old_pcs.at(index);
    if (saved.isUsed) {
      saved.pc.setValue(this->instruction_pc + 3);
      saved.isUsed = false;
      return static_cast<uint8_t>(index);
    }
  }

  if (this->old_pcs.size() >= 256) {
    throw Exceptions::StackOverflowException;
  }
  pc_t saved_pc = this->pc;
  saved_pc.setValue(this->instruction_pc + 3);
  old_pc_t old_pc = {.isUsed = false, .pc = saved_pc};
  this->old_pcs.push_back(old_pc);
  return (this->old_pcs.size() - 1);
}

/**
 * Restore PC value for ret instruction.
 *
 * @param[index] where the PC is located.
 * @return the saved PC.
 */
pc_t &Frame::restorePC(const uint8_t index)
#if !defined(JCVM_FIREWALL_CHECKS) && !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
{
  old_pc_t &old_pc = this->old_pcs.at(index);

#ifdef JCVM_FIREWALL_CHECKS

  if (old_pc.isUsed) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_FIREWALL_CHECKS */

  old_pc.isUsed = true;
  return old_pc.pc;
}

}; // namespace jcvm
