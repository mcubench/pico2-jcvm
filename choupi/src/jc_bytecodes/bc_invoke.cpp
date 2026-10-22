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
#include "../stack.hpp"
#include "bytecodes.hpp"

#include <cstring>
#include <cstdio>

namespace jcvm {

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
  const jref_t receiver = stack.get_Pushed_Element(nargs);
  if (receiver.isNullPointer()) {
    throw Exceptions::NullPointerException;
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
  // Calling the method and updating PC value
  method_handler.setPackage(target_package);
  method_handler.callVirtualMethod(target_method_offset);

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
  const jref_t receiver = stack.get_Pushed_Element(nargs);
  if (receiver.isNullPointer()) {
    throw Exceptions::NullPointerException;
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
        std::printf(
            "CHOUPI:JC305_SECURITY_CALL raw_class=%u mapped_class=%u "
            "class_offset=%u raw_method=%u mapped_method=%u offset=%u\r\n",
            static_cast<unsigned>(imported_class_token),
            static_cast<unsigned>(class_token),
            static_cast<unsigned>(exported_class_offset),
            static_cast<unsigned>(method_ref.static_method_ref.external_ref.token),
            static_cast<unsigned>(method_token),
            static_cast<unsigned>(method_offset));
        std::fflush(stdout);
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
  if (interface_ref.isExternalClassRef()) {
    Import_Handler import_handler(context.getCurrentPackage());
    const auto *package_info = import_handler.getPackageAID(
        interface_ref.external_classref.package_token & 0x7F);
    interface_method = remap_imported_interface_method_token(
        *package_info, interface_ref.external_classref.class_token, method);
  }

  // nargs-th arguments should be popped to access the objectref.
  jref_t objectref = stack.get_Pushed_Element(nargs);

  if (objectref.isNullPointer()) {
    throw Exceptions::NullPointerException;
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
    // Calling the method
    method_handler.setPackage(method_offset.first);
    method_handler.callVirtualMethod(method_offset.second);
  }

  return;
}

} // namespace jcvm
