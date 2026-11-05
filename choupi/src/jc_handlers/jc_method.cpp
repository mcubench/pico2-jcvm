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
#include <cstddef>
#include <cstdio>

namespace jcvm {

namespace {

uint16_t method_offset_be16(const uint8_t *bytes) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                               bytes[1]);
}

template <typename VisitOffset>
bool visit_cap_method_offsets(const JC_Cap &cap, VisitOffset &&visit) {
  const auto *method_component = cap.getMethod();
  if (method_component == nullptr) return false;

  // Every applet install entry is a callable method start.
  if (const auto *applet = cap.getApplet(); applet != nullptr) {
    const std::size_t component_size = NTOHS(applet->size);
    if (component_size < 1) return false;
    const uint8_t *cursor = reinterpret_cast<const uint8_t *>(applet->applets);
    std::size_t remaining = component_size - 1;
    for (uint8_t index = 0; index < applet->count; ++index) {
      if (remaining < 1) return false;
      const std::size_t aid_length = cursor[0];
      const std::size_t record_size = 1 + aid_length + 2;
      if (aid_length < 5 || aid_length > 16 || record_size > remaining ||
          !visit(method_offset_be16(cursor + 1 + aid_length))) {
        return false;
      }
      cursor += record_size;
      remaining -= record_size;
    }
    if (remaining != 0) return false;
  }

  // Class public/package virtual tables contain their concrete method starts.
  if (const auto *class_component = cap.getClass();
      class_component != nullptr) {
    const auto class_bytes = class_component->claz();
    const uint8_t *data = class_bytes.data();
    const std::size_t size = class_bytes.size();
    std::size_t offset = 0;
    const auto *header = cap.getHeader();
    if (header == nullptr) return false;
    if (header->minor_version >= 2) {
      if (!class_component->hasValidSignaturePool()) return false;
      offset = class_component->classInfoStartOffset();
    }
    if (offset > size) return false;

    // Class 2.1 and 2.2 place interface_info records before class_info records.
    while (offset < size && (data[offset] & 0x80u) != 0) {
      const uint8_t flags = data[offset] >> 4;
      const std::size_t superinterfaces = data[offset] & 0x0fu;
      const std::size_t name_length_offset =
          offset + 1 + superinterfaces * sizeof(jc_cap_class_ref);
      std::size_t record_size = 1 + superinterfaces * sizeof(jc_cap_class_ref);
      if ((flags & 0x02u) != 0) {
        if (name_length_offset >= size ||
            data[name_length_offset] > size - name_length_offset - 1) {
          return false;
        }
        record_size += 1 + data[name_length_offset];
      }
      if (record_size > size - offset) return false;
      offset += record_size;
    }

    while (offset < size) {
      constexpr std::size_t class_prefix_size = 10;
      if (size - offset < class_prefix_size ||
          (data[offset] & 0x80u) != 0) {
        return false;
      }
      const uint8_t interface_count = data[offset] & 0x0fu;
      const std::size_t public_count = data[offset + 7];
      const std::size_t package_count = data[offset + 9];
      const std::size_t table_count = public_count + package_count;
      const std::size_t table_bytes = table_count * sizeof(uint16_t);
      if (table_bytes > size - offset - class_prefix_size) return false;
      for (std::size_t index = 0; index < table_count; ++index) {
        if (!visit(method_offset_be16(
                data + offset + class_prefix_size + index * 2))) {
          return false;
        }
      }

      std::size_t record_size = class_prefix_size + table_bytes;
      std::size_t cursor = offset + record_size;
      for (uint8_t index = 0; index < interface_count; ++index) {
        if (size - cursor < 3) return false;
        const std::size_t method_map_count = data[cursor + 2];
        const std::size_t interface_record_size = 3 + method_map_count;
        if (interface_record_size > size - cursor) return false;
        cursor += interface_record_size;
        record_size += interface_record_size;
      }
      const uint8_t flags = data[offset] >> 4;
      if ((flags & 0x02u) != 0) {
        // The CAP validator has already validated the variable remote class
        // record. getSize() accounts for its remote method signatures/names.
        const auto *class_info =
            reinterpret_cast<const jc_cap_class_info *>(data + offset);
        const std::size_t full_size = class_info->getSize();
        if (full_size < record_size || full_size > size - offset) return false;
        record_size = full_size;
      }
      if (record_size == 0 || record_size > size - offset) return false;
      offset += record_size;
    }
  }

  // Static method calls use internal CONSTANT_StaticMethodref offsets.
  if (const auto *constant_pool = cap.getConstantPool();
      constant_pool != nullptr) {
    const auto entries = constant_pool->constantpool();
    for (std::size_t index = 0; index < entries.size(); ++index) {
      const auto &entry = entries.data()[index];
      if (entry.tag != JC_CP_TAG_CONSTANT_STATICMETHODREF) continue;
      const auto &reference = entry.info.static_method_ref_info.static_method_ref;
      if (IS_CP_INTERNAL_REF(reference) &&
          !visit(NTOHS(reference.internal_ref.offset))) {
        return false;
      }
    }
  }

  // Exported static methods are callable entry points even when no local
  // ConstantPool entry references them.
  if (const auto *export_component = cap.getExport();
      export_component != nullptr) {
    const std::size_t component_size = NTOHS(export_component->size);
    if (component_size < 1) return false;
    const uint8_t *cursor = export_component->class_exports;
    std::size_t remaining = component_size - 1;
    for (uint8_t index = 0; index < export_component->class_count; ++index) {
      if (remaining < 4) return false;
      const std::size_t fields = cursor[2];
      const std::size_t static_methods = cursor[3];
      const std::size_t field_bytes = fields * sizeof(uint16_t);
      const std::size_t method_bytes = static_methods * sizeof(uint16_t);
      const std::size_t record_size = 4 + field_bytes + method_bytes;
      if (record_size > remaining) return false;
      for (std::size_t method = 0; method < static_methods; ++method) {
        if (!visit(method_offset_be16(cursor + 4 + field_bytes + method * 2))) {
          return false;
        }
      }
      cursor += record_size;
      remaining -= record_size;
    }
    if (remaining != 0) return false;
  }

  return true;
}

} // namespace

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
  const uintptr_t method_to_call_address =
      reinterpret_cast<uintptr_t>(method_to_call);
  if (method_to_call_address < method_begin_address ||
      method_to_call_address >= method_end_address) {
    throw Exceptions::SecurityException;
  }
  const std::size_t method_header_offset =
      static_cast<std::size_t>(method_to_call_address - method_begin_address);
  const std::size_t component_method_offset =
      handler_table_bytes + 1 + method_header_offset;
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
  if (!have_body_bounds && descriptor == nullptr &&
      cap_execution_policy::is_dynamic_package(this->package.getPackageID())) {
    have_body_bounds = cap_method_body_bounds::find_from_offsets(
        methods.data(), methods.size(), handler_table_bytes,
        static_cast<uint16_t>(component_method_offset),
        [&](const auto &visit) {
          return visit_cap_method_offsets(this->package.getCap(), visit);
        },
        body);
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
