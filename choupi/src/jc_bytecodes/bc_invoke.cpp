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

#include "../jc_config.h"

#include "../types.hpp"

#include "../context.hpp"
#include "../debug.hpp"
#include "../exceptions.hpp"
#include "../heap.hpp"
#include "../jc_handlers/flashmemory.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#include "../jc_handlers/jc_export.hpp"
#include "../jc_handlers/jc_import.hpp"
#include "../jc_handlers/jc_method.hpp"
#include "../jc_handlers/package.hpp"
#include "../jc_types/jc_array.hpp"
#include "../jc_types/jc_field.hpp"
#include "../jc_types/jc_instance.hpp"
#include "../interpretor.hpp"
#include "../stack.hpp"
#include "javacard-rom-metadata.h"
#include "shareable_arguments.hpp"
#include "bytecodes.hpp"
#include "../../../src/vm_applet_context.hpp"

#include <array>
#include <cstring>
#include <cstdio>

namespace jcvm {

namespace {
Frame::SensitiveReturn pinSensitiveReturn(
    Context &context, const jc_cap_class_ref &interface_ref,
    const uint8_t mapped_method) {
  if (!FlashMemory_Handler::isDynamicPackageID(context.getCurrentPackageID()) ||
      !interface_ref.isExternalClassRef()) {
    return Frame::SensitiveReturn::None;
  }
  const auto &external = interface_ref.external_classref;
  const auto *package_info = Import_Handler(context.getCurrentPackage())
                                .getPackageAID(external.package_token & 0x7F);
  if (package_info == nullptr || package_info->AID_length != 7 ||
      std::memcmp(package_info->AID, "\xA0\x00\x00\x00\x62\x01\x01", 7) != 0) {
    return Frame::SensitiveReturn::None;
  }
  // Embedded PIN method order: getTriesRemaining, check, isValidated, reset.
  if (external.class_token == 1) {
    if (mapped_method == 0) return Frame::SensitiveReturn::Short;
    if (mapped_method == 1 || mapped_method == 2)
      return Frame::SensitiveReturn::Boolean;
  }
  // OwnerPINx and its predecrement variant extend the same PIN prefix.
  if (external.class_token == 19 || external.class_token == 20) {
    if (mapped_method == 0 || mapped_method == 5 ||
        (external.class_token == 20 && mapped_method == 8)) {
      return Frame::SensitiveReturn::Short;
    }
    if (mapped_method == 1 || mapped_method == 2)
      return Frame::SensitiveReturn::Boolean;
  }
  return Frame::SensitiveReturn::None;
}

Frame::SensitiveReturn ownerPinVirtualSensitiveReturn(
    Context &context, const jc_cap_virtual_method_ref_info &method_ref) {
  if (!FlashMemory_Handler::isDynamicPackageID(context.getCurrentPackageID()) ||
      !method_ref.class_ref.isExternalClassRef()) {
    return Frame::SensitiveReturn::None;
  }
  const auto &external = method_ref.class_ref.external_classref;
  if (external.class_token != 9) {
    return Frame::SensitiveReturn::None;
  }
  const auto *package_info = Import_Handler(context.getCurrentPackage())
                                .getPackageAID(external.package_token & 0x7F);
  if (package_info == nullptr || package_info->AID_length != 7 ||
      std::memcmp(package_info->AID, "\xA0\x00\x00\x00\x62\x01\x01", 7) != 0) {
    return Frame::SensitiveReturn::None;
  }
  // JC305 OwnerPIN public method tokens: check=1, getTriesRemaining=2,
  // getValidatedFlag=3, and isValidated=4.
  switch (method_ref.token & 0x7F) {
  case 2:
    return Frame::SensitiveReturn::Short;
  case 1:
  case 3:
  case 4:
    return Frame::SensitiveReturn::Boolean;
  default:
    return Frame::SensitiveReturn::None;
  }
}

Frame::SensitiveReturn randomDataVirtualSensitiveReturn(
    Context &context, const jc_cap_virtual_method_ref_info &method_ref) {
  if (!FlashMemory_Handler::isDynamicPackageID(context.getCurrentPackageID()) ||
      !method_ref.class_ref.isExternalClassRef()) {
    return Frame::SensitiveReturn::None;
  }
  const auto &external = method_ref.class_ref.external_classref;
  if (external.class_token != 14 && external.class_token != 32) {
    return Frame::SensitiveReturn::None;
  }
  const auto *package_info = Import_Handler(context.getCurrentPackage())
                                .getPackageAID(external.package_token & 0x7F);
  if (package_info == nullptr || package_info->AID_length != 7 ||
      std::memcmp(package_info->AID, "\xA0\x00\x00\x00\x62\x01\x02", 7) != 0) {
    return Frame::SensitiveReturn::None;
  }
  // JC305 RandomData tokens: nextBytes=3 and getAlgorithm=4.
  const uint8_t method_token = method_ref.token & 0x7F;
  if (method_token == 3 || method_token == 4) {
    return Frame::SensitiveReturn::Short;
  }
  return Frame::SensitiveReturn::None;
}

// Descriptor access_flags use java.lang.reflect.Modifier bit values, unlike
// the compact Class component flags used by JC_CAP_CLASS_ACC_INTERFACE.
constexpr uint8_t JC_CAP_DESCRIPTOR_ACC_INTERFACE = 0x80;

enum class ShareableSignatureStatus { Found, DescriptorUnavailable, Invalid };

bool isReferenceFieldType(const FieldType type) noexcept {
  return type == FIELD_TYPE_OBJECT || type == FIELD_TYPE_GLOBAL_AID_OBJECT ||
         type == FIELD_TYPE_SHAREABLE_PROXY_OBJECT ||
         type == FIELD_TYPE_ARRAY_OBJECT ||
         type == FIELD_TYPE_TRANSIENT_ARRAY_OBJECT;
}

jref_t copySioExceptionToCaller(Context &caller, Context &server,
                                const uint16_t compact_reference) {
  const auto source = server.getHeap().getInstance(jref_t(compact_reference));
  if (source == nullptr) throw Exceptions::SecurityException;

  const jref_t local_reference = caller.getHeap().addInstance(
      source->getPackageID(), source->getClassIndex());
  const auto destination = caller.getHeap().getInstance(local_reference);
  if (destination == nullptr) throw Exceptions::SecurityException;

  auto *source_fields = source->getFields();
  auto *destination_fields = destination->getFields();
  if (source_fields == nullptr || destination_fields == nullptr ||
      source_fields->size() != destination_fields->size()) {
    throw Exceptions::SecurityException;
  }
  for (uint16_t index = 0; index < source_fields->size(); ++index) {
    const jc_field_t field = source_fields->at(index);
    if (isReferenceFieldType(field.type) && field.value != 0) {
      const jref_t reference(field.value);
      if (!reference.isGlobalArray() && !reference.isGlobalInstance()) {
        // Do not make references into the server applet's private object graph
        // reachable from the caller through a copied exception.
        throw Exceptions::SecurityException;
      }
    }
    destination_fields->at(index) = field;
  }
  return local_reference;
}

ShareableSignatureStatus getShareableMethodSignature(
    const Package &package, const jc_cap_interface_info *interface_info,
    const uint8_t method_token, std::array<uint8_t, 128> &nibbles,
    uint8_t &nibble_count) {
  const auto *descriptor = package.getCap().getDescriptor();
  if (descriptor == nullptr) {
    return ShareableSignatureStatus::DescriptorUnavailable;
  }
  if (interface_info == nullptr) return ShareableSignatureStatus::Invalid;

  const auto *descriptor_bytes =
      reinterpret_cast<const uint8_t *>(descriptor);
  const uint16_t component_size = NTOHS(descriptor->size);
  const uint8_t *component_end = descriptor_bytes + 3 + component_size;
  const uint8_t *cursor = descriptor->data;
  ConstantPool_Handler cp(package);
  uint16_t method_type_offset = 0;
  bool method_found = false;

  for (uint8_t class_index = 0; class_index < descriptor->class_count;
       ++class_index) {
    if (cursor + sizeof(jc_cap_class_descriptor_info) > component_end) {
      return ShareableSignatureStatus::Invalid;
    }
    const auto *class_descriptor =
        reinterpret_cast<const jc_cap_class_descriptor_info *>(cursor);
    const bool is_interface =
        (class_descriptor->access_flags & JC_CAP_DESCRIPTOR_ACC_INTERFACE) !=
        0;
    const bool matches_interface =
        is_interface &&
        cp.classref2interface(class_descriptor->this_class_ref).second ==
            interface_info;
    const auto methods = class_descriptor->methods();
    if (matches_interface) {
      for (uint16_t method_index = 0; method_index < methods.size();
           ++method_index) {
        const auto &method_descriptor = methods.at(method_index);
        if (method_descriptor.token == method_token) {
          if (method_found) return ShareableSignatureStatus::Invalid;
          method_type_offset = NTOHS(method_descriptor.type_offset);
          method_found = true;
        }
      }
    }

    const size_t class_record_size =
        sizeof(jc_cap_class_descriptor_info) +
        static_cast<size_t>(class_descriptor->interface_count) *
            sizeof(jc_cap_class_ref) +
        NTOHS(class_descriptor->field_count) *
            sizeof(jc_cap_field_descriptor_info) +
        NTOHS(class_descriptor->method_count) *
            sizeof(jc_cap_method_descriptor_info);
    if (class_record_size > static_cast<size_t>(component_end - cursor)) {
      return ShareableSignatureStatus::Invalid;
    }
    cursor += class_record_size;
  }
  if (!method_found || cursor > component_end) {
    return ShareableSignatureStatus::Invalid;
  }

  // Type offsets are relative to the TypeDescriptor component body, which
  // follows the variable-length class descriptor records. Avoid the legacy
  // accessor's fixed-size class-table assumption here.
  const uint8_t *types = cursor;
  const size_t remaining = static_cast<size_t>(component_end - types);
  if (method_type_offset >= remaining) return ShareableSignatureStatus::Invalid;
  const uint8_t *signature = types + method_type_offset;
  if (static_cast<size_t>(component_end - signature) < 1) {
    return ShareableSignatureStatus::Invalid;
  }
  nibble_count = signature[0];
  const size_t byte_count = (static_cast<size_t>(nibble_count) + 1) / 2;
  if (nibble_count == 0 || nibble_count > nibbles.size() ||
      byte_count > static_cast<size_t>(component_end - signature) - 1) {
    return ShareableSignatureStatus::Invalid;
  }
  for (uint16_t nibble_index = 0; nibble_index < nibble_count;
       ++nibble_index) {
    const uint8_t packed = signature[1 + nibble_index / 2];
    nibbles[nibble_index] = static_cast<uint8_t>(
        (nibble_index & 1) == 0 ? packed >> 4 : packed & 0x0f);
  }
  return ShareableSignatureStatus::Found;
}

bool classImplementsShareableInterface(const Package &package,
                                       const jclass_index_t class_index) {
  Package current_package = package;
  auto *current_class =
      ConstantPool_Handler(current_package).getClassFromClassIndex(class_index);
  for (uint16_t depth = 0; depth < 256 && current_class != nullptr; ++depth) {
    if (current_class->isObjectClass()) return false;
    ConstantPool_Handler class_cp(current_package);
    for (uint8_t index = 0; index < current_class->interface_count; ++index) {
      try {
        const auto interface_info = class_cp.classref2interface(
            current_class->interfaces(index).interface);
        if ((interface_info.second->flags & JC_CAP_CLASS_ACC_SHAREABLE) != 0) {
          return true;
        }
      } catch (Exceptions) {
        // Ignore malformed/unresolvable entries here; the call remains
        // fail-closed unless another resolved interface proves Shareable.
      }
    }
    const auto superclass =
        class_cp.classref2class(current_class->super_class_ref);
    current_package = superclass.first;
    current_class = superclass.second;
  }
  // Cyclic or over-deep metadata cannot establish that the class is
  // shareable. Fail closed without keeping an invocation handler in a loop.
  return false;
}

jref_t wrapShareableReference(Context &client_context,
                              Context &server_context,
                              const uint8_t *server_aid,
                              const uint8_t server_aid_length,
                              const jref_t server_reference) {
  if (server_reference.isNullPointer() ||
      server_reference.isGlobalArray() ||
      server_reference.isGlobalInstance()) {
    return server_reference;
  }
  if (server_reference.isArray()) {
    throw Exceptions::SecurityException;
  }

  const auto server_object =
      server_context.getHeap().getInstance(server_reference);
  const Package server_package(server_object->getPackageID());
  if (!classImplementsShareableInterface(server_package,
                                         server_object->getClassIndex())) {
    throw Exceptions::SecurityException;
  }

  const jref_t existing = client_context.findShareableProxy(
      server_aid, server_aid_length, server_reference);
  if (!existing.isNullPointer()) {
    return existing;
  }
  if (!server_context.retainShareableObjectRoot(server_reference)) {
    throw Exceptions::SecurityException;
  }
  const jref_t proxy = client_context.getHeap().addInstance(
      server_object->getPackageID(), server_object->getClassIndex());
  if (!client_context.registerShareableProxy(
          proxy, server_aid, server_aid_length, server_reference,
          server_object->getPackageID())) {
    throw Exceptions::SecurityException;
  }
  return proxy;
}
} // namespace

/**
 * Invoke instance method; dispatch based on class
 *
 * Format:
 *   invokevirtual
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   invokevirtual = 139 (0x8b)
 *
 * Stack:
 *   ..., objectref, [arg1, [arg2 ...]] -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at that index must be of type CONSTANT_VirtualMethodref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a virtual method
 *   token. The specified method is resolved. The method must not be <init>,
 *   an instance initialization method, or <clinit>, a class or interface
 *   initialization method. Finally, if the resolved method is protected,
 *   and it is a member of a superclass of the current class, and the method
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The resolved method reference includes an unsigned index into the
 *   method table of the resolved class and an unsigned byte nargs that must
 *   not be zero.
 *
 *   The objectref must be of type reference. The index is an unsigned byte
 *   that is used as an index into the method table of the class of the type
 *   of objectref. If the objectref is an array type, then the method table
 *   of class Object (§2.2.1.4 Unsupported Classes) is used. The table entry
 *   at that index includes a direct reference to the method's code and
 *   modifier information.
 *
 *   The objectref must be followed on the operand stack by nargs - 1 words of
 *   arguments, where the number of words of arguments and the type and order of
 *   the values they represent must be consistent with those of the selected
 *   instance method.
 *
 *   The nargs - 1 words of arguments and objectref are popped from the
 *   operand stack. A new stack frame is created for the method being
 *   invoked, and objectref and the arguments are made the values of its
 *   first nargs words of local variables, with objectref in local variable
 *   0, arg1 in local variable 1, and so on. The new stack frame is then
 *   made current, and the Java Card virtual machine pc is set to the opcode
 *   of the first instruction of the method to be invoked. Execution
 *   continues with the first instruction of the method.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the invokevirtual instruction throws a
 *   NullPointerException.
 *
 *   In some circumstances, the invokevirtual instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   context (§3.4 Contexts) of the object referenced by objectref. The
 *   exact circumstances when the exception will be thrown are specified in
 *   Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition. If the current context is not the
 *   object's context and the Java Card RE permits invocation of the method,
 *   the invokevirtual instruction will cause a context switch (§3.4
 *   Contexts) to the object's context before invoking the method, and will
 *   cause a return context switch to the previous context when the invoked
 *   method returns.
 *
 */
void Bytecodes::bc_invokevirtual() {
  Context &context = this->context;
  Stack &stack = this->context.getStack();
  ConstantPool_Handler cp_handler(this->context.getCurrentPackage());
  Method_Handler method_handler(this->context);
  Class_Handler class_handler(this->context.getCurrentPackage());
  pc_t &pc = stack.getPC();

  uint16_t index = pc.getNextShort();

  TRACE_JCVM_DEBUG("INVOKEVIRTUAL 0x%04X", index);

  auto virtual_method_ref_info = cp_handler.getVirtualMethodRef(index);
  const auto declared_method =
      class_handler.getMethodOffset(virtual_method_ref_info);
  Method_Handler declared_method_handler(context);
  declared_method_handler.setPackage(declared_method.first);
  const uint8_t nargs =
      declared_method_handler.getArgumentCount(declared_method.second);
  if (nargs == 0) {
    throw Exceptions::SecurityException;
  }

  const Frame &caller_frame = stack.getCurrentFrame();
  if (static_cast<uint32_t>(nargs) >
      static_cast<uint32_t>(caller_frame.getTOS() - caller_frame.getOP())) {
    throw Exceptions::StackUnderflowException;
  }
  const jref_t receiver = stack.get_Pushed_Reference(nargs);
  if (receiver.isNullPointer()) {
    const auto &frame = stack.getCurrentFrame();
    const auto *method_component =
        context.getCurrentPackage().getCap().getMethod();
    const auto methods = method_component->methods();
    const auto *instruction = frame.getInstructionPC();
    const auto *method_start = frame.getMethodStartPC();
    std::printf(
        "CHOUPI:NULL_RECEIVER op=INVOKEVIRTUAL package=%u pc=%u "
        "method_pc=%u cp_index=%u nargs=%u receiver=%04X\r\n",
        static_cast<unsigned>(context.getCurrentPackageID()),
        instruction >= methods.data() &&
                instruction < methods.data() + methods.size()
            ? static_cast<unsigned>(instruction - methods.data())
            : 0xFFFFFFFFu,
        instruction >= method_start
            ? static_cast<unsigned>(instruction - method_start)
            : 0xFFFFFFFFu,
        static_cast<unsigned>(index), static_cast<unsigned>(nargs),
        static_cast<unsigned>(receiver.compact()));
    std::fflush(stdout);
    throw Exceptions::NullPointerException;
  }
  if (context.isShareableProxy(receiver)) {
    throw Exceptions::SecurityException;
  }
  if (context.isStaleShareableProxy(receiver)) {
    throw Exceptions::SecurityException;
  }

  const auto method_offset = [&]() {
    if (receiver.isArray()) {
      return declared_method;
    }
    const auto receiver_instance = context.getHeap().getInstance(receiver);
    return class_handler.getMethodOffset(
        virtual_method_ref_info, receiver_instance->getPackageID(),
        receiver_instance->getClassIndex());
  }();
  Package target_package = method_offset.first;
  uint16_t target_method_offset = method_offset.second;
  // ROM methods use embedded API tokens, while dynamic applet override tables
  // use the importing CAP's token order. Always resolve these overrides in
  // the inverse-mapped namespace: two different methods can have the same
  // argument count, so stack-shape checks alone cannot detect a bad dispatch.
  if (virtual_method_ref_info.isPublicMethod() &&
      !FlashMemory_Handler::isDynamicPackageID(
          context.getCurrentPackageID()) &&
      !receiver.isArray()) {
    const auto receiver_instance = context.getHeap().getInstance(receiver);
    if (FlashMemory_Handler::isDynamicPackageID(
            receiver_instance->getPackageID())) {
      const auto reverse_offset = class_handler.getMethodOffset(
          virtual_method_ref_info, receiver_instance->getPackageID(),
          receiver_instance->getClassIndex(), true);
      target_package = reverse_offset.first;
      target_method_offset = reverse_offset.second;
    }
  }
  Method_Handler resolved_handler(context);
  resolved_handler.setPackage(target_package);
  if (resolved_handler.getArgumentCount(target_method_offset) != nargs) {
    throw Exceptions::SecurityException;
  }
  auto sensitive_return =
      ownerPinVirtualSensitiveReturn(context, virtual_method_ref_info);
  if (sensitive_return == Frame::SensitiveReturn::None) {
    sensitive_return =
        randomDataVirtualSensitiveReturn(context, virtual_method_ref_info);
  }
  if (sensitive_return != Frame::SensitiveReturn::None) {
    context.resetSensitiveResult();
  }
  // Calling the method and updating PC value
  method_handler.setPackage(target_package);
  method_handler.callVirtualMethod(target_method_offset);
  if (sensitive_return != Frame::SensitiveReturn::None) {
    stack.getCurrentFrame().setSensitiveReturn(sensitive_return);
  }

  return;
}

/**
 * Invoke instance method; special handling for superclass, private, and
 * instance initialization method invocations
 *
 * Format:
 *   invokespecial
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   invokespecial = 140 (0x8c)
 *
 * Stack:
 *   ..., objectref, [arg1, [arg2 ...]] -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. If the invoked
 *   method is a private instance method or an instance initialization
 *   method, the constant pool item at index must be of type
 *   CONSTANT_StaticMethodref (§6.8.3 CONSTANT_StaticFieldref and
 *   CONSTANT_StaticMethodref), a reference to a statically linked instance
 *   method. If the invoked method is a superclass method, the constant pool
 *   item at index must be of type CONSTANT_SuperMethodref (6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to an instance method of a
 *   specified class. The reference is resolved. The resolved method must
 *   not be <clinit>, a class or interface initialization method. If the
 *   method is <init>, an instance initialization method, then the method
 *   must only be invoked once on an uninitialized object, and before the
 *   first backward branch following the execution of the new instruction
 *   that allocated the object. Finally, if the resolved method is
 *   protected, and it is a member of a superclass of the current class, and
 *   the method is not declared in the same package as the current class,
 *   then the class of objectref must be either the current class or a
 *   subclass of the current class.
 *
 *   The resolved method includes the code for the method, an unsigned byte
 *   nargs that must not be zero, and the method's modifier information.
 *
 *   The objectref must be of type reference, and must be followed on the
 *   operand stack by nargs - 1 words of arguments, where the number of
 *   words of arguments and the type and order of the values they represent
 *   must be consistent with those of the selected instance method.
 *
 *   The nargs - 1 words of arguments and objectref are popped from the
 *   operand stack. A new stack frame is created for the method being
 *   invoked, and objectref and the arguments are made the values of its
 *   first nargs words of local variables, with objectref in local variable
 *   0, arg1 in local variable 1, and so on. The new stack frame is then
 *   made current, and the Java Card virtual machine pc is set to the opcode
 *   of the first instruction of the method to be invoked. Execution
 *   continues with the first instruction of the method.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the invokespecial instruction throws a
 *   NullPointerException.
 */
void Bytecodes::bc_invokespecial() {
  Context &context = this->context;
  Stack &stack = context.getStack();
  ConstantPool_Handler cp_handler(context.getCurrentPackage());
  Method_Handler method_handler(context);
  uint16_t method_offset = 0;
  pc_t &pc = stack.getPC();

  uint16_t index = pc.getNextShort();

  TRACE_JCVM_DEBUG("INVOKESPECIAL 0x%04X", index);

  auto cp_entry = cp_handler.getCPEntry(index);
  TRACE_JCVM_DEBUG("INVOKESPECIAL CP tag=%u", cp_entry.tag);

  try {
  switch (cp_entry.tag) {
  case JC_CP_TAG_CONSTANT_STATICMETHODREF:
    /*
     * The invoked method is a private instance method or an instance
     * initialization method.
     */
    {
      jc_cap_static_method_ref_info method_ref =
          cp_entry.info.static_method_ref_info;

      if (IS_CP_INTERNAL_REF(method_ref.static_method_ref)) {
        method_offset = HTONS(method_ref.static_method_ref.internal_ref.offset);
      } else { // Is external static method ref
        TRACE_JCVM_DEBUG(
            "INVOKESPECIAL external package=%u class=%u method=%u",
            method_ref.static_method_ref.external_ref.package_token,
            method_ref.static_method_ref.external_ref.class_token,
            method_ref.static_method_ref.external_ref.token);
        Import_Handler imported(context.getCurrentPackage());
        const jc_cap_package_info *package_aid = imported.getPackageAID(
            method_ref.static_method_ref.external_ref.package_token & 0x7F);

        Package exported_package(imported.getPackageIndex(package_aid));
        Export_Handler export_handler(exported_package);

        const uint8_t imported_class_token =
            method_ref.static_method_ref.external_ref.class_token;
        const uint8_t class_token =
            FlashMemory_Handler::isDynamicPackageID(
                context.getCurrentPackageID())
                ? remap_imported_class_token(*package_aid,
                                             imported_class_token)
                : imported_class_token;
        // Constructors also use StaticMethodref entries. Resolve their
        // imported token in the constructor namespace rather than assuming
        // the ROM export uses the same token as the JC305 import.
        const uint8_t method_token =
            FlashMemory_Handler::isDynamicPackageID(
                context.getCurrentPackageID())
                ? remap_imported_constructor_method_token(
                      *package_aid, imported_class_token,
                      method_ref.static_method_ref.external_ref.token)
                : method_ref.static_method_ref.external_ref.token;
        TRACE_JCVM_DEBUG("INVOKESPECIAL mapped package=%u class=%u method=%u",
                         static_cast<unsigned>(imported.getPackageIndex(package_aid)),
                         class_token, method_token);
        method_offset = export_handler.getExportedStaticMethodOffset(
            class_token, method_token);
        TRACE_JCVM_DEBUG("INVOKESPECIAL resolved offset=%u", method_offset);
        method_handler.setPackage(exported_package);
      }
    }
    break;

  case JC_CP_TAG_CONSTANT_SUPERMETHODREF: {
    /*
     * The invoked method is a superclass method.
     */
    auto super_method_ref = cp_handler.getSuperMethodRef(index);
    jc_cap_virtual_method_ref_info virtual_method_ref_info{
        super_method_ref.class_ref, super_method_ref.token};

#ifdef JCVM_DYNAMIC_CHECKS_CAP

    /*
     * The class referenced in the CONSTANT_SuperMethodref_info structure must
     * always be internal to the class that defines the method that contains
     * the Java language-level super invocation. The class must be defined in
     * this package.
     */
    if (virtual_method_ref_info.class_ref.isExternalClassRef()) {
      // TODO
      throw Exceptions::SecurityException;
    }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

    // A SuperMethodref names the class that contains the invokespecial
    // instruction. Resolve against that class's immediate superclass so the
    // call reaches the inherited implementation instead of virtual dispatch
    // back into the overriding method.
    auto declaring_class =
        cp_handler.classref2class(virtual_method_ref_info.class_ref);
    virtual_method_ref_info.class_ref =
        declaring_class.second->super_class_ref;
    Class_Handler superclass_handler(declaring_class.first);
    auto method = superclass_handler.getMethodOffset(virtual_method_ref_info);
    method_handler.setPackage(method.first);
    method_offset = method.second;
  } break;

  default:
    // NOTE: Wrong invokestatic method type
    throw Exceptions::SecurityException;
    break;
  }

  const uint8_t nargs = method_handler.getArgumentCount(method_offset);
  const Frame &caller_frame = stack.getCurrentFrame();
  if (nargs == 0) {
    throw Exceptions::SecurityException;
  }
  if (static_cast<uint32_t>(nargs) >
      static_cast<uint32_t>(caller_frame.getTOS() - caller_frame.getOP())) {
    throw Exceptions::StackUnderflowException;
  }

  // The receiver is the nargs-th word from the top, before the call consumes
  // the receiver and arguments. Checking local 0 after the call is incorrect:
  // the current frame is then the caller, whose local 0 need not be a reference.
  const jref_t receiver = stack.get_Pushed_Reference(nargs);
  if (receiver.isNullPointer()) {
    throw Exceptions::NullPointerException;
  }
  if (context.isShareableProxy(receiver)) {
    throw Exceptions::SecurityException;
  }
  if (context.isStaleShareableProxy(receiver)) {
    throw Exceptions::SecurityException;
  }

  // Calling the method after validating its receiver.
  method_handler.callVirtualMethod(method_offset);

  } catch (Exceptions) {
    throw;
  }

  // auto instance = context.getHeap().getInstance(objectref);

  // instance->setInitialized(true);

  return;
}

/**
 * Invoke a class (static) method
 *
 * Format:
 *   invokestatic
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   invokestatic = 141 (0x8d)
 *
 * Stack:
 *   ..., [arg1, [arg2 ...]] -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at that index must be of type CONSTANT_StaticMethodref (§6.8.3
 *   CONSTANT_StaticFieldref and CONSTANT_StaticMethodref), a reference to a
 *   static method. The method must not be <init>, an instance
 *   initialization method, or <clinit>, a class or interface initialization
 *   method. It must be static, and therefore cannot be abstract.
 *
 *   The resolved method includes the code for the method, an unsigned byte
 *   nargs that may be zero, and the method's modifier information.
 *
 *   The operand stack must contain nargs words of arguments, where the
 *   number of words of arguments and the type and order of the values they
 *   represent must be consistent with those of the resolved method.
 *
 *   The nargs words of arguments are popped from the operand stack. A new
 *   stack frame is created for the method being invoked, and the words of
 *   arguments are made the values of its first nargs words of local
 *   variables, with arg1 in local variable 0, arg2 in local variable 1, and
 *   so on. The new stack frame is then made current, and the Java Card
 *   virtual machine pc is set to the opcode of the first instruction of the
 *   method to be invoked. Execution continues with the first instruction of
 *   the method.
 *
 */
void Bytecodes::bc_invokestatic() {
  Context &context = this->context;
  Stack &stack = context.getStack();
  ConstantPool_Handler cp_handler(context.getCurrentPackage());
  Method_Handler method_handler(context);
  Class_Handler class_handler(context.getCurrentPackage());

  uint16_t method_offset = 0;
  bool intercept_shareable_lookup = false;
  uint8_t intercept_util_non_atomic = 0;
  pc_t &pc = stack.getPC();

  uint16_t index = pc.getNextShort();

  TRACE_JCVM_DEBUG("INVOKESTATIC 0x%04X", index);

  auto cp_entry = cp_handler.getCPEntry(index);

  try {
  if (cp_entry.tag != JC_CP_TAG_CONSTANT_STATICMETHODREF) {
    throw Exceptions::SecurityException;
  }

  jc_cap_static_method_ref_info method_ref =
      cp_entry.info.static_method_ref_info;

  if (IS_CP_INTERNAL_REF(method_ref.static_method_ref)) {
    TRACE_JCVM_DEBUG("INVOKESTATIC internal offset=%u handler_size=%u",
                     static_cast<unsigned>(method_ref.static_method_ref.internal_ref.offset),
                     static_cast<unsigned>(sizeof(uint8_t)));
    method_offset = HTONS(method_ref.static_method_ref.internal_ref.offset);
  } else { // Is external static method ref
    TRACE_JCVM_DEBUG("INVOKESTATIC external package=%u class=%u method=%u",
                     static_cast<unsigned>(method_ref.static_method_ref.external_ref.package_token),
                     static_cast<unsigned>(method_ref.static_method_ref.external_ref.class_token),
                     static_cast<unsigned>(method_ref.static_method_ref.external_ref.token));
    Import_Handler imported(context.getCurrentPackage());
    const jc_cap_package_info *package_aid = imported.getPackageAID(
        method_ref.static_method_ref.external_ref.package_token & 0x7F);

    Package exported_package(imported.getPackageIndex(package_aid));
    TRACE_JCVM_DEBUG("INVOKESTATIC target package=%u",
                     static_cast<unsigned>(exported_package.getPackageID()));
    Export_Handler export_handler(exported_package);

    const uint8_t imported_class_token =
        method_ref.static_method_ref.external_ref.class_token;
    const bool use_jc305_compatibility_tokens =
        FlashMemory_Handler::isDynamicPackageID(
            context.getCurrentPackageID());
    const uint8_t class_token =
        use_jc305_compatibility_tokens
            ? remap_imported_class_token(*package_aid, imported_class_token)
            : imported_class_token;
    const uint8_t method_token =
        use_jc305_compatibility_tokens
            ? remap_imported_static_method_token(
                  *package_aid, imported_class_token,
                  method_ref.static_method_ref.external_ref.token)
            : method_ref.static_method_ref.external_ref.token;
    intercept_shareable_lookup =
        package_aid->AID_length == 7 &&
        std::memcmp(package_aid->AID,
                    "\xA0\x00\x00\x00\x60\x00\x01", 7) == 0 &&
        class_token == 0 && method_token == 53;
    // Util's non-atomic methods are implemented by the ROM API bytecode in
    // ordinary builds. Intercept them here so invalid ranges are checked in
    // full before any destination element can be changed.
    if (package_aid->AID_length == 7 &&
        std::memcmp(package_aid->AID,
                    "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
        class_token == ROM_CLASS_EXPORT_TOKEN_JAVACARD_FRAMEWORK_UTIL) {
      if (method_token == 1)
        intercept_util_non_atomic = 1; // arrayCopyNonAtomic
      if (method_token == 3)
        intercept_util_non_atomic = 2; // arrayFillNonAtomic
    }
    bool trace_security_call = false;
#ifndef PC_VERSION
    trace_security_call =
        use_jc305_compatibility_tokens && package_aid->AID_length == 7 &&
        std::memcmp(package_aid->AID,
                    "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0;
#endif
    if (trace_security_call) {
      static uint8_t security_call_trace_budget = 0;
      if (security_call_trace_budget < 24) {
        const uint16_t exported_class_offset =
            export_handler.getExportedClassOffset(class_token);
        method_offset = export_handler.getExportedStaticMethodOffset(
            class_token, method_token);
        Method_Handler exported_method_handler(context);
        exported_method_handler.setPackage(exported_package);
        const uint8_t argument_count =
            exported_method_handler.getArgumentCount(method_offset);
        const auto *operand_top = stack.getCurrentFrame().getTOS();
        const uint16_t first_argument = argument_count == 0
                                           ? 0xFFFF
                                           : static_cast<uint16_t>(
                                                 operand_top[-argument_count]);
        std::printf(
            "CHOUPI:JC305_SECURITY_CALL raw_class=%u mapped_class=%u "
            "class_offset=%u raw_method=%u mapped_method=%u offset=%u "
            "arg_count=%u arg0=%04X\r\n",
            static_cast<unsigned>(imported_class_token),
            static_cast<unsigned>(class_token),
            static_cast<unsigned>(exported_class_offset),
            static_cast<unsigned>(method_ref.static_method_ref.external_ref.token),
            static_cast<unsigned>(method_token),
            static_cast<unsigned>(method_offset),
            static_cast<unsigned>(argument_count),
            static_cast<unsigned>(first_argument));
        std::fflush(stdout);
        if (imported_class_token == 14 &&
            method_ref.static_method_ref.external_ref.token == 0) {
          for (uint8_t candidate = 25; candidate <= 31; ++candidate) {
            try {
              const uint16_t candidate_offset =
                  export_handler.getExportedStaticMethodOffset(candidate, 0);
              const uint8_t candidate_nargs =
                  exported_method_handler.getArgumentCount(candidate_offset);
              std::printf(
                  "CHOUPI:JC305_SECURITY_CANDIDATE class=%u offset=%u "
                  "nargs=%u\r\n",
                  static_cast<unsigned>(candidate),
                  static_cast<unsigned>(candidate_offset),
                  static_cast<unsigned>(candidate_nargs));
              std::fflush(stdout);
            } catch (...) {
            }
          }
        }
        ++security_call_trace_budget;
      } else {
        method_offset = export_handler.getExportedStaticMethodOffset(
            class_token, method_token);
      }
    } else {
    method_offset = export_handler.getExportedStaticMethodOffset(
        class_token, method_token);
    }
    TRACE_JCVM_DEBUG("INVOKESTATIC method offset=%u",
                     static_cast<unsigned>(method_offset));

    method_handler.setPackage(exported_package);
  }

  // Calling the method
  if (intercept_shareable_lookup) {
    constexpr uint8_t expected_arguments = 2;
    context.resetSensitiveResult();
    if (method_handler.getArgumentCount(method_offset) !=
        expected_arguments) {
      throw Exceptions::SecurityException;
    }
    const jref_t client_aid =
        stack.get_Pushed_Reference(expected_arguments);
    const int8_t parameter = static_cast<int8_t>(stack.pop_Byte());
    (void)stack.pop_Reference();
    const jref_t shareable = choupi_vm_get_applet_shareable_interface_object(
        context, client_aid, parameter);
    context.setSensitiveResultObject(shareable);
    stack.push_Reference(shareable);
    return;
  }
  if (intercept_util_non_atomic != 0) {
    Context::getActiveContext().resetSensitiveResult();
    const uint8_t expected_arguments =
        intercept_util_non_atomic == 1 ? 5 : 4;
    if (method_handler.getArgumentCount(method_offset) != expected_arguments) {
      throw Exceptions::SecurityException;
    }
    if (intercept_util_non_atomic == 1) {
      const jshort_t length = stack.pop_Short();
      const jshort_t destination_offset = stack.pop_Short();
      const jref_t destination_ref = stack.pop_Reference();
      const jshort_t source_offset = stack.pop_Short();
      const jref_t source_ref = stack.pop_Reference();
      if (source_ref.isNullPointer() || destination_ref.isNullPointer()) {
        throw Exceptions::NullPointerException;
      }
      auto &heap = Context::getActiveContext().getHeap();
      auto source = heap.getArray(source_ref);
      auto destination = heap.getArray(destination_ref);
      if (source->getType() != JAVA_ARRAY_T_BYTE ||
          destination->getType() != JAVA_ARRAY_T_BYTE) {
        throw Exceptions::ArrayStoreException;
      }
      const auto check_range = [length](const JC_Array &array,
                                        const jshort_t offset) {
        if (offset < 0 || length < 0 ||
            static_cast<uint32_t>(offset) +
                    static_cast<uint32_t>(length) > array.size()) {
          throw Exceptions::ArrayIndexOutOfBoundsException;
        }
      };
      check_range(*source, source_offset);
      check_range(*destination, destination_offset);

      if (source == destination && destination_offset > source_offset &&
          static_cast<uint32_t>(destination_offset) <
              static_cast<uint32_t>(source_offset) +
                  static_cast<uint32_t>(length)) {
        for (jshort_t index = length; index > 0; --index) {
          const uint16_t source_index = static_cast<uint16_t>(
              source_offset + index - 1);
          const uint16_t destination_index = static_cast<uint16_t>(
              destination_offset + index - 1);
          destination->setByteEntryNonAtomic(
              destination_index, source->getByteEntry(source_index));
        }
      } else {
        for (jshort_t index = 0; index < length; ++index) {
          const uint16_t source_index =
              static_cast<uint16_t>(source_offset + index);
          const uint16_t destination_index =
              static_cast<uint16_t>(destination_offset + index);
          destination->setByteEntryNonAtomic(
              destination_index, source->getByteEntry(source_index));
        }
      }
      const jshort_t result = static_cast<jshort_t>(
          static_cast<uint16_t>(destination_offset + length));
      Context::getActiveContext().setSensitiveResultShort(result);
      stack.push_Short(result);
      return;
    }

    const jbyte_t value = stack.pop_Byte();
    const jshort_t length = stack.pop_Short();
    const jshort_t offset = stack.pop_Short();
    const jref_t array_ref = stack.pop_Reference();
    if (array_ref.isNullPointer()) throw Exceptions::NullPointerException;
    auto array = Context::getActiveContext().getHeap().getArray(array_ref);
    if (array->getType() != JAVA_ARRAY_T_BYTE) {
      throw Exceptions::ArrayStoreException;
    }
    if (offset < 0 || length < 0 ||
        static_cast<uint32_t>(offset) + static_cast<uint32_t>(length) >
            array->size()) {
      throw Exceptions::ArrayIndexOutOfBoundsException;
    }
    for (jshort_t index = 0; index < length; ++index) {
      array->setByteEntryNonAtomic(static_cast<uint16_t>(offset + index),
                                   value);
    }
    const jshort_t result = static_cast<jshort_t>(
        static_cast<uint16_t>(offset + length));
    Context::getActiveContext().setSensitiveResultShort(result);
    stack.push_Short(result);
    return;
  }
  method_handler.callStaticMethod(method_offset);

  } catch (Exceptions) {
    throw;
  }

  return;
}

/**
 * Invoke interface method
 *
 * Format:
 *   invokeinterface
 *   nargs
 *   indexbyte1
 *   indexbyte2
 *   method
 *
 * Forms:
 *   invokeinterface = 142 (0x8e)
 *
 * Stack:
 *   ..., objectref, [arg1, [arg2 ...]] -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at that index must be of type CONSTANT_Classref (§6.8.1
 *   CONSTANT_Classref), a reference to an interface class. The specified
 *   interface is resolved.
 *
 *   The nargs operand is an unsigned byte that must not be zero.
 *
 *   The method operand is an unsigned byte that is the interface method
 *   token for the method to be invoked. The interface method must not be
 *   <init> or an instance initialization method.
 *
 *   The object-ref must be of type reference and must be followed on the
 *   operand stack by nargs - 1 words of arguments. The number of words of
 *   arguments and the type and order of the values they represent must be
 *   consistent with those of the selected interface method.
 *
 *   The interface table of the class of the type of objectref is
 *   determined. If objectref is an array type, then the interface table of
 *   class Object (§2.2.1.4 Unsupported Classes) is used. The interface
 *   table is searched for the resolved interface. The result of the search
 *   is a table that is used to map the method token to a index.
 *
 *   The index is an unsigned byte that is used as an index into the method
 *   table of the class of the type of objectref. If the objectref is an
 *   array type, then the method table of class Object is used. The table
 *   entry at that index includes a direct reference to the method's code
 *   and modifier information.
 *
 *   The nargs - 1 words of arguments and objectref are popped from the
 *   operand stack. A new stack frame is created for the method being
 *   invoked, and objectref and the arguments are made the values of its
 *   first nargs words of local variables, with objectref in local variable
 *   0, arg1 in local variable 1, and so on. The new stack frame is then
 *   made current, and the Java Card virtual machine pc is set to the opcode
 *   of the first instruction of the method to be invoked. Execution
 *   continues with the first instruction of the method.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the invokeinterface instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the invokeinterface instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   context (§3.4 Contexts) of the object referenced by objectref. The
 *   exact circumstances when the exception will be thrown are specified in
 *   Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition. If the current context is not the
 *   object's context and the Java Card RE permits invocation of the method,
 *   the invokeinterface instruction will cause a context switch (§3.4
 *   Contexts) to the object's context before invoking the method, and will
 *   cause a return context switch to the previous context when the invoked
 *   method returns.
 *
 */
void Bytecodes::bc_invokeinterface() {
  Context &context = this->context;
  Stack &stack = context.getStack();
  ConstantPool_Handler cp_handler(context.getCurrentPackage());
  Method_Handler method_handler(context);
  Class_Handler class_handler(context.getCurrentPackage());

  pc_t &pc = stack.getPC();

  TRACE_JCVM_DEBUG("INVOKEINTERFACE");

  uint8_t nargs = pc.getNextByte();
  uint16_t index = pc.getNextShort();
  uint8_t method = pc.getNextByte();

  if (nargs == 0) {
    throw Exceptions::SecurityException;
  }

  auto interface_ref = cp_handler.getClassRef(index);
  uint8_t interface_method = method;
  // Embedded ROM API bytecode already uses the embedded export's interface
  // method order. Translate imported method tokens only when the caller is a
  // JC305-compatible uploaded CAP, just as class and static method refs do.
  if (interface_ref.isExternalClassRef() &&
      usesJC305CompatibilityTokens(context.getCurrentPackage())) {
    Import_Handler import_handler(context.getCurrentPackage());
    const auto *package_info = import_handler.getPackageAID(
        interface_ref.external_classref.package_token & 0x7F);
    interface_method = remap_imported_interface_method_token(
        *package_info, interface_ref.external_classref.class_token, method);
  }

  // nargs-th arguments should be popped to access the objectref.
  jref_t objectref = stack.get_Pushed_Reference(nargs);

  if (objectref.isNullPointer()) {
    const auto &frame = stack.getCurrentFrame();
    const auto *method_component =
        context.getCurrentPackage().getCap().getMethod();
    const auto methods = method_component->methods();
    const auto *instruction = frame.getInstructionPC();
    const auto *method_start = frame.getMethodStartPC();
    std::printf(
        "CHOUPI:NULL_RECEIVER op=INVOKEINTERFACE package=%u pc=%u "
        "method_pc=%u cp_index=%u nargs=%u interface_method=%u "
        "receiver=%04X\r\n",
        static_cast<unsigned>(context.getCurrentPackageID()),
        instruction >= methods.data() &&
                instruction < methods.data() + methods.size()
            ? static_cast<unsigned>(instruction - methods.data())
            : 0xFFFFFFFFu,
        instruction >= method_start
            ? static_cast<unsigned>(instruction - method_start)
            : 0xFFFFFFFFu,
        static_cast<unsigned>(index), static_cast<unsigned>(nargs),
        static_cast<unsigned>(method),
        static_cast<unsigned>(objectref.compact()));
    std::fflush(stdout);
    throw Exceptions::NullPointerException;
  }

  // A Shareable proxy is only a client-side handle. It must be invoked via
  // its declared Shareable interface in the server applet's context; never
  // interpret the placeholder object's class or fields in the client heap.
  if (context.isShareableProxy(objectref)) {
    TRACE_JCVM_DEBUG("SIO_INVOKE proxy=%04X nargs=%u method=%u",
                     static_cast<unsigned>(objectref.compact()),
                     static_cast<unsigned>(nargs),
                     static_cast<unsigned>(interface_method));
    auto interface_info =
        cp_handler.classref2interface(interface_ref);
    if (interface_info.second == nullptr ||
        IS_CLASS(reinterpret_cast<const uint8_t *>(interface_info.second))) {
      throw Exceptions::SecurityException;
    }
    TRACE_JCVM_DEBUG("SIO_INVOKE interface flags=%02X token=%u",
                     static_cast<unsigned>(interface_info.second->flags),
                     static_cast<unsigned>(interface_method));
    if ((interface_info.second->flags & JC_CAP_CLASS_ACC_SHAREABLE) == 0) {
      throw Exceptions::SecurityException;
    }

    uint8_t server_aid[16]{};
    uint8_t server_aid_length = 0;
    jref_t server_reference;
    if (!context.getShareableProxy(objectref, server_aid,
                                   server_aid_length, server_reference)) {
      TRACE_JCVM_DEBUG("SIO_INVOKE proxy metadata missing");
      throw Exceptions::SecurityException;
    }
    uint8_t client_aid[16]{};
    uint8_t client_aid_length = 0;
    if (!context.copyAppletAID(client_aid, client_aid_length)) {
      throw Exceptions::SecurityException;
    }
    const uint8_t client_owner = static_cast<uint8_t>(context.getAppletID());
    bool client_state_restored = false;
    const auto restore_client_state = [&]() {
      if (client_state_restored) return true;
      if (!context.activateAppletInstance(client_owner, client_aid,
                                          client_aid_length)) {
        return false;
      }
      client_state_restored = true;
      return true;
    };
    struct ClientStateRestore {
      decltype(restore_client_state) &restore;
      ~ClientStateRestore() { (void)this->restore(); }
    } client_state_restore{restore_client_state};
    TRACE_JCVM_DEBUG("SIO_INVOKE target AID len=%u %02X%02X%02X%02X%02X%02X%02X%02X%02X ref=%04X",
                     static_cast<unsigned>(server_aid_length), server_aid[0],
                     server_aid[1], server_aid[2], server_aid[3], server_aid[4],
                     server_aid[5], server_aid[6], server_aid[7], server_aid[8],
                     static_cast<unsigned>(server_reference.compact()));
    Context *server_context =
        choupi_vm_find_or_restore_applet_context(server_aid,
                                                 server_aid_length);
    if (server_context == nullptr) {
      TRACE_JCVM_DEBUG("SIO_INVOKE target context missing");
      throw Exceptions::SecurityException;
    }
    TRACE_JCVM_DEBUG("SIO_INVOKE client_context=%u server_context=%u",
                     static_cast<unsigned>(context.getAppletID()),
                     static_cast<unsigned>(server_context->getAppletID()));
    const auto server_object =
        server_context->getHeap().getInstance(server_reference);
    if (server_object == nullptr) {
      throw Exceptions::SecurityException;
    }
    Package server_package(server_object->getPackageID());
    ConstantPool_Handler server_cp(server_package);
    const auto server_class_ref = server_cp.getClassRefFromClassIndex(
        server_object->getClassIndex());
    const auto target_method =
        Class_Handler(server_package).getImplementedInterfaceMethodOffset(
            server_class_ref, interface_ref, interface_method, false,
            context.getCurrentPackage());
    TRACE_JCVM_DEBUG("SIO_INVOKE target method package=%u offset=%u nargs=%u",
                     static_cast<unsigned>(target_method.first.getPackageID()),
                     static_cast<unsigned>(target_method.second),
                     static_cast<unsigned>(nargs));
    Method_Handler target_handler(*server_context);
    target_handler.setPackage(target_method.first);
    if (target_handler.getArgumentCount(target_method.second) != nargs) {
      throw Exceptions::SecurityException;
    }

    if (server_context == &context) {
      // Same-context calls already have all typed operand words in the
      // caller frame. Replace only the proxy receiver and use normal VM
      // dispatch, preserving primitive arguments without copying them.
      stack.set_Pushed_Element(nargs, server_reference);
      target_handler.callVirtualMethod(target_method.second, true);
      return;
    }

    // Cross-context arguments are copied according to the CAP interface
    // descriptor. Primitive values can safely cross the heap boundary. Object
    // references are accepted only when null until the firewall can translate
    // shareable references back to their owning context.
    std::array<uint8_t, 128> signature_nibbles{};
    uint8_t signature_nibble_count = 0;
    std::array<InterpretorInitialLocal, 255> initial_locals{};
    uint8_t initial_local_count = 0;
    const auto signature_status = getShareableMethodSignature(
        interface_info.first, interface_info.second, interface_method,
        signature_nibbles, signature_nibble_count);
    if (signature_status == ShareableSignatureStatus::Found) {
      if (!buildShareableInitialLocals(
              signature_nibbles.data(), signature_nibble_count, nargs,
              server_reference, stack, initial_locals, initial_local_count,
              context.getCurrentPackageID())) {
        TRACE_JCVM_DEBUG("SIO_INVOKE argument marshalling failed nibbles=%u",
                         static_cast<unsigned>(signature_nibble_count));
        throw Exceptions::SecurityException;
      }
    } else if (signature_status !=
                   ShareableSignatureStatus::DescriptorUnavailable ||
               !buildShareableTaggedInitialLocals(
                   nargs, server_reference, stack, initial_locals,
                   initial_local_count, context.getCurrentPackageID())) {
      // Missing type tags or malformed reference provenance fail closed. The
      // method header above has verified the target's argument word count.
      TRACE_JCVM_DEBUG("SIO_INVOKE descriptor-free argument tags rejected");
      throw Exceptions::SecurityException;
    }
    TRACE_JCVM_DEBUG("SIO_INVOKE marshalled locals=%u",
                     static_cast<unsigned>(initial_local_count));

    // Pop all caller words only after validation and copying succeeded.
    for (uint16_t word = 0; word < nargs; ++word) {
      stack.pop();
    }
    Interpretor server_interpretor(
        *server_context, target_method.second,
        target_method.first.getPackageID(), initial_locals.data(),
        initial_local_count, true);
    const uint16_t status = server_interpretor.run();
    if (const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
        trace_setting != nullptr && std::strcmp(trace_setting, "1") == 0) {
      std::fprintf(stderr,
                   "CHOUPI:SIO_SERVER_RESULT status=%04X uncaught_ref=%04X "
                   "uncaught_owner=%u server_owner=%u\n",
                   static_cast<unsigned>(status),
                   static_cast<unsigned>(
                       server_interpretor.lastUncaughtExceptionReference()),
                   static_cast<unsigned>(
                       server_interpretor.lastUncaughtExceptionOwner()),
                   static_cast<unsigned>(server_context->getAppletID()));
    }
    if (!restore_client_state()) {
      TRACE_JCVM_DEBUG("SIO_INVOKE caller context restoration failed");
      throw Exceptions::SecurityException;
    }
    TRACE_JCVM_DEBUG("SIO_INVOKE server status=%04X return_kind=%u",
                     static_cast<unsigned>(status),
                     static_cast<unsigned>(
                         server_context->getStack().getRootReturnKind()));
    if (status != 0x9000) {
      const uint16_t uncaught_reference =
          server_interpretor.lastUncaughtExceptionReference();
      if (status == 0x6f00 && uncaught_reference != 0) {
        // The server's compact reference is meaningful only in its heap.
        // Copy the exception object into the caller heap, retaining its class
        // and scalar reason fields while refusing private object-graph edges.
        const uint8_t exception_owner =
            server_interpretor.lastUncaughtExceptionOwner();
        if (exception_owner != Stack::kGlobalReferenceOwner &&
            exception_owner !=
                static_cast<uint8_t>(server_context->getAppletID())) {
          throw Exceptions::SecurityException;
        }
        const jref_t caller_exception = copySioExceptionToCaller(
            context, *server_context, uncaught_reference);
        this->doThrow(caller_exception);
        return;
      }
      throw Exceptions::SecurityException;
    }

    Stack &server_stack = server_context->getStack();
    switch (server_stack.getRootReturnKind()) {
    case Stack::ReturnKind::Void:
      break;
    case Stack::ReturnKind::Short:
      stack.push_Short(static_cast<jshort_t>(server_stack.getRootReturnValue()));
      break;
    case Stack::ReturnKind::Int:
#ifdef JCVM_INT_SUPPORTED
      stack.push_Int(static_cast<jint_t>(server_stack.getRootReturnValue()));
#else
      throw Exceptions::SecurityException;
#endif
      break;
    case Stack::ReturnKind::Reference: {
      Stack::ReferenceValue returned_reference{};
      if (!server_stack.getRootReferenceReturnValue(returned_reference)) {
        throw Exceptions::SecurityException;
      }
      if (!returned_reference.reference.isNullPointer() &&
          returned_reference.owner ==
              static_cast<uint8_t>(context.getCurrentPackageID())) {
        // The server returned a reference supplied by this caller. Restore its
        // local marker only after returning to the reference's owner context.
        stack.push_Reference(returned_reference.reference,
                             Stack::kCurrentReferenceOwner);
      } else if (!returned_reference.reference.isNullPointer() &&
                 returned_reference.owner ==
                     Stack::kCurrentReferenceOwner) {
        stack.push_Reference(wrapShareableReference(
            context, *server_context, server_aid, server_aid_length,
            returned_reference.reference));
      } else {
        // Global references and opaque references owned by other applets keep
        // their provenance while crossing back to the original caller.
        stack.push_Reference(returned_reference.reference,
                             returned_reference.owner);
      }
      break;
    }
    case Stack::ReturnKind::None:
      throw Exceptions::SecurityException;
    }
    return;
  }

  if (context.isStaleShareableProxy(objectref)) {
    TRACE_JCVM_DEBUG("SIO_INVOKE rejecting unregistered foreign object");
    throw Exceptions::SecurityException;
  }

  if (objectref.isArray()) { // Is an array
    // class variable must point to Object class.
    jref_t thisref = stack.readLocal_Reference((uint8_t)0);
    auto instance = context.getHeap().getInstance(thisref);
    auto classref = ConstantPool_Handler(instance->getPackageID())
                        .getClassRefFromClassIndex(instance->getClassIndex());
    auto method_offset = class_handler.getImplementedInterfaceMethodOffset(
        classref, interface_ref, interface_method, true,
        context.getCurrentPackage());
    // Calling the method
    method_handler.setPackage(method_offset.first);
    method_handler.callVirtualMethod(method_offset.second);

  } else { // is an interface
    auto instance = context.getHeap().getInstance(objectref);

    auto classref = ConstantPool_Handler(instance->getPackageID())
                        .getClassRefFromClassIndex(instance->getClassIndex());

    // classref is encoded in the receiver object's package, not the current
    // caller's package. Resolve its imported superclass/interface references
    // with that package's import table before dispatching the interface call.
    Class_Handler receiver_class_handler(Package(instance->getPackageID()));
    auto method_offset = receiver_class_handler
                             .getImplementedInterfaceMethodOffset(
                                 classref, interface_ref, interface_method,
                                 false, context.getCurrentPackage());
    const auto sensitive_return =
        pinSensitiveReturn(context, interface_ref, interface_method);
    if (sensitive_return != Frame::SensitiveReturn::None) {
      context.resetSensitiveResult();
    }
    // Calling the method
    method_handler.setPackage(method_offset.first);
    method_handler.callVirtualMethod(method_offset.second);
    if (sensitive_return != Frame::SensitiveReturn::None) {
      stack.getCurrentFrame().setSensitiveReturn(sensitive_return);
    }
  }

  return;
}

} // namespace jcvm
