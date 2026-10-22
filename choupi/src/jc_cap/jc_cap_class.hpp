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

#ifndef _JC_CAP_CLASS_HPP
#define _JC_CAP_CLASS_HPP

#include "../jcvm_types/jcvmarray.hpp"
#include "../types.hpp"
#include "jc_cap_cp.hpp"

namespace jcvm {

#define JC_CAP_CLASS_ACC_INTERFACE (uint8_t)0x8
#define JC_CAP_CLASS_ACC_SHAREABLE (uint8_t)0x4
#define JC_CAP_CLASS_ACC_REMOTE (uint8_t)0x2

struct __attribute__((__packed__)) jc_cap_implemented_interface_info {
  jc_cap_class_ref interface; /* Represents the implemented interface */
  uint8_t count;              /* Number of entries in the index[] array */
  uint8_t index[/* count */]; /* maps declarations of interface methods */

  const JCVMArray<const uint8_t> indexes() const noexcept {
    return JCVMArray<const uint8_t>(count, index);
  }

  uint16_t getSizeOf() const noexcept {
    return sizeof(jc_cap_implemented_interface_info) + count * sizeof(uint8_t);
  }
};

struct __attribute__((__packed__)) jc_cap_remote_method_info {
  uint16_t method_hash;
  uint16_t signature_offset;
  uint8_t virtual_method_token;

  uint16_t getMethodHash() const noexcept { return NTOHS(method_hash); }
  uint16_t getSignatureOffset() const noexcept {
    return NTOHS(signature_offset);
  }
};

struct __attribute__((__packed__)) jc_cap_remote_interface_info {
  uint8_t remote_methods_count;
  jc_cap_remote_method_info remote_methods[/* remote_methods_count */];

  const uint8_t *hashModifierLengthAddress() const noexcept {
    return reinterpret_cast<const uint8_t *>(remote_methods) +
           remote_methods_count * sizeof(jc_cap_remote_method_info);
  }

  uint8_t hashModifierLength() const noexcept {
    return *hashModifierLengthAddress();
  }

  const uint8_t *hashModifier() const noexcept {
    return hashModifierLengthAddress() + 1;
  }

  const uint8_t *classNameLengthAddress() const noexcept {
    return hashModifier() + hashModifierLength();
  }

  uint8_t classNameLength() const noexcept {
    return *classNameLengthAddress();
  }

  const uint8_t *className() const noexcept {
    return classNameLengthAddress() + 1;
  }

  const uint8_t *remoteInterfacesCountAddress() const noexcept {
    return className() + classNameLength();
  }

  uint8_t remoteInterfacesCount() const noexcept {
    return *remoteInterfacesCountAddress();
  }

  const jc_cap_class_ref *remoteInterfaces() const noexcept {
    return reinterpret_cast<const jc_cap_class_ref *>(
        remoteInterfacesCountAddress() + 1);
  }

  uint16_t getSize() const noexcept {
    return static_cast<uint16_t>(
        sizeof(remote_methods_count) +
        remote_methods_count * sizeof(jc_cap_remote_method_info) +
        1 + hashModifierLength() + 1 + classNameLength() + 1 +
        remoteInterfacesCount() * sizeof(jc_cap_class_ref));
  }
};

// Encode the Java Card RMI remote_ref_with_class representation. The caller
// supplies the package name from Header.cap and the remote object ID from the
// per-applet export registry. Returns 0 when metadata or output capacity is
// invalid.
inline uint16_t encodeRemoteReferenceWithClass(
    const uint16_t remote_object_id, const uint8_t *package_name,
    const uint8_t package_name_length,
    const jc_cap_remote_interface_info &remote_info, uint8_t *output,
    const uint16_t output_capacity) noexcept {
  const uint8_t hash_modifier_length = remote_info.hashModifierLength();
  const uint8_t class_name_length = remote_info.classNameLength();
  if (remote_object_id == 0xffff || package_name == nullptr ||
      package_name_length == 0 || remote_info.className() == nullptr ||
      class_name_length == 0 || output == nullptr) {
    return 0;
  }

  const uint16_t required = static_cast<uint16_t>(
      2 + 1 + hash_modifier_length + 1 + package_name_length + 1 +
      class_name_length);
  if (required > output_capacity) {
    return 0;
  }

  uint16_t offset = 0;
  output[offset++] = static_cast<uint8_t>(remote_object_id >> 8);
  output[offset++] = static_cast<uint8_t>(remote_object_id);
  output[offset++] = hash_modifier_length;
  for (uint8_t index = 0; index < hash_modifier_length; ++index) {
    output[offset++] = remote_info.hashModifier()[index];
  }
  output[offset++] = package_name_length;
  for (uint8_t index = 0; index < package_name_length; ++index) {
    output[offset++] = package_name[index];
  }
  output[offset++] = class_name_length;
  for (uint8_t index = 0; index < class_name_length; ++index) {
    output[offset++] = remote_info.className()[index];
  }
  return offset;
}

struct jc_cap_remote_interface_name {
  const uint8_t *package_name;
  uint8_t package_name_length;
  const uint8_t *interface_name;
  uint8_t interface_name_length;
};

// Encode the Java Card RMI remote_ref_with_interfaces representation. A zero
// package-name length reuses the preceding interface's package as specified
// by the RMI protocol; the first interface must always supply its package.
inline uint16_t encodeRemoteReferenceWithInterfaces(
    const uint16_t remote_object_id, const uint8_t *hash_modifier,
    const uint8_t hash_modifier_length,
    const jc_cap_remote_interface_name *interfaces,
    const uint8_t interface_count, uint8_t *output,
    const uint16_t output_capacity) noexcept {
  if (remote_object_id == 0xffff || output == nullptr || interfaces == nullptr ||
      interface_count == 0 || interface_count >= 16 ||
      (hash_modifier_length != 0 && hash_modifier == nullptr) ||
      interfaces[0].package_name_length == 0) {
    return 0;
  }

  uint16_t required = static_cast<uint16_t>(
      2 + 1 + hash_modifier_length + 1);
  for (uint8_t index = 0; index < interface_count; ++index) {
    const auto &interface = interfaces[index];
    if (interface.interface_name == nullptr ||
        interface.interface_name_length == 0 ||
        (interface.package_name_length != 0 &&
         interface.package_name == nullptr)) {
      return 0;
    }
    required = static_cast<uint16_t>(required + 1 +
                                     interface.package_name_length + 1 +
                                     interface.interface_name_length);
  }
  if (required > output_capacity) {
    return 0;
  }

  uint16_t offset = 0;
  output[offset++] = static_cast<uint8_t>(remote_object_id >> 8);
  output[offset++] = static_cast<uint8_t>(remote_object_id);
  output[offset++] = hash_modifier_length;
  for (uint8_t index = 0; index < hash_modifier_length; ++index) {
    output[offset++] = hash_modifier[index];
  }
  output[offset++] = interface_count;
  for (uint8_t index = 0; index < interface_count; ++index) {
    const auto &interface = interfaces[index];
    output[offset++] = interface.package_name_length;
    for (uint8_t name_index = 0;
         name_index < interface.package_name_length; ++name_index) {
      output[offset++] = interface.package_name[name_index];
    }
    output[offset++] = interface.interface_name_length;
    for (uint8_t name_index = 0;
         name_index < interface.interface_name_length; ++name_index) {
      output[offset++] = interface.interface_name[name_index];
    }
  }
  return offset;
}

struct __attribute__((__packed__)) jc_cap_class_info {
#ifdef NVM_BIG_ENDIAN
  uint8_t flags : 4;                /* Class flags */
  uint8_t interface_count : 4;      /* Number of entries in the implemented
                             interfaces      table */
#else                               /* !NVM_BIG_ENDIAN */
  uint8_t interface_count : 4; /* Number of entries in the implemented
                        interfaces table */
  uint8_t flags : 4;           /* Class flags */
#endif                              /* NVM_BIG_ENDIAN */
  jc_cap_class_ref super_class_ref; /* super class of this class */
  uint8_t declared_instance_size;   /* Instance size in word */
  uint8_t
      first_reference_token; /* First reference in the instance field token */
  uint8_t reference_count; /* Number of reference in the type instance field */
  uint8_t public_method_table_base;  /* Public method table base */
  uint8_t public_method_table_count; /* Number of public method table entries */
  uint8_t package_method_table_base; /* Package method table base */
  uint8_t
      package_method_table_count; /* Number of package method table entries */
  uint16_t data[];
  // {
  //   uint16_t public_virtual_method_table [/* public_method_table_count */];
  //   uint16_t package_virtual_method_table [/* package_method_table_count */];
  //   struct jc_cap_implemented_interface_info interfaces [/* interface_count
  //   */];
  // }

  const uint16_t getSize() const noexcept {
    uint16_t size = sizeof(jc_cap_class_info) +
                    (public_method_table_count + package_method_table_count) *
                        sizeof(uint16_t);
    const uint8_t *variable_data = reinterpret_cast<const uint8_t *>(data) +
                                   (public_method_table_count +
                                    package_method_table_count) *
                                       sizeof(uint16_t);

    for (uint8_t index = 0; index < interface_count; index++) {
      auto interface =
          reinterpret_cast<const jc_cap_implemented_interface_info *>(
              variable_data);

      uint16_t current_size = interface->getSizeOf();
      size += current_size;
      variable_data += current_size;
    }

    if ((flags & JC_CAP_CLASS_ACC_REMOTE) != 0) {
      const auto *remote_info =
          reinterpret_cast<const jc_cap_remote_interface_info *>(variable_data);
      size += remote_info->getSize();
    }

    return size;
  }

  const jc_cap_remote_interface_info *remoteInterfaceInfo() const noexcept {
    if ((flags & JC_CAP_CLASS_ACC_REMOTE) == 0) {
      return nullptr;
    }

    const uint16_t method_table_bytes =
        (public_method_table_count + package_method_table_count) *
        sizeof(uint16_t);
    const uint8_t *interfaces =
        reinterpret_cast<const uint8_t *>(data) + method_table_bytes;
    uint16_t byte_offset = 0;
    for (uint8_t index = 0; index < interface_count; ++index) {
      const auto *implemented_interface =
          reinterpret_cast<const jc_cap_implemented_interface_info *>(
              interfaces + byte_offset);
      byte_offset += implemented_interface->getSizeOf();
    }
    return reinterpret_cast<const jc_cap_remote_interface_info *>(
        interfaces + byte_offset);
  }

  inline bool isObjectClass() const noexcept {
    return (NTOHS(super_class_ref.internal_classref) == (uint16_t)0xFFFF);
  }

  const JCVMArray<const uint16_t> public_virtual_method_table() const noexcept {
    return JCVMArray<const uint16_t>(public_method_table_count, data);
  }

  const JCVMArray<const uint16_t> package_virtual_method_table() const
      noexcept {
    return JCVMArray<const uint16_t>(
        package_method_table_count,
        (data + public_method_table_count));
  }

  const jc_cap_implemented_interface_info &
  interfaces(const uint16_t index) const
#ifdef JCVM_ARRAY_SIZE_CHECK
      noexcept(false)
#else
      noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
  {
#ifdef JCVM_ARRAY_SIZE_CHECK

    if (interface_count <= index) {
      throw Exceptions::SecurityException;
    }

#endif /* JCVM_ARRAY_SIZE_CHECK */

    uint16_t offset = 0;
    const uint8_t *interfaces = reinterpret_cast<const uint8_t *>(
        &(data[public_method_table_count + package_method_table_count]));

    for (uint16_t foo = 0; foo < index; ++foo) {
      const jc_cap_implemented_interface_info *implemented_interface =
          reinterpret_cast<const jc_cap_implemented_interface_info *>(
              interfaces + offset);
      offset += implemented_interface->getSizeOf();
    }

    const jc_cap_implemented_interface_info *ret =
        reinterpret_cast<const jc_cap_implemented_interface_info *>(interfaces +
                                                                    offset);
    return *ret;
  }
};

struct __attribute__((__packed__)) jc_cap_interface_info {
#ifdef NVM_BIG_ENDIAN
  uint8_t flags : 4; /* Interface flags */
  uint8_t
      interface_count : 4; /* Number of entries in the superinterfaces table */
#else                      /* !NVM_BIG_ENDIAN */
  uint8_t
      interface_count : 4; /* Number of entries in the superinterfaces table */
  uint8_t flags : 4;       /* Interface flags */
#endif                     /* NVM_BIG_ENDIAN */
  /// Current interface's superclasses */
  jc_cap_class_ref superinterfaces[/* interface_count */];

  const JCVMArray<const jc_cap_class_ref> super_interfaces() const noexcept {
    // interface_count is already extracted from the CAP component's 4-bit
    // count field. It is not a 16-bit network-order value.
    return JCVMArray<const jc_cap_class_ref>(interface_count, superinterfaces);
  }

  const uint8_t *interfaceNameLengthAddress() const noexcept {
    if ((flags & JC_CAP_CLASS_ACC_REMOTE) == 0) {
      return nullptr;
    }
    return reinterpret_cast<const uint8_t *>(superinterfaces) +
           interface_count * sizeof(jc_cap_class_ref);
  }

  uint8_t interfaceNameLength() const noexcept {
    const uint8_t *length = interfaceNameLengthAddress();
    return length == nullptr ? 0 : *length;
  }

  const uint8_t *interfaceName() const noexcept {
    const uint8_t *length = interfaceNameLengthAddress();
    return length == nullptr ? nullptr : length + 1;
  }

  const uint16_t getSize() const noexcept {
    uint16_t size = sizeof(jc_cap_interface_info) +
                    interface_count * sizeof(jc_cap_class_ref);
    if ((flags & JC_CAP_CLASS_ACC_REMOTE) != 0) {
      size += 1 + interfaceNameLength();
    }
    return size;
  }
};

struct __attribute__((__packed__)) jc_cap_class_component {
  uint8_t tag;   /* Component Tag: COMPONENT_Class (6) */
  uint16_t size; /* Component size */
  uint8_t infos[/* size */];
  // {
  /**
   * The interfaces item represents an array of interface_info structures.
   * Each interface defined in this package is represented in the array. The
   * entries are ordered based on hierarchy such that a superinterface has a
   * lower index than any of its subinterfaces.
   */
  // struct jc_cap_interface_info interfaces[];
  /**
   * The classes item represents a table of variable-length class_info
   * structures. Each class defined in this package is represented in the
   * array. The entries are ordered based on hierarchy such that a
   * superclass has a lower index than any of its subclasses.
   */
  // struct jc_cap_class_info classes[];
  // }

  /*
    const struct jc_cap_interface_info * interfaces() const noexcept
  {
    return reinterpret_cast<struct jc_cap_interface_info *>(infos);
  }

  const struct jc_cap_class_info * classes() const noexcept
  {
    return reinterpret_cast<struct jc_cap_class_info *>(infos + ???);
  }
  */

  const JCVMArray<const uint8_t> claz() const noexcept {
    return JCVMArray<const uint8_t>(NTOHS(size), infos);
  }

  // CAP 2.2+ prefixes Class component contents with a u2 signature-pool
  // length and that many signature bytes. Callers must check the CAP version
  // before using this accessor; CAP 2.1 starts directly with interface_info.
  bool hasValidSignaturePool() const noexcept {
    const uint16_t component_size = NTOHS(size);
    if (component_size < 2) {
      return false;
    }
    const uint16_t length = static_cast<uint16_t>(
        (static_cast<uint16_t>(infos[0]) << 8) | infos[1]);
    return length <= component_size - 2;
  }

  uint16_t signaturePoolLength() const noexcept {
    return hasValidSignaturePool()
               ? static_cast<uint16_t>((static_cast<uint16_t>(infos[0]) << 8) |
                                       infos[1])
               : 0;
  }

  const uint8_t *signaturePool() const noexcept {
    return hasValidSignaturePool() ? infos + 2 : nullptr;
  }

  uint16_t classInfoStartOffset() const noexcept {
    return hasValidSignaturePool()
               ? static_cast<uint16_t>(2 + signaturePoolLength())
               : 0;
  }
};

#define IS_INTERFACE(classref)                                                 \
  ((((uint8_t)(((jc_cap_interface_info *)classref)->flags)) &                  \
    JC_CAP_CLASS_ACC_INTERFACE))
#define IS_CLASS(classref) (!IS_INTERFACE(classref))

} // namespace jcvm

#endif /* _JC_CAP_CLASS_HPP */
