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
#include "../exceptions.hpp"
#include "../heap.hpp"
#include "../jc_handlers/flashmemory.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#include "../stack.hpp"
#include "bytecodes.hpp"

#include <cstdio>

namespace jcvm {

namespace {
void rejectShareableProxyFieldAccess(Context &context, const jref_t reference) {
  if (context.isShareableProxy(reference) ||
      context.isStaleShareableProxy(reference)) {
    throw Exceptions::SecurityException;
  }
}

uint16_t readDescriptorU16(const uint8_t *data) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               data[1]);
}

void validateDeclaredInstanceType(
    const std::pair<Package, const jc_cap_class_info *> &declaring_class,
    const uint8_t token, const FieldType expected) {
  if (!FlashMemory_Handler::isDynamicPackageID(
          declaring_class.first.getPackageID())) {
    return;
  }
  const auto cap = declaring_class.first.getCap();
  const auto *descriptor = cap.getDescriptor();
  if (descriptor == nullptr) return;

  const auto *class_component = cap.getClass();
  if (class_component == nullptr) throw Exceptions::SecurityException;
  const uintptr_t class_data =
      reinterpret_cast<uintptr_t>(class_component->infos);
  const uintptr_t class_address =
      reinterpret_cast<uintptr_t>(declaring_class.second);
  if (class_address < class_data || class_address - class_data > UINT16_MAX) {
    throw Exceptions::SecurityException;
  }
  const uint16_t class_offset =
      static_cast<uint16_t>(class_address - class_data);

  const auto *payload =
      reinterpret_cast<const uint8_t *>(&descriptor->class_count);
  const size_t payload_size = NTOHS(descriptor->size);
  if (payload_size == 0) throw Exceptions::SecurityException;
  size_t offset = 1;
  bool found = false;
  FieldType declared = FieldType::FIELD_TYPE_UNINITIALIZED;
  for (uint8_t class_index = 0; class_index < descriptor->class_count;
       ++class_index) {
    if (offset > payload_size || payload_size - offset < 9) {
      throw Exceptions::SecurityException;
    }
    const uint8_t *class_info = payload + offset;
    const size_t interfaces = class_info[4];
    const size_t fields = readDescriptorU16(class_info + 5);
    const size_t methods = readDescriptorU16(class_info + 7);
    const size_t fields_offset = 9 + interfaces * 2;
    const size_t record_size = fields_offset + fields * 7 + methods * 12;
    if (record_size > payload_size - offset) {
      throw Exceptions::SecurityException;
    }
    if (readDescriptorU16(class_info + 2) != class_offset) {
      offset += record_size;
      continue;
    }

    for (size_t field_index = 0; field_index < fields; ++field_index) {
      const uint8_t *field = class_info + fields_offset + field_index * 7;
      constexpr uint8_t kStaticAccessFlag = 0x08;
      if ((field[1] & kStaticAccessFlag) != 0 ||
          readDescriptorU16(field + 2) != class_offset || field[4] != token) {
        continue;
      }
      if (found) throw Exceptions::SecurityException;
      found = true;
      const uint16_t type = readDescriptorU16(field + 5);
      if ((type & 0x8000) == 0) {
        declared = FieldType::FIELD_TYPE_OBJECT;
      } else {
        switch (type & 0x7fff) {
        case 2:
          declared = FieldType::FIELD_TYPE_BOOLEAN;
          break;
        case 3:
          declared = FieldType::FIELD_TYPE_BYTE;
          break;
        case 4:
          declared = FieldType::FIELD_TYPE_SHORT;
          break;
        case 5:
          declared = FieldType::FIELD_TYPE_INT;
          break;
        default:
          throw Exceptions::SecurityException;
        }
      }
    }
    offset += record_size;
  }

  const bool byte_boolean = expected == FieldType::FIELD_TYPE_BYTE &&
      declared == FieldType::FIELD_TYPE_BOOLEAN;
  if (!found || (declared != expected && !byte_boolean)) {
    throw Exceptions::SecurityException;
  }
}

uint16_t resolveInstanceFieldToken(Context &context, const uint16_t cp_index,
                                   const FieldType expected,
                                   const JC_Instance &receiver) {
  ConstantPool_Handler constant_pool(context.getCurrentPackage());
  const jc_cap_instance_field_ref_info field_ref =
      constant_pool.getInstanceFieldRef(cp_index);
  uint16_t token = field_ref.token;

  // Instance field tokens are local to the class that declares the field.
  // The heap stores the complete inheritance chain in one flat field array,
  // so add the declared field count of each superclass before indexing it.
  const auto declaring_class = constant_pool.classref2class(field_ref.class_ref);
  validateDeclaredInstanceType(declaring_class, field_ref.token, expected);

  // A field token is meaningful only for instances whose concrete class is
  // the declaring class or one of its subclasses. Without this check, a CAP
  // can use a valid fieldref against an unrelated same-layout object.
  ConstantPool_Handler receiver_cp(Package(receiver.getPackageID()));
  auto receiver_class = receiver_cp.classref2class(
      receiver_cp.getClassRefFromClassIndex(receiver.getClassIndex()));
  bool assignable = false;
  for (size_t depth = 0; depth < 256; ++depth) {
    if (receiver_class.first == declaring_class.first &&
        receiver_class.second == declaring_class.second) {
      assignable = true;
      break;
    }
    if (receiver_class.second->isObjectClass()) break;
    ConstantPool_Handler superclass_cp(receiver_class.first);
    receiver_class = superclass_cp.classref2class(
        receiver_class.second->super_class_ref);
  }
  if (!assignable) throw Exceptions::SecurityException;
  ConstantPool_Handler declaring_cp(declaring_class.first);
  auto superclass = declaring_cp.classref2class(
      declaring_class.second->super_class_ref);
  for (uint16_t depth = 0; depth < 256; ++depth) {
    if (superclass.second == nullptr) throw Exceptions::SecurityException;
    if (superclass.second->isObjectClass()) return token;
    const uint32_t adjusted_token = static_cast<uint32_t>(token) +
        superclass.second->declared_instance_size;
    if (adjusted_token > UINT16_MAX) throw Exceptions::SecurityException;
    token = static_cast<uint16_t>(adjusted_token);
    ConstantPool_Handler superclass_cp(superclass.first);
    superclass = superclass_cp.classref2class(
        superclass.second->super_class_ref);
  }
  throw Exceptions::SecurityException;
}
} // namespace

/**
 * Fetch reference from object
 *
 * Format:
 *   getfield_a
 *   index
 *
 * Forms:
 *   getfield_a = 131 (0x83)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned index is used as an index into the constant
 *   pool of the current package (§3.5 Frames). The constant pool item at
 *   the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset 17 . The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_a instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_a instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_a() {
  jref_t objectref;
  uint8_t index;
  jref_t ref;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_A 0x%02X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);
  if (objectref.isNullPointer()) {
    const auto &frame = stack.getCurrentFrame();
    const auto *method_component =
        this->context.getCurrentPackage().getCap().getMethod();
    const auto methods = method_component->methods();
    const auto *instruction = frame.getInstructionPC();
    const auto *method_start = frame.getMethodStartPC();
    const auto local_zero = stack.readLocal_Reference(0);
    std::printf(
        "CHOUPI:NULL_RECEIVER op=GETFIELD_A package=%u pc=%u method_pc=%u "
        "cp_index=%u receiver=%04X local0=%04X\r\n",
        static_cast<unsigned>(this->context.getCurrentPackageID()),
        instruction >= methods.data() &&
                instruction < methods.data() + methods.size()
            ? static_cast<unsigned>(instruction - methods.data())
            : 0xFFFFFFFFu,
        instruction >= method_start
            ? static_cast<unsigned>(instruction - method_start)
            : 0xFFFFFFFFu,
        static_cast<unsigned>(index),
        static_cast<unsigned>(objectref.compact()),
        static_cast<unsigned>(local_zero.compact()));
    std::fflush(stdout);
    throw Exceptions::NullPointerException;
  }
  auto instance = heap.getInstance(objectref);
  const auto field_token = resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance);
  ref = instance->getField_Reference(field_token);
  TRACE_JCVM_DEBUG("GETFIELD_A value=0x%04X",
                   static_cast<unsigned>(ref.compact()));
  stack.push_Reference(ref);

  return;
}

/**
 * Fetch byte from object
 *
 * Format:
 *   getfield_b
 *   index
 *
 * Forms:
 *   getfield_b = 132 (0x84)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned index is used as an index into the constant
 *   pool of the current package (§3.5 Frames). The constant pool item at the
 *   index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset 17 . The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_b instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_b instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_b() {
  jref_t objectref;
  uint8_t index;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_B 0x%02X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance));

  stack.push_Byte(value);

  return;
}

/**
 * Fetch short from object
 *
 * Format:
 *   getfield_s
 *   index
 *
 * Forms:
 *   getfield_b = 133 (0x85)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned index is used as an index into the constant
 *   pool of the current package (§3.5 Frames). The constant pool item at
 *   the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset 17 . The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_s instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_s instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_s() {
  jref_t objectref;
  uint8_t index;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_S 0x%02X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Short(
      resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance));

  stack.push_Short(value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Fetch int from object
 *
 * Format:
 *   getfield_i
 *   index
 *
 * Forms:
 *   getfield_i = 134 (0x86)
 *
 * Stack:
 *   ..., objectref -> ..., value.word1, value.word2
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned index is used as an index into the constant
 *   pool of the current package (§3.5 Frames). The constant pool item at the
 *   index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset 17 . The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_i instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_i instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 *   If a virtual machine does not support the int data type, the getfield_i
 *   instruction will not be available.
 */
void Bytecodes::bc_getfield_i() {
  jref_t objectref;
  uint8_t index;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_I 0x%02X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance));

  stack.push_Int(value);

  return;
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Set reference field in object
 *
 * Format:
 *   putfield_a
 *   index
 *
 * Forms:
 *   putfield_a = 135 (0x87)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[22]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   22: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_a instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_a instruction may throw a
 *   SecurityException if the current context §3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 */
void Bytecodes::bc_putfield_a() {
  jref_t objectref;
  uint8_t index;
  jref_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUTFIELD_A 0x%02X", index);

  value = stack.pop_Reference();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Reference(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance),
                               value);

  return;
}

/**
 * Set byte field in object
 *
 * Format:
 *   putfield_b
 *   index
 *
 * Forms:
 *   putfield_b = 136 (0x88)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[22]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   22: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_b instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_b instruction may throw a
 *   SecurityException if the current context §3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 */
void Bytecodes::bc_putfield_b() {
  jref_t objectref;
  uint8_t index;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUTFIELD_B 0x%02X", index);

  value = stack.pop_Byte();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance), value);

  return;
}

/**
 * Set short field in object
 *
 * Format:
 *   putfield_s
 *   index
 *
 * Forms:
 *   putfield_s = 137 (0x89)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[22]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   22: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_s instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_s instruction may throw a
 *   SecurityException if the current context §3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 */
void Bytecodes::bc_putfield_s() {
  jref_t objectref;
  uint8_t index;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUTFIELD_S 0x%02X", index);

  value = stack.pop_Short();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Short(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance), value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Set int field in object
 *
 * Format:
 *   putfield_i
 *   index
 *
 * Forms:
 *   putfield_i = 138 (0x8a)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[22]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   22: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_i instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_i instruction may throw a
 *   SecurityException if the current context §3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 *   If a virtual machine does not support the int data type, the putfield_i
 *   instruction will not be available.
 *
 */
void Bytecodes::bc_putfield_i() {
  jref_t objectref;
  uint8_t index;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUTFIELD_I 0x%02X", index);

  value = stack.pop_Int();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance), value);

  return;
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Fetch reference field from object (wide index)
 *
 * Format:
 *   getfield_a_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   getfield_a_w = 169 (0xa9)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned indexbyte1 and indexbyte2 are used to
 *   construct an index into the constant pool of the current package (§3.5
 *   Frames), where the value of the index is (indexbyte1 << 8) |
 *   indexbyte2. The constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token. The item must resolve to a field of type
 *   reference.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[19]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   19: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_b_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_a_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_a_w() {
  jref_t objectref;
  uint16_t index;
  jref_t ref;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("GETFIELD_A_W 0x%04X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  ref = instance->getField_Reference(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance));

  stack.push_Reference(ref);

  return;
}

/**
 * Fetch byte field from object (wide index)
 *
 * Format:
 *   getfield_b_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   getfield_b_w = 170 (0xaa)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned indexbyte1 and indexbyte2 are used to
 *   construct an index into the constant pool of the current package (§3.5
 *   Frames), where the value of the index is (indexbyte1 << 8) |
 *   indexbyte2. The constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token. The item must resolve to a field of type
 *   reference.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[19]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   19: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_b_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_b_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_b_w() {
  jref_t objectref;
  uint16_t index;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("GETFIELD_B_W 0x%04X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance));

  stack.push_Byte(value);

  return;
}

/**
 * Fetch short field from object (wide index)
 *
 * Format:
 *   getfield_s_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   getfield_s_w = 171 (0xab)
 *
 * Stack:
 *   ..., objectref -> ..., value
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned indexbyte1 and indexbyte2 are used to
 *   construct an index into the constant pool of the current package (§3.5
 *   Frames), where the value of the index is (indexbyte1 << 8) |
 *   indexbyte2. The constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token. The item must resolve to a field of type
 *   reference.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[19]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   19: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_s_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_s_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_s_w() {
  jref_t objectref;
  uint16_t index;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("GETFIELD_S_W 0x%04X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Short(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance));

  stack.push_Short(value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Fetch int field from object (wide index)
 *
 * Format:
 *   getfield_i_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   getfield_i_w = 172 (0xac)
 *
 * Stack:
 *   ..., objectref -> ..., value.word1, value.word2
 *
 * Description:
 *
 *   The objectref, which must be of type reference, is popped from the
 *   operand stack. The unsigned indexbyte1 and indexbyte2 are used to
 *   construct an index into the constant pool of the current package (§3.5
 *   Frames), where the value of the index is (indexbyte1 << 8) |
 *   indexbyte2. The constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token. The item must resolve to a field of type
 *   reference.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[19]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   19: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_i_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_i_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 *   If a virtual machine does not support the int data type, the
 *   getfield_i_w instruction will not be available.
 */
void Bytecodes::bc_getfield_i_w() {
  jref_t objectref;
  uint16_t index;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("GETFIELD_I_W 0x%04X", index);

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  value = instance->getField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance));

  stack.push_Int(value);

  return;
}

#endif /* JCVM_INT_SUPPORTED */

/**
 * Fetch reference field from current object
 *
 * Format:
 *   getfield_a_this
 *   index
 *
 * Forms:
 *   getfield_a_this = 173 (0xad)
 *
 * Stack:
 *   ... -> ..., value
 *
 * Description:
 *
 *   The currently executing method must be an instance method. The local
 *   variable at index 0 must contain a reference objectref to the currently
 *   executing method's this parameter. The unsigned index is used as an
 *   index into the constant pool of the current package (§3.5 Frames). The
 *   constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[18]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   18: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_a_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_a_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_a_this() {
  jref_t objectref;
  uint8_t index;
  jref_t ref;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_A_THIS 0x%02X", index);

  /*
   * To obtain the this reference, the JCVM specification says that "The
   * local variable at index 0 must contain a reference objectref to the
   * currently executing method's this parameter."
   */
  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  auto instance = heap.getInstance(objectref);
  const auto field_token = resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance);
  ref = instance->getField_Reference(field_token);
  TRACE_JCVM_DEBUG("GETFIELD_A_THIS value=0x%04X",
                   static_cast<unsigned>(ref.compact()));
  stack.push_Reference(ref);

  return;
}

/**
 * Fetch byte field from current object
 *
 * Format:
 *   getfield_b_this
 *   index
 *
 * Forms:
 *   getfield_b_this = 174 (0xae)
 *
 * Stack:
 *   ... -> ..., value
 *
 * Description:
 *
 *   The currently executing method must be an instance method. The local
 *   variable at index 0 must contain a reference objectref to the currently
 *   executing method's this parameter. The unsigned index is used as an
 *   index into the constant pool of the current package (§3.5 Frames). The
 *   constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[18]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   18: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_b_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_b_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_b_this() {
  jref_t objectref;
  uint8_t index;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_B_THIS 0x%02X", index);

  /*
   * To obtain the this reference, the JCVM specification says that "The
   * local variable at index 0 must contain a reference objectref to the
   * currently executing method's this parameter."
   */
  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  auto instance = heap.getInstance(objectref);
  value = instance->getField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance));
  stack.push_Byte(value);

  return;
}

/**
 * Fetch short field from current object
 *
 * Format:
 *   getfield_s_this
 *   index
 *
 * Forms:
 *   getfield_s_this = 175 (0xaf)
 *
 * Stack:
 *   ... -> ..., value
 *
 * Description:
 *
 *   The currently executing method must be an instance method. The local
 *   variable at index 0 must contain a reference objectref to the currently
 *   executing method's this parameter. The unsigned index is used as an
 *   index into the constant pool of the current package (§3.5 Frames). The
 *   constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[18]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   18: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_s_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_s_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_s_this() {
  jref_t objectref;
  uint8_t index;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_S_THIS 0x%02X", index);

  /*
   * To obtain the this reference, the JCVM specification says that "The
   * local variable at index 0 must contain a reference objectref to the
   * currently executing method's this parameter."
   */
  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  auto instance = heap.getInstance(objectref);
  const uint16_t field_token = resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance);
  value = instance->getField_Short(field_token);
  stack.push_Short(value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Fetch int field from current object
 *
 * Format:
 *   getfield_i_this
 *   index
 *
 * Forms:
 *   getfield_i_this = 176 (0xb0)
 *
 * Stack:
 *   ... -> ..., value.word1, value.word2
 *
 * Description:
 *
 *   The currently executing method must be an instance method. The local
 *   variable at index 0 must contain a reference objectref to the currently
 *   executing method's this parameter. The unsigned index is used as an
 *   index into the constant pool of the current package (§3.5 Frames). The
 *   constant pool item at the index must be of type
 *   CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[18]. The value at that offset into the class instance
 *   referenced by objectref is fetched. If the value is of type byte or
 *   type boolean, it is sign-extended to a short. The value is pushed onto
 *   the operand stack.
 *
 *   18: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the getfield_i_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the getfield_i_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_getfield_i_this() {
  jref_t objectref;
  uint8_t index;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("GETFIELD_I_THIS 0x%02X", index);

  /*
   * To obtain the this reference, the JCVM specification says that "The
   * local variable at index 0 must contain a reference objectref to the
   * currently executing method's this parameter."
   */
  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  auto instance = heap.getInstance(objectref);
  value = instance->getField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance));
  stack.push_Int(value);

  return;
}

#endif /* JCVM_INT_SUPPORTED */

/**
 * Set reference field in object (wide index)
 *
 * Format:
 *   putfield_a_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   putfield_a_w = 177 (0xb1)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[24]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   24: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance. If a
 *   virtual machine does not support the int data type, the putfield_i_w
 *   instruction will not be available.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_a_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_a_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_a_w() {
  jref_t objectref;
  uint16_t index;
  jref_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("PUFIELD_A_W 0x%04X", index);

  value = stack.pop_Reference();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Reference(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance), value);

  return;
}

/**
 * Set byte field in object (wide index)
 *
 * Format:
 *   putfield_b_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   putfield_b_w = 178 (0xb2)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[24]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   24: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance. If a
 *   virtual machine does not support the int data type, the putfield_i_w
 *   instruction will not be available.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_b_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_b_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_b_w() {
  jref_t objectref;
  uint16_t index;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("PUFIELD_B_W 0x%04X", index);

  value = stack.pop_Byte();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance), value);

  return;
}

/**
 * Set short field in object (wide index)
 *
 * Format:
 *   putfield_s_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   putfield_s_w = 179 (0xb3)
 *
 * Stack:
 *   ..., objectref, value -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[24]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   24: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance. If a
 *   virtual machine does not support the int data type, the putfield_i_w
 *   instruction will not be available.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_s_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_s_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_s_w() {
  jref_t objectref;
  uint16_t index;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("PUFIELD_S_W 0x%04X", index);

  value = stack.pop_Short();

  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);
  auto instance = heap.getInstance(objectref);

  instance->setField_Short(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance), value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Set int field in object (wide index)
 *
 * Format:
 *   putfield_i_w
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   putfield_i_w = 180 (0xb4)
 *
 * Stack:
 *   ..., objectref, value.word1, value.word2 -> ...
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The constant pool
 *   item at the index must be of type CONSTANT_InstanceFieldref (§6.8.2
 *   CONSTANT_InstanceFieldref, CONSTANT_VirtualMethodref, and
 *   CONSTANT_SuperMethodref), a reference to a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[24]. The objectref, which must be of type reference,
 *   and the value are popped from the operand stack. If the field is of
 *   type byte or type boolean, the value is truncated to a byte. The field
 *   at the offset from the start of the object referenced by objectref is
 *   set to the value.
 *
 *   24: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance. If a
 *   virtual machine does not support the int data type, the putfield_i_w
 *   instruction will not be available.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_i_w instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_i_w instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_i_w() {
  jref_t objectref;
  uint16_t index;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextShort();

  TRACE_JCVM_DEBUG("PUFIELD_I_W 0x%04X", index);

  value = stack.pop_Int();
  objectref = stack.pop_Reference();
  rejectShareableProxyFieldAccess(this->context, objectref);

  auto instance = heap.getInstance(objectref);
  instance->setField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance), value);

  return;
}
#endif /* JCVM_INT_SUPPORTED */

/**
 * Set reference field in current object
 *
 * Format:
 *   putfield_a_this
 *   index
 *
 * Forms:
 *   putfield_a_this = 181 (0xb5)
 *
 * Stack:
 *   ..., value -> ...
 *
 * Description:
 *
 *   The currently executing method must be an instance method that was
 *   invoked using the invokevirtual, invokeinterface or invokespecial
 *   instruction. The local variable at index 0 must contain a reference
 *   objectref to the currently executing method's this parameter. The
 *   unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[23]. The value is popped from the operand stack. If
 *   the field is of type byte or type boolean, the value is truncated to a
 *   byte. The field at the offset from the start of the object referenced
 *   by objectref is set to the value.
 *
 *   23: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_a_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_a_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_a_this() {
  uint8_t index;
  jref_t objectref, value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUFIELD_A_THIS 0x%02X", index);

  /*
   * To obtain the this reference, the JCVM specification says that "The
   * local variable at index 0 must contain a reference objectref to the
   * currently executing method's this parameter."
   */
  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  value = stack.pop_Reference();
  auto instance = heap.getInstance(objectref);
  instance->setField_Reference(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_OBJECT, *instance), value);

  return;
}

/**
 * Set byte field in current object
 *
 * Format:
 *   putfield_b_this
 *   index
 *
 * Forms:
 *   putfield_b_this = 182 (0xb6)
 *
 * Stack:
 *   ..., value -> ...
 *
 * Description:
 *
 *   The currently executing method must be an instance method that was
 *   invoked using the invokevirtual, invokeinterface or invokespecial
 *   instruction. The local variable at index 0 must contain a reference
 *   objectref to the currently executing method's this parameter. The
 *   unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[23]. The value is popped from the operand stack. If
 *   the field is of type byte or type boolean, the value is truncated to a
 *   byte. The field at the offset from the start of the object referenced
 *   by objectref is set to the value.
 *
 *   23: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_b_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_b_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_b_this() {
  uint8_t index;
  jref_t objectref;
  jbyte_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUFIELD_B_THIS 0x%02X", index);

  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);

  value = stack.pop_Byte();
  auto instance = heap.getInstance(objectref);
  instance->setField_Byte(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_BYTE, *instance), value);

  return;
}

/**
 * Set short field in current object
 *
 * Format:
 *   putfield_s_this
 *   index
 *
 * Forms:
 *   putfield_s_this = 183 (0xb7)
 *
 * Stack:
 *   ..., value -> ...
 *
 * Description:
 *
 *   The currently executing method must be an instance method that was
 *   invoked using the invokevirtual, invokeinterface or invokespecial
 *   instruction. The local variable at index 0 must contain a reference
 *   objectref to the currently executing method's this parameter. The
 *   unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[23]. The value is popped from the operand stack. If
 *   the field is of type byte or type boolean, the value is truncated to a
 *   byte. The field at the offset from the start of the object referenced
 *   by objectref is set to the value.
 *
 *   23: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_s_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_s_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_s_this() {
  uint8_t index;
  jref_t objectref;
  jshort_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUFIELD_S_THIS 0x%02X", index);

  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  value = stack.pop_Short();

  auto instance = heap.getInstance(objectref);
  instance->setField_Short(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_SHORT, *instance), value);

  return;
}

#ifdef JCVM_INT_SUPPORTED
/**
 * Set int field in current object
 *
 * Format:
 *   putfield_i_this
 *   index
 *
 * Forms:
 *   putfield_i_this = 184 (0xb8)
 *
 * Stack:
 *   ..., value.word1, value.word2 -> ...
 *
 * Description:
 *
 *   The currently executing method must be an instance method that was
 *   invoked using the invokevirtual, invokeinterface or invokespecial
 *   instruction. The local variable at index 0 must contain a reference
 *   objectref to the currently executing method's this parameter. The
 *   unsigned index is used as an index into the constant pool of the
 *   current package (§3.5 Frames). The constant pool item at the index must
 *   be of type CONSTANT_InstanceFieldref (§6.8.2 CONSTANT_InstanceFieldref,
 *   CONSTANT_VirtualMethodref, and CONSTANT_SuperMethodref), a reference to
 *   a class and a field token.
 *
 *   The class of objectref must not be an array. If the field is protected,
 *   and it is a member of a superclass of the current class, and the field
 *   is not declared in the same package as the current class, then the
 *   class of objectref must be either the current class or a subclass of
 *   the current class. If the field is final, it must be declared in the
 *   current class.
 *
 *   The width of a field in a class instance is determined by the field
 *   type specified in the instruction. The item is resolved, determining
 *   the field offset[23]. The value is popped from the operand stack. If
 *   the field is of type byte or type boolean, the value is truncated to a
 *   byte. The field at the offset from the start of the object referenced
 *   by objectref is set to the value.
 *
 *   23: The offset may be computed by adding the field token value to the
 *   size of an instance of the immediate superclass. However, this method
 *   is not required by this specification. A Java Card virtual machine may
 *   define any mapping from token value to offset into an instance.
 *
 * Runtime Exception:
 *
 *   If objectref is null, the putfield_i_this instruction throws a
 *   NullPointerException.
 *
 * Notes:
 *
 *   In some circumstances, the putfield_i_this instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 */
void Bytecodes::bc_putfield_i_this() {
  uint8_t index;
  jref_t objectref;
  jint_t value;
  Stack &stack = this->context.getStack();
  Heap &heap = this->context.getHeap();
  pc_t &pc = stack.getPC();

  index = pc.getNextByte();

  TRACE_JCVM_DEBUG("PUFIELD_I_THIS 0x%02X", index);

  objectref = stack.readLocal_Reference(0);
  rejectShareableProxyFieldAccess(this->context, objectref);
  value = stack.pop_Int();

  auto instance = heap.getInstance(objectref);
  instance->setField_Int(resolveInstanceFieldToken(this->context, index, FieldType::FIELD_TYPE_INT, *instance), value);

  return;
}
#endif /* JCVM_INT_SUPPORTED */

} // namespace jcvm
