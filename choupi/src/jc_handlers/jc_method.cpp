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

#include "jc_method.hpp"
#include "../debug.hpp"
#include "../heap.hpp"
#include "../stack.hpp"
#include "../../../src/cap_method_bounds.hpp"
#include "../../../src/cap_method_body_bounds.hpp"
#include "../../../src/cap_execution_policy.hpp"
#include <cstdio>

namespace jcvm {

/**
 * Get method from offset.
 *
 * @param[method_offset] method offset to resolve.
 *
 * @return method from method_offset with the type struct jc_cap_method_info or
 * struct jc_cap_extended_method_info
 */
const uint8_t *Method_Handler::getMethodFromOffset(const uint16_t method_offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  auto cap = this->package.getCap();

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cap.getMethod() == nullptr) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  const auto *method_component = cap.getMethod();
  const JCVMArray<const uint8_t> methods = method_component->methods();
  const uint16_t handler_bytes = static_cast<uint16_t>(
      method_component->handler_count *
      sizeof(jc_cap_exception_handler_info));
  TRACE_JCVM_DEBUG("METHOD validate offset=%u handlers=%u handler_bytes=%u",
                   static_cast<unsigned>(method_offset),
                   static_cast<unsigned>(method_component->handler_count),
                   static_cast<unsigned>(handler_bytes));
  if (method_offset <= handler_bytes) {
    throw Exceptions::SecurityException;
  }
  // CAP method offsets are relative to the Method component's data area,
  // which starts with the exception-handler table. methods() starts after
  // that table, so translate the component offset before indexing it.
  const uint16_t method_array_offset =
      static_cast<uint16_t>(method_offset - handler_bytes);

  if (method_offset > 100) {
    TRACE_JCVM_DEBUG("METHOD offset=%u method_bytes=%u",
                     static_cast<unsigned>(method_offset),
                     static_cast<unsigned>(methods.size()));
  }

  // NOTE: The method offset starts from 1. Validate the complete CAP method
  // header before callMethod/getArgumentCount reads its packed fields.
  const uint16_t method_index =
      static_cast<uint16_t>(method_array_offset - 1);
  if (method_index >= methods.size()) {
    throw Exceptions::SecurityException;
  }
  const uint8_t *method = methods.data() + method_index;
  if (!cap_method_bounds::header_fits(
          methods.size(), method_index, methods.at(method_index))) {
    throw Exceptions::SecurityException;
  }
  return method;
}

/**
 * Calling a method.
 *
 * @param[method_to_call] pointer to the method to call.
 * @param[isStaticMethod] is a static method?
 */
void Method_Handler::callMethod(const uint8_t *const method_to_call,
                                const jbool_t isStaticMethod,
                                const bool arguments_on_caller_stack)
#if !defined(JCVM_DYNAMIC_CHECKS_CAP) && !defined(JCVM_FIREWALL_CHECKS)
    noexcept
#endif
{
  uint8_t nargs, max_stack, max_locals;
  const uint8_t *new_pc;

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_ABSTRACT_METHOD(method_to_call)) {
    std::fprintf(stderr, "METHOD:ABSTRACT header=%02X %02X %02X\n",
                 method_to_call[0], method_to_call[1], method_to_call[2]);
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  if (IS_EXTENDED_METHOD(method_to_call)) {
    auto method_to_run =
        reinterpret_cast<const jc_cap_extended_method_info *>(method_to_call);

    max_stack = method_to_run->method_header.max_stack;
    nargs = method_to_run->method_header.nargs;
    max_locals = method_to_run->method_header.max_locals;
    new_pc = method_to_run->bytecodes;
  } else { // normal method header
    auto method_to_run =
        reinterpret_cast<const jc_cap_method_info *>(method_to_call);

    TRACE_JCVM_DEBUG("METHOD header=%02X %02X %02X",
                     method_to_call[0], method_to_call[1], method_to_call[2]);

    max_stack = LOW_NIBBLE(method_to_run->method_header.max_stack);
    nargs = LOW_NIBBLE(method_to_run->method_header.nargs);
    max_locals = LOW_NIBBLE(method_to_run->method_header.max_locals);
    new_pc = method_to_run->bytecodes;
  }

#ifdef JCVM_FIREWALL_CHECKS

  if ((isStaticMethod == FALSE) && (nargs == 0)) {
    std::fprintf(stderr,
                 "METHOD:ZERO_NARGS header=%02X %02X %02X package=%u\n",
                 method_to_call[0], method_to_call[1], method_to_call[2],
                 static_cast<unsigned>(this->package.getPackageID()));
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_FIREWALL_CHECKS */

  const auto *method_component = this->package.getCap().getMethod();
  if (method_component == nullptr) {
    throw Exceptions::SecurityException;
  }
  const auto methods = method_component->methods();
  if (methods.size() == 0 || methods.data() == nullptr) {
    throw Exceptions::SecurityException;
  }
  const uint8_t *method_begin = methods.data();
  const uint8_t *method_end = method_begin + methods.size();
  const uintptr_t method_begin_address =
      reinterpret_cast<uintptr_t>(method_begin);
  const uintptr_t method_end_address =
      reinterpret_cast<uintptr_t>(method_end);
  const uintptr_t new_pc_address = reinterpret_cast<uintptr_t>(new_pc);
  if (new_pc_address < method_begin_address ||
      new_pc_address >= method_end_address) {
    throw Exceptions::SecurityException;
  }

  const std::size_t handler_table_bytes =
      static_cast<std::size_t>(method_component->handler_count) *
      sizeof(jc_cap_exception_handler_info);
  const std::size_t method_stream_offset =
      static_cast<std::size_t>(new_pc_address - method_begin_address);
  const std::size_t component_method_offset =
      handler_table_bytes + 1 + method_stream_offset;
  const auto *descriptor = this->package.getCap().getDescriptor();
  cap_method_body_bounds::Range body{};
  bool have_body_bounds = false;
  if (descriptor != nullptr && component_method_offset <= UINT16_MAX) {
    const std::size_t descriptor_size = NTOHS(descriptor->size);
    have_body_bounds = cap_method_body_bounds::find(
        &descriptor->class_count, descriptor_size, methods.data(),
        methods.size(), handler_table_bytes,
        static_cast<uint16_t>(component_method_offset), body);
  }
  if (!have_body_bounds && cap_execution_policy::is_dynamic_package(
                               this->package.getPackageID())) {
    throw Exceptions::SecurityException;
  }
  if (have_body_bounds) {
    method_begin = methods.data() + body.begin;
    method_end = methods.data() + body.end;
  }

  // pushing the new frame
  this->context.getStack().push_Frame(nargs, max_locals, max_stack, new_pc,
                                      method_begin, method_end,
                                      arguments_on_caller_stack);

  //  and updating executed package ID.

  this->context.changePackageID(this->package.getPackageID());

  return;
}

/**
 * Call a virtual method
 *
 * @param[method_offset] offset in the method component where the method to
 *                       executed is located.
 */
void Method_Handler::callVirtualMethod(
    const uint16_t method_offset, const bool arguments_on_caller_stack) {
  const uint8_t *method_to_call = this->getMethodFromOffset(method_offset);
  this->callMethod(method_to_call, FALSE, arguments_on_caller_stack);
  return;
}

uint8_t Method_Handler::getArgumentCount(const uint16_t method_offset) {
  const uint8_t *method = this->getMethodFromOffset(method_offset);
  if (IS_EXTENDED_METHOD(method)) {
    return reinterpret_cast<const jc_cap_extended_method_info *>(method)
        ->method_header.nargs;
  }
  return LOW_NIBBLE(
      reinterpret_cast<const jc_cap_method_info *>(method)->method_header.nargs);
}

/**
 * Call a static method
 *
 * @param[method_offset] offset in the method component where the method to
 *                       executed is located.
 */
void Method_Handler::callStaticMethod(const uint16_t method_offset) {
  const uint8_t *method_to_call = this->getMethodFromOffset(method_offset);
  this->callMethod(method_to_call, TRUE);
  return;
}

} // namespace jcvm
