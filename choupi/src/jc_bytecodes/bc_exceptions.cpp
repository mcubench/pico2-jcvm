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
#include "../jc_handlers/jc_import.hpp"
#include "javacard-class-token-map.h"
#include "../jc_types/jc_instance.hpp"
#include "../stack.hpp"
#include "bytecodes.hpp"

#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace jcvm {

namespace {
bool isCryptoExceptionClass(
    const std::pair<Package, const uint8_t *> class_reference) {
  static constexpr uint8_t framework_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x01};
  static constexpr uint8_t security_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};
  const auto *header = class_reference.first.getCap().getHeader();
  if (header == nullptr) {
    return false;
  }

  const auto &package = header->package;
  ConstantPool_Handler class_pool(class_reference.first);
  constexpr uint8_t jc305_crypto_exception_class_token = 12;
  const uint8_t crypto_exception_export_token =
      jc305_javacard_security_class_tokens[
          jc305_crypto_exception_class_token];
  if (package.AID_length == sizeof(framework_aid) &&
      std::memcmp(package.AID, framework_aid, sizeof(framework_aid)) == 0) {
    // The combined ROM framework package does not expose the security
    // package export-token namespace. Match its runtime-owned exception
    // record directly.
    constexpr jclass_index_t crypto_exception_rom_index = 234;
    return class_reference.second == reinterpret_cast<const uint8_t *>(
                                         class_pool.getClassFromClassIndex(
                                             crypto_exception_rom_index));
  }
  if (package.AID_length == sizeof(security_aid) &&
      std::memcmp(package.AID, security_aid, sizeof(security_aid)) == 0) {
    // Optional providers change byte offsets in the security Class component.
    // Resolve CryptoException from the package export table instead.
    const auto class_index = HTONS(Export_Handler(class_reference.first)
                                       .getExportedClassOffset(
                                           crypto_exception_export_token));
    return class_reference.second == reinterpret_cast<const uint8_t *>(
                                         class_pool.getClassFromClassIndex(
                                             class_index));
  }
  return false;
}
bool isCryptoExceptionCatchType(Package source_package,
                                const jc_cap_class_ref catch_ref) {
  if (!catch_ref.isExternalClassRef() ||
      !usesJC305CompatibilityTokens(source_package)) {
    return false;
  }
  Import_Handler imports(source_package);
  const auto *imported_package = imports.getPackageAID(
      catch_ref.external_classref.package_token & 0x7F);
  static constexpr uint8_t security_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};
  return imported_package->AID_length == sizeof(security_aid) &&
         std::memcmp(imported_package->AID, security_aid,
                     sizeof(security_aid)) == 0 &&
         remap_imported_class_token(
             *imported_package, catch_ref.external_classref.class_token) ==
             jc305_javacard_security_class_tokens[12];
}
} // namespace

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
  Stack::ReferenceValue thrown_reference{};
  Stack &stack = this->context.getStack();

  TRACE_JCVM_DEBUG("ATHROW");

#ifndef PC_VERSION
  {
    const auto &frame = stack.getCurrentFrame();
    std::printf("CHOUPI:ATHROW depth=%u package=%u\r\n",
                static_cast<unsigned>(frame.getTOS() - frame.getOP()),
                static_cast<unsigned>(this->context.getCurrentPackageID()));
    std::fflush(stdout);
  }
#endif
  thrown_reference = stack.pop_ReferenceValue();
#ifndef PC_VERSION
  std::printf("CHOUPI:ATHROW ref=%08X\r\n",
              static_cast<unsigned>(thrown_reference.reference.compact()));
  std::fflush(stdout);
#endif
  this->doThrow(thrown_reference.reference, thrown_reference.owner);

  return;
}

/**
 * Throwing an exception
 *
 */
void Bytecodes::doThrow(jref_t objectref, const uint8_t reference_owner) {
  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }

  if (objectref.isArray()) {
    throw Exceptions::SecurityException;
  }

  const auto current_package =
      static_cast<uint8_t>(this->context.getCurrentPackageID());
  if (reference_owner != Stack::kCurrentReferenceOwner &&
      reference_owner != Stack::kGlobalReferenceOwner &&
      reference_owner != current_package) {
    if (this->dispatchJavaException(Exceptions::SecurityException)) {
      return;
    }
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
#ifdef PC_VERSION
  const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
  const bool trace_dispatch = trace_setting != nullptr &&
                              std::strcmp(trace_setting, "1") == 0;
  if (trace_dispatch) {
    std::fprintf(stderr,
                 "CHOUPI:EXCEPTION_START ref=%04X owner=%u depth=%u\n",
                 static_cast<unsigned>(objectref.compact()),
                 static_cast<unsigned>(reference_owner),
                 static_cast<unsigned>(stack.frameDepth()));
    const auto *exception_header = exception_package.getCap().getHeader();
    std::fprintf(stderr, "CHOUPI:EXCEPTION_AID package=%u aid=",
                 static_cast<unsigned>(exception_package.getPackageID()));
    if (exception_header != nullptr) {
      for (uint8_t index = 0; index < exception_header->package.AID_length;
           ++index) {
        std::fprintf(stderr, "%02X", exception_header->package.AID[index]);
      }
    }
    std::fprintf(stderr, "\n");
    std::fprintf(stderr,
                 "CHOUPI:EXCEPTION_THROW package=%u class=%u fields=%u\n",
                 static_cast<unsigned>(exception->getPackageID()),
                 static_cast<unsigned>(exception->getClassIndex()),
                 static_cast<unsigned>(exception->getNumberOfFields()));
    if (isCryptoExceptionClass(exception_class)) {
      std::fprintf(stderr, "CHOUPI:CRYPTO_EXCEPTION reason=%u\n",
                   static_cast<unsigned>(exception->getField_Short(0)));
    }
  }
#endif

#ifndef PC_VERSION
  static uint8_t exception_trace_budget = 0;
  if (exception_trace_budget < 8) {
    const auto field_count = exception->getNumberOfFields();
    const uint16_t first_field =
        field_count == 0 ? 0 : static_cast<uint16_t>(exception->getField_Short(0));
    std::printf("CHOUPI:EXCEPTION_DISPATCH package=%u class=%u fields=%u field0=%04X\r\n",
                static_cast<unsigned>(exception->getPackageID()),
                static_cast<unsigned>(exception->getClassIndex()),
                static_cast<unsigned>(field_count),
                static_cast<unsigned>(first_field));
    std::fflush(stdout);
    ++exception_trace_budget;
    std::printf(
        "CHOUPI:EXCEPTION_CLASS object_package=%u object_class=%u "
        "resolved_package=%u resolved_address=%p\r\n",
        static_cast<unsigned>(exception->getPackageID()),
        static_cast<unsigned>(exception->getClassIndex()),
        static_cast<unsigned>(exception_class.first.getPackageID()),
        static_cast<const void *>(exception_class.second));
    std::fflush(stdout);
  }
#endif

  while (!stack.empty() &&
         (!this->enforce_exception_boundary ||
          stack.frameDepth() > this->exception_boundary_depth)) {
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
#ifdef PC_VERSION
    if (trace_dispatch) {
      std::fprintf(stderr,
                   "CHOUPI:EXCEPTION_FRAME package=%u pc=%u opcode=%02X caller=%u handlers=%u\n",
                   static_cast<unsigned>(package.getPackageID()),
                   static_cast<unsigned>(opcode_offset),
                   static_cast<unsigned>(methods[opcode_offset]),
                   checking_caller_frame ? 1u : 0u,
                   static_cast<unsigned>(method_component->exception_handlers().size()));
    }
#endif
#ifndef PC_VERSION
    if (exception_trace_budget <= 8) {
      std::printf("CHOUPI:EXCEPTION_FRAME package=%u pc=%u opcode=%02X caller=%u\r\n",
                  static_cast<unsigned>(package.getPackageID()),
                  static_cast<unsigned>(opcode_offset),
                  static_cast<unsigned>(methods[opcode_offset]),
                  checking_caller_frame ? 1u : 0u);
      std::fflush(stdout);
    }
#endif
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
#ifdef PC_VERSION
      if (trace_dispatch) {
        std::fprintf(stderr,
                     "CHOUPI:EXCEPTION_RANGE index=%u start=%u len=%u handler=%u catch=%u\n",
                     static_cast<unsigned>(index), static_cast<unsigned>(start),
                     static_cast<unsigned>(active_length),
                     static_cast<unsigned>(handler_component_offset),
                     static_cast<unsigned>(catch_type_index));
      }
#endif
      if (start <= handler_table_bytes ||
          handler_component_offset <= handler_table_bytes) {
        throw Exceptions::SecurityException;
      }
      // CAP offsets are one-based after the handler table, like handler
      // destinations; convert them to the zero-based methods() span.
      const uint32_t start_method_offset = start - handler_table_bytes - 1;
      const uint32_t handler_method_offset =
          handler_component_offset - handler_table_bytes - 1;
      const uint32_t protected_range_end =
          start_method_offset + static_cast<uint32_t>(active_length);
      if (start_method_offset >= methods.size() ||
          protected_range_end > methods.size()) {
        throw Exceptions::SecurityException;
      }
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

      const auto *range_start = methods.data() + start_method_offset;
      const auto *range_end = methods.data() + protected_range_end;
      const pc_t &frame_pc = stack.getPC();
      const bool start_boundary =
          frame_pc.isInstructionBoundary(range_start, false);
      const bool end_in_frame = frame_pc.containsAddress(range_end, true);
#ifdef PC_VERSION
      if (trace_dispatch) {
        std::fprintf(stderr,
                     "CHOUPI:EXCEPTION_ACTIVE_RANGE index=%u start=%u end=%u "
                     "start_boundary=%u end_in_frame=%u\n",
                     static_cast<unsigned>(index),
                     static_cast<unsigned>(start_method_offset),
                     static_cast<unsigned>(protected_range_end),
                     start_boundary ? 1u : 0u, end_in_frame ? 1u : 0u);
      }
#endif
      if (!start_boundary || !end_in_frame) {
        throw Exceptions::SecurityException;
      }

      bool matches = catch_type_index == 0;
      if (!matches) {
        ConstantPool_Handler catch_cp(package);
#ifdef PC_VERSION
        if (trace_dispatch) {
          const auto cp_entry = catch_cp.getCPEntry(catch_type_index);
          std::fprintf(stderr,
                       "CHOUPI:EXCEPTION_CATCH stage=cp-entry index=%u tag=%u\n",
                       static_cast<unsigned>(catch_type_index),
                       static_cast<unsigned>(cp_entry.tag));
        }
#endif
        const auto catch_ref = catch_cp.getClassRef(catch_type_index);
#ifdef PC_VERSION
        if (trace_dispatch) {
          std::fprintf(stderr,
                       "CHOUPI:EXCEPTION_CATCH stage=class-ref index=%u\n",
                       static_cast<unsigned>(catch_type_index));
        }
#endif
        const auto catch_class = catch_cp.resolveClassref(catch_ref);
#ifndef PC_VERSION
        if (exception_trace_budget <= 8) {
          std::printf(
              "CHOUPI:EXCEPTION_CATCH_REF index=%u external=%u "
              "package_token=%u class_token=%u crypto_catch=%u "
              "crypto_object=%u crypto_resolved=%u\r\n",
              static_cast<unsigned>(catch_type_index),
              catch_ref.isExternalClassRef() ? 1u : 0u,
              catch_ref.isExternalClassRef()
                  ? static_cast<unsigned>(catch_ref.external_classref.package_token)
                  : 0u,
              catch_ref.isExternalClassRef()
                  ? static_cast<unsigned>(catch_ref.external_classref.class_token)
                  : 0u,
              isCryptoExceptionCatchType(package, catch_ref) ? 1u : 0u,
              isCryptoExceptionClass(exception_class) ? 1u : 0u,
              isCryptoExceptionClass(catch_class) ? 1u : 0u);
          std::fflush(stdout);
        }
#endif
#ifdef PC_VERSION
        if (trace_dispatch) {
          std::fprintf(stderr,
                       "CHOUPI:EXCEPTION_CATCH stage=class-resolved\n");
        }
#endif
        matches = Class_Handler::docheckcast(exception_class, catch_class);
#ifdef PC_VERSION
        if (trace_dispatch) {
          std::fprintf(stderr,
                       "CHOUPI:EXCEPTION_CATCH stage=assignable result=%u\n",
                       matches ? 1u : 0u);
        }
#endif
        // CryptoException has two class records in this VM's API model: one
        // in the combined ROM mask and one in the JC305 imported security
        // package. They are the same Java Card type even though their class
        // records have different addresses and offsets.
        if (!matches && isCryptoExceptionClass(exception_class) &&
            (isCryptoExceptionClass(catch_class) ||
             isCryptoExceptionCatchType(package, catch_ref))) {
          matches = true;
        }
      }
#ifdef PC_VERSION
      if (trace_dispatch) {
        std::fprintf(stderr,
                     "CHOUPI:EXCEPTION_HANDLER index=%u range=%u..%u catch=%u match=%u\n",
                     static_cast<unsigned>(index),
                     static_cast<unsigned>(start_method_offset),
                     static_cast<unsigned>(start_method_offset + active_length),
                     static_cast<unsigned>(catch_type_index), matches ? 1u : 0u);
      }
#endif

      if (matches) {
        if (handler_method_offset >= methods.size()) {
          throw Exceptions::SecurityException;
        }
        stack.getCurrentFrame().clearOperands();
        stack.push_Reference(objectref, reference_owner);
        // Handler offsets come from the uploaded Method component. Keep the
        // transfer inside this frame and reject offsets into instruction
        // operands just like dynamic-package branch targets.
        stack.getPC().setBranchFromOffset(
            methods.data() + handler_method_offset, 0, true);
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
      // Resolve ISOException through the generated JC305-to-ROM token map.
      // The ROM-mask API order changes with its selected capabilities, so a
      // fixed export token can alias a different exception class.
      constexpr uint8_t jc305_iso_exception_token = 7;
      constexpr uint8_t iso_exception_export_token =
          jc305_javacard_framework_class_tokens[
              jc305_iso_exception_token];
      const uint16_t iso_exception_index = HTONS(
          Export_Handler(exception_package)
              .getExportedClassOffset(iso_exception_export_token));
      // The ROM Class component keeps a runtime-owned ISOException class
      // record 34 bytes after the public exported class record. Derive its
      // location from the export offset because optional providers shift the
      // Class component in each support profile.
      const jclass_index_t iso_exception_rom_offset =
          static_cast<jclass_index_t>(iso_exception_index + 34);
      const auto iso_exception_class = exception_cp.resolveClassref(
          exception_cp.getClassRefFromClassIndex(iso_exception_index));
      const bool is_iso_exception =
          exception->getClassIndex() == iso_exception_index ||
          exception->getClassIndex() == iso_exception_rom_offset ||
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

  if (this->enforce_exception_boundary &&
      stack.frameDepth() <= this->exception_boundary_depth) {
    uint8_t uncaught_owner = reference_owner;
    if (uncaught_owner == Stack::kCurrentReferenceOwner) {
      uncaught_owner = heap.isRuntimeExceptionReference(objectref)
                           ? Stack::kGlobalReferenceOwner
                           : static_cast<uint8_t>(context.getAppletID());
    }
    throw UncaughtJavaException{objectref.compact(), uncaught_owner};
  }

  throw Exceptions::SecurityException;
}

} // namespace jcvm
