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

#include "../debug.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#include "../jc_handlers/jc_export.hpp"
#include "../jc_types/jc_instance.hpp"
#include "../stack.hpp"
#include "bytecodes.hpp"
#include <cstring>

namespace jcvm {

/**
 * Throw exception or error
 *
 * Format:
 *   athrow
 *
 * Forms:
 *   athrow = 147 (0x93)
 *
 * Stack:
 *   ..., objectref -> objectref
 *
 * Description:
 *
 *   The objectref must be of type reference and must refer to an object
 *   that is an instance of class Throwable or of a subclass of Throwable.
 *   It is popped from the operand stack. The objectref is then thrown by
 *   searching the current frame (§3.5 Frames) for the most recent catch
 *   clause that catches the class of objectref or one of its superclasses.
 *
 *   If a catch clause is found, it contains the location of the code
 *   intended to handle this exception. The pc register is reset to that
 *   location, the operand stack of the current frame is cleared, objectref
 *   is pushed back onto the operand stack, and execution continues. If no
 *   appropriate clause is found in the current frame, that frame is popped,
 *   the frame of its invoker is reinstated, and the objectref is rethrown.
 *
 *   If no catch clause is found that handles this exception, the virtual
 *   machine exits.
 *
 * Runtime Exception:
 *
 *   If objectref is null, athrow throws a NullPointerException instead of
 *   objectref.
 *
 * Notes:
 *
 *   In some circumstances, the athrow instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_athrow() {
  jref_t objectref;
  Stack &stack = this->context.getStack();

  TRACE_JCVM_DEBUG("ATHROW");

  objectref = stack.pop_Reference();
  this->doThrow(objectref);

  return;
}

/**
 * Throwing an exception
 *
 */
void Bytecodes::doThrow(jref_t objectref) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  if (objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  auto exception = heap.getInstance(objectref);
  Package exception_package(exception->getPackageID());
  ConstantPool_Handler exception_cp(exception_package);
  auto exception_class = exception_cp.resolveClassref(
      exception_cp.getClassRefFromClassIndex(exception->getClassIndex()));
  bool checking_caller_frame = false;

  while (!stack.empty()) {
    Package package = this->context.getCurrentPackage();
    const auto method_component = package.getCap().getMethod();
    const auto methods = method_component->methods();
    const std::uintptr_t method_offset =
        reinterpret_cast<std::uintptr_t>(methods.data());
    const std::uintptr_t throw_pc = reinterpret_cast<std::uintptr_t>(
        stack.getCurrentFrame().getInstructionPC());

    if (throw_pc < method_offset || throw_pc - method_offset >= methods.size()) {
      throw Exceptions::SecurityException;
    }

    const uint32_t opcode_offset =
        static_cast<uint32_t>(throw_pc - method_offset);
    const auto handlers = method_component->exception_handlers();
    const uint32_t handler_table_bytes = static_cast<uint32_t>(
        handlers.size() * sizeof(jc_cap_exception_handler_info));

    for (uint16_t index = 0; index < handlers.size(); ++index) {
      const auto *record = reinterpret_cast<const uint8_t *>(&handlers[index]);
      const uint16_t start = BYTES_TO_SHORT(record[0], record[1]);
      const uint16_t range_flags = BYTES_TO_SHORT(record[2], record[3]);
      const uint16_t active_length = range_flags & 0x7FFF;
      const uint16_t handler_component_offset =
          BYTES_TO_SHORT(record[4], record[5]);
      const uint16_t catch_type_index = BYTES_TO_SHORT(record[6], record[7]);
      if (start < handler_table_bytes ||
          handler_component_offset <= handler_table_bytes) {
        throw Exceptions::SecurityException;
      }
      const uint32_t start_method_offset = start - handler_table_bytes;
      const uint32_t handler_method_offset =
          handler_component_offset - handler_table_bytes - 1;
      // The current frame stores the address of its throwing instruction.
      // An unwound caller stores the invoke opcode, while the CAP handler
      // range begins at the caller's next bytecode boundary.
      const uint32_t exception_range_pc =
          opcode_offset + (checking_caller_frame ? 1u : 0u);
      const bool handler_active =
          exception_range_pc >= start_method_offset &&
          exception_range_pc < start_method_offset + active_length;
      if (!handler_active) {
        continue;
      }

      bool matches = catch_type_index == 0;
      if (!matches) {
        ConstantPool_Handler catch_cp(package);
        const auto catch_class = catch_cp.resolveClassref(
            catch_cp.getClassRef(catch_type_index));
        matches = Class_Handler::docheckcast(exception_class, catch_class);
      }

      if (matches) {
        if (handler_method_offset >= methods.size()) {
          throw Exceptions::SecurityException;
        }
        stack.getCurrentFrame().clearOperands();
        stack.push_Reference(objectref);
        stack.getPC().setValue(methods.data() + handler_method_offset);
        return;
      }
    }

    stack.pop_Frame();
    if (!stack.empty()) {
      checking_caller_frame = true;
      this->context.backToPreviousPackageID();
    }
  }

  // An exception with no matching handler must not look like normal method
  // completion to the interpreter.
  if (exception_package.getPackageID() == 0) {
    const auto *package_info = exception_package.getCap().getHeader();
    static constexpr uint8_t framework_aid[] = {
        0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x01};
    if (package_info->package.AID_length == sizeof(framework_aid) &&
        std::memcmp(package_info->package.AID, framework_aid,
                    sizeof(framework_aid)) == 0) {
      // JC305's ISOException import token 7 maps to ROM-mask export token 18.
      static constexpr uint8_t iso_exception_export_token = 18;
      const uint16_t iso_exception_index = HTONS(
          Export_Handler(exception_package)
              .getExportedClassOffset(iso_exception_export_token));
      const auto iso_exception_class = exception_cp.resolveClassref(
          exception_cp.getClassRefFromClassIndex(iso_exception_index));
      const bool is_iso_exception =
          Class_Handler::docheckcast(exception_class, iso_exception_class);
      if (is_iso_exception) {
        // ISOException inherits CardRuntimeException.reason, a short stored
        // as the first instance field in the embedded Java Card API layout.
        const uint16_t status_word = static_cast<uint16_t>(
            exception->getField_Short(0));
        throw ISOExceptionStatusWord{status_word};
      }
    }
  }

  throw Exceptions::SecurityException;
}

} // namespace jcvm
