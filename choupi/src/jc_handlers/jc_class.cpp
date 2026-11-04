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

#include "jc_class.hpp"
#include "jc_cp.hpp"
#include "jc_import.hpp"
#include "flashmemory.hpp"
#include "../../../src/vm_diagnostics.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <array>

namespace jcvm {

namespace {
jc_cap_virtual_method_ref_info remapVirtualMethodRef(
    Package package, jc_cap_virtual_method_ref_info method_ref) {
  if (!method_ref.class_ref.isExternalClassRef()) {
    return method_ref;
  }

  const uint8_t package_token =
      method_ref.class_ref.external_classref.package_token & 0x7F;
  const auto *imported_package = Import_Handler(package).getPackageAID(package_token);
  const uint8_t method_flags = method_ref.token & 0x80;
  const uint8_t class_token =
      method_ref.class_ref.external_classref.class_token;
  const uint8_t method_token = method_ref.token & 0x7F;
  const uint8_t remapped_method =
      FlashMemory_Handler::isDynamicPackageID(package.getPackageID())
          ? remap_imported_virtual_method_token(*imported_package, class_token,
                                                method_token)
          : method_token;
  const bool applet_protected_entry =
      imported_package->AID_length == 7 &&
      std::memcmp(imported_package->AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 3;
  method_ref.token = static_cast<uint8_t>(
      (applet_protected_entry ? 0 : method_flags) | remapped_method);
  return method_ref;
}
} // namespace

/*
 * The checkcast function check if a type in is compatible with the
 * type_out. The rule is the following one:
 *
 * - If S is a class type, then:
 *     + If T is a class type, then S must be the same class as T, or
 *       S must be a subclass of T;
 *     + If T is an interface type, then S must implement interface T.
 * - If S is an interface type[13], then:
 *     + If T is a class type, then T must be Object (§2.2.1.4
 *       Unsupported Classes);
 *     + If T is an interface type, T must be the same interface as S
 *       or a superinterface of S.
 * - If S is an array type, namely the type SC[], that is, an array of
 *   components of type SC, then:
 *     + If T is a class type, then T must be Object.
 *     + If T is an array type, namely the type TC[], an array of
 *       components of type TC, then one of the following must be true:
 *         * TC and SC are the same primitive type (§3.1 Data Types and
 *           Values).
 *         * TC and SC are reference types[14] (§3.1 Data Types and Values)
 *           with type SC assignable to TC, by these rules.
 *     + If T is an interface type, T must be one of the interfaces
 *       implemented by arrays.
 *
 * 13: When both S and T are arrays of reference types, this algorithm is
 * applied recursively using the types of the arrays, namely SC and TC. In
 * the recursive call, S, which was SC in the original call, may be an
 * interface type. This rule can only be reached in this manner.
 * Similarly, in the recursive call, T, which was TC in the original call,
 * may be an interface type.
 *
 * @param[jtype_in] the input type
 * @param[jtype_out] the out type
 *
 * @return TRUE if the types are compatible, else FALSE
 */
jbool_t
Class_Handler::docheckcast(const std::pair<Package, const uint8_t *> jtype_in,
                           const std::pair<Package, const uint8_t *> jtype_out)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
  const bool trace_types = trace_setting != nullptr &&
                           std::strcmp(trace_setting, "1") == 0;
  if (trace_types) {
    std::fprintf(stderr,
                 "CHOUPI:CLASS_CAST_TYPES in_package=%u in_ptr=%p in_flags=%u "
                 "in_kind=%u out_package=%u out_ptr=%p out_flags=%u "
                 "out_kind=%u\n",
                 static_cast<unsigned>(jtype_in.first.getPackageID()),
                 static_cast<const void *>(jtype_in.second),
                 static_cast<unsigned>(*jtype_in.second),
                 IS_INTERFACE(jtype_in.second) ? 1u : 0u,
                 static_cast<unsigned>(jtype_out.first.getPackageID()),
                 static_cast<const void *>(jtype_out.second),
                 static_cast<unsigned>(*jtype_out.second),
                 IS_INTERFACE(jtype_out.second) ? 1u : 0u);
  }
  // case 1: type_in is a class type
  if (IS_CLASS(jtype_in.second)) {
    auto jclass_in = std::make_pair(
        jtype_in.first,
        reinterpret_cast<const jc_cap_class_info *>(jtype_in.second));

    // case 1.1: type_out is a class type
    if (IS_CLASS(jtype_out.second)) {
      auto jclass_out =
          reinterpret_cast<const jc_cap_class_info *>(jtype_out.second);

      // then S must be the same class as T, or S must be a subclass of T
      if (jclass_out->isObjectClass()) {
        return TRUE;
      }

      while (!jclass_in.second->isObjectClass()) {
        if (jclass_in.second == jclass_out) {
          return TRUE;
        }

        jclass_in = ConstantPool_Handler(jclass_in.first)
                        .classref2class(jclass_in.second->super_class_ref);
      }

      return FALSE;
    }
    // case 1.2: type_out is an interface type
    else {
      const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
      const bool trace_interfaces = trace_setting != nullptr &&
                                    std::strcmp(trace_setting, "1") == 0;
      // Interfaces may be declared on any superclass, including one from an
      // imported package. Keep the walk bounded so malformed cyclic CAP
      // metadata fails closed instead of looping forever.
      for (uint16_t depth = 0; depth < 256; ++depth) {
        if (jclass_in.second->isObjectClass()) return FALSE;

        for (uint8_t i = 0; i < jclass_in.second->interface_count; ++i) {
          if (trace_interfaces) {
            std::fprintf(stderr,
                         "CHOUPI:INTERFACE_ENTRY package=%u index=%u count=%u\n",
                         static_cast<unsigned>(jclass_in.first.getPackageID()),
                         static_cast<unsigned>(i),
                         static_cast<unsigned>(jclass_in.second->interface_count));
          }
          auto implemented_interface = jclass_in.second->interfaces(i);
          ConstantPool_Handler cp(jclass_in.first);
          std::pair<Package, const uint8_t *> resolved_interface(
              jclass_in.first, static_cast<const uint8_t *>(nullptr));
          try {
            resolved_interface =
                cp.resolveClassref(implemented_interface.interface, true);
            if (trace_interfaces) {
              std::fprintf(stderr,
                           "CHOUPI:INTERFACE_RESOLVED index=%u package=%u "
                           "kind=%u ptr=%p\n",
                           static_cast<unsigned>(i),
                           static_cast<unsigned>(resolved_interface.first.getPackageID()),
                           IS_INTERFACE(resolved_interface.second) ? 1u : 0u,
                           static_cast<const void *>(resolved_interface.second));
            }
          } catch (Exceptions exception) {
            if (trace_interfaces) {
              const auto interface_ref = implemented_interface.interface;
              std::fprintf(stderr,
                           "CHOUPI:INTERFACE_RESOLVE_EXCEPTION package=%u "
                           "entry=%u external=%u package_token=%u "
                           "class_token=%u internal_token=%u code=%u\n",
                           static_cast<unsigned>(jclass_in.first.getPackageID()),
                           static_cast<unsigned>(i),
                           interface_ref.isExternalClassRef() ? 1u : 0u,
                           interface_ref.isExternalClassRef()
                               ? static_cast<unsigned>(interface_ref.external_classref.package_token)
                               : 0u,
                           interface_ref.isExternalClassRef()
                               ? static_cast<unsigned>(interface_ref.external_classref.class_token)
                               : 0u,
                           interface_ref.isExternalClassRef()
                               ? 0u
                               : static_cast<unsigned>(interface_ref.internal_classref),
                           static_cast<unsigned>(exception));
            }
            throw;
          }
          bool matches = false;
          try {
            matches = Class_Handler::checkInterfaceCast(
                          resolved_interface, jtype_out) == TRUE;
          } catch (Exceptions exception) {
            if (trace_interfaces) {
              std::fprintf(stderr,
                           "CHOUPI:INTERFACE_CHECK_EXCEPTION in_package=%u "
                           "interface_ptr=%p interface_kind=%u "
                           "target_package=%u target_ptr=%p target_kind=%u "
                           "code=%u\n",
                           static_cast<unsigned>(
                               resolved_interface.first.getPackageID()),
                           static_cast<const void *>(resolved_interface.second),
                           IS_INTERFACE(resolved_interface.second) ? 1u : 0u,
                           static_cast<unsigned>(jtype_out.first.getPackageID()),
                           static_cast<const void *>(jtype_out.second),
                           IS_INTERFACE(jtype_out.second) ? 1u : 0u,
                           static_cast<unsigned>(exception));
            }
            throw;
          }
          if (trace_interfaces) {
            std::fprintf(stderr,
                         "CHOUPI:INTERFACE_CHECK in_package=%u in_class_ptr=%p "
                         "interface_package=%u interface_ptr=%p interface_kind=%u "
                         "target_package=%u target_ptr=%p target_kind=%u "
                         "matches=%u\n",
                         static_cast<unsigned>(jclass_in.first.getPackageID()),
                         static_cast<const void *>(jclass_in.second),
                         static_cast<unsigned>(resolved_interface.first.getPackageID()),
                         static_cast<const void *>(resolved_interface.second),
                         IS_INTERFACE(resolved_interface.second) ? 1u : 0u,
                         static_cast<unsigned>(jtype_out.first.getPackageID()),
                         static_cast<const void *>(jtype_out.second),
                         IS_INTERFACE(jtype_out.second) ? 1u : 0u,
                         matches ? 1u : 0u);
          }
          if (matches) {
            return TRUE;
          }
        }

        ConstantPool_Handler class_cp(jclass_in.first);
        const auto superclass =
            class_cp.classref2class(jclass_in.second->super_class_ref);
        if (superclass.second == nullptr ||
            (superclass.first == jclass_in.first &&
             superclass.second == jclass_in.second)) {
          return FALSE;
        }
        jclass_in = superclass;
      }
      return FALSE;
    }

    return FALSE;
  }

  // case 2: type_in is an interface type
  else {
    // jtype_out must be Object
    if (IS_CLASS(jtype_out.second)) {
      const auto jclass_out = std::make_pair(
          jtype_out.first,
          reinterpret_cast<const jc_cap_class_info *>(jtype_out.second));
      return (jclass_out.second->isObjectClass() ? TRUE : FALSE);
    }

    return Class_Handler::checkInterfaceCast(jtype_in, jtype_out);
  }

  return FALSE;
}

/*
 * Check if two interfaces (interface_in and interface_out) have a hierarchy
 * link together. This function returns TRUE when interface_in is the
 * daughter of interface_out.
 *
 * @param[interface_in]
 * @param[interface_out]
 *
 * @return returns TRUE when interface_in is the daughter of interface_out
 */
jbool_t Class_Handler::checkInterfaceCast(
    const std::pair<Package, const uint8_t *> interface_in,
    const std::pair<Package, const uint8_t *> interface_out)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (!IS_INTERFACE(interface_in.second) ||
      !IS_INTERFACE(interface_out.second)) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  if (interface_in.first == interface_out.first &&
      interface_in.second == interface_out.second) {
    return TRUE;
  }

  // Traverse the complete superinterface graph. The fixed queue bounds both
  // cycles in malformed CAP data and stack/workspace usage on Pico 2.
  constexpr size_t kMaxInterfaceNodes = 64;
  struct InterfaceNode {
    jpackage_ID_t package_id;
    const uint8_t *info;
  };
  std::array<InterfaceNode, kMaxInterfaceNodes> pending{};
  size_t count = 1;
  size_t next = 0;
  pending[0] = {interface_in.first.getPackageID(), interface_in.second};

  while (next < count) {
    const InterfaceNode node = pending[next++];
    const Package node_package(node.package_id);
    const auto superinterfaces =
        reinterpret_cast<const jc_cap_interface_info *>(node.info)
            ->super_interfaces();

    for (uint8_t j = 0; j < superinterfaces.size(); ++j) {
      const jc_cap_class_ref super_interface_ref = superinterfaces.at(j);
      const auto super_interface_ptr =
          ConstantPool_Handler(node_package)
              .resolveClassref(super_interface_ref, true);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

      if (!IS_INTERFACE(super_interface_ptr.second)) {
        throw Exceptions::SecurityException;
      }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

      if (super_interface_ptr.first == interface_out.first &&
          super_interface_ptr.second == interface_out.second) {
        return TRUE;
      }

      const InterfaceNode candidate = {
          super_interface_ptr.first.getPackageID(),
          super_interface_ptr.second};
      bool already_visited = false;
      for (size_t index = 0; index < count; ++index) {
        if (pending[index].package_id == candidate.package_id &&
            pending[index].info == candidate.info) {
          already_visited = true;
          break;
        }
      }
      if (!already_visited) {
        if (count == pending.size()) return FALSE;
        pending[count++] = candidate;
      }
    }
  }

  return FALSE;
}

/*
 * Get the reference to the Object class from a classref.
 *
 * @param[classref] a classref to find this ultimate class.
 * @return the reference to the Object class.
 */
std::pair<Package, const jc_cap_class_info *>
Class_Handler::getObjectClassFromAnObjectRef(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  ConstantPool_Handler cp_handler(this->package);
  auto token = cp_handler.classref2class(classref);

  while (!(token.second->isObjectClass())) {
    cp_handler.setPackage(token.first);
    token = cp_handler.classref2class(token.second->super_class_ref);
  }

  return token;
}

/**
 * Get a class public method offset from a class' public method token.
 *
 * @param[public_method_token] public method token to resolve.
 *
 * @return the public virtual method offset in the Method component.
 */
std::pair<Package, const uint16_t> Class_Handler::getPublicMethodOffset(
    const jc_cap_virtual_method_ref_info virtual_method_ref_info,
    const bool reverse_embedded_token_for_dynamic_class) {
#ifndef PC_VERSION
  choupi_install_stage(30);
  choupi_install_context(
      (static_cast<uint32_t>(
           NTOHS(virtual_method_ref_info.class_ref.internal_classref))
       << 16) |
      virtual_method_ref_info.token);
#endif

  if (!virtual_method_ref_info.isPublicMethod()) {
    throw Exceptions::SecurityException;
  }

  ConstantPool_Handler cp_handler(this->package);
  const uint8_t method_token = virtual_method_ref_info.token;
  auto token = cp_handler.classref2class(virtual_method_ref_info.class_ref);
#ifndef PC_VERSION
  choupi_install_stage(27);
  choupi_install_context(
      (static_cast<uint32_t>(token.first.getPackageID()) << 24) |
      (static_cast<uint32_t>(method_token) << 16) |
      (static_cast<uint32_t>(token.second->public_method_table_base) << 8) |
      token.second->public_method_table_count);
#endif
  return this->doGetPublicMethodOffset(token.first, token.second, method_token,
                                      reverse_embedded_token_for_dynamic_class);
}

/**
 * Resolve a public method token against each superclass using that class's
 * own table and package. Inherited entries retain the same public token.
 */
std::pair<Package, const uint16_t>
Class_Handler::doGetPublicMethodOffset(Package &package,
                                       const jc_cap_class_info *claz,
                                       uint8_t public_method_offset,
                                       const bool reverse_embedded_token_for_dynamic_class) {
  ConstantPool_Handler cp_handler(this->package);
  auto token = std::make_pair(package, claz);
  const uint8_t embedded_method_token = public_method_offset;

  // A ROM caller uses the embedded API's token namespace, while the
  // applet's public method table uses the imported JC API's token namespace.
  // Convert only while searching dynamic classes; restore the embedded token
  // when lookup crosses back into the ROM superclass.
  if (reverse_embedded_token_for_dynamic_class &&
      FlashMemory_Handler::isDynamicPackageID(token.first.getPackageID())) {
    auto superclass = token;
    for (uint16_t depth = 0;
         depth < 256 && !superclass.second->super_class_ref.isExternalClassRef();
         ++depth) {
      if (superclass.second->isObjectClass()) break;
      cp_handler.setPackage(superclass.first);
      superclass = cp_handler.classref2class(
          superclass.second->super_class_ref);
    }
    if (!superclass.second->isObjectClass() &&
        superclass.second->super_class_ref.isExternalClassRef() &&
        FlashMemory_Handler::isDynamicPackageID(superclass.first.getPackageID())) {
      const auto &external_ref =
          superclass.second->super_class_ref.external_classref;
      const auto *imported_package = Import_Handler(superclass.first)
          .getPackageAID(external_ref.package_token & 0x7F);
      if (imported_package == nullptr) {
        throw Exceptions::SecurityException;
      }
      public_method_offset = remap_embedded_virtual_method_token(
          *imported_package, external_ref.class_token, embedded_method_token);
    }
  }

  // Preserve the established CAP-caller translation. For the one ROM-origin
  // callback above, use the embedded token again after leaving the dynamic
  // class table.
  const auto resolve_superclass = [&]() {
    const jc_cap_class_ref super_ref = token.second->super_class_ref;
    if (super_ref.isExternalClassRef() &&
        FlashMemory_Handler::isDynamicPackageID(token.first.getPackageID())) {
      if (reverse_embedded_token_for_dynamic_class) {
        public_method_offset = embedded_method_token;
      } else {
        const auto &external_ref = super_ref.external_classref;
        const auto *imported_package =
            Import_Handler(token.first).getPackageAID(
                external_ref.package_token & 0x7F);
        public_method_offset = remap_imported_virtual_method_token(
            *imported_package, external_ref.class_token,
            public_method_offset);
      }
    }
    cp_handler.setPackage(token.first);
    token = cp_handler.classref2class(super_ref);
  };

  for (uint16_t depth = 0; depth < 256; ++depth) {
    const uint8_t base = token.second->public_method_table_base;
    const auto table = token.second->public_virtual_method_table();
    // An empty table contributes no public methods; continue through the
    // superclass chain so inherited methods can still be resolved.
    if (table.size() == 0 || public_method_offset < base) {
      if (token.second->isObjectClass()) {
        throw Exceptions::SecurityException;
      }
      resolve_superclass();
      continue;
    }
    const uint16_t offset = public_method_offset - base;
    if (offset >= table.size()) {
      throw Exceptions::SecurityException;
    }
#ifndef PC_VERSION
    choupi_install_stage(28);
    choupi_install_context(
        (static_cast<uint32_t>(public_method_offset) << 24) |
        (static_cast<uint32_t>(base) << 16) |
        (static_cast<uint32_t>(table.size()) << 8) | offset);
#endif
    const uint16_t method_offset = NTOHS(table.at(offset));
#ifndef PC_VERSION
    choupi_install_stage(29);
    choupi_install_context(static_cast<uint32_t>(method_offset));
#endif
    if (method_offset != 0xFFFF) {
      return std::make_pair(token.first, method_offset);
    }
    if (token.second->isObjectClass()) {
      throw Exceptions::SecurityException;
    }
    resolve_superclass();
  }
  throw Exceptions::SecurityException;
}

/** Resolve a package method token through the class hierarchy. */
std::pair<Package, const uint16_t> Class_Handler::getPackageMethodOffset(
    const jc_cap_virtual_method_ref_info virtual_method_ref_info) {
  if (virtual_method_ref_info.isPublicMethod()) {
    throw Exceptions::SecurityException;
  }

  ConstantPool_Handler cp_handler(this->package);
  // The high bit identifies a package method. The remaining seven bits are
  // the method token used by the class' package virtual-method table.
  const uint8_t method_token = virtual_method_ref_info.token & 0x7F;
  auto token = cp_handler.classref2class(virtual_method_ref_info.class_ref);
  for (uint16_t depth = 0; depth < 256; ++depth) {
    const uint8_t base = token.second->package_method_table_base;
    const auto table = token.second->package_virtual_method_table();
    if (method_token < base) {
      if (token.second->isObjectClass()) {
        throw Exceptions::SecurityException;
      }
      cp_handler.setPackage(token.first);
      token = cp_handler.classref2class(token.second->super_class_ref);
      continue;
    }
    const uint16_t offset = method_token - base;
    if (offset >= table.size()) {
      throw Exceptions::SecurityException;
    }
    const uint16_t method_offset = NTOHS(table.at(offset));
    if (method_offset != 0xFFFF) {
      return std::make_pair(token.first, method_offset);
    }
    if (token.second->isObjectClass()) {
      throw Exceptions::SecurityException;
    }
    cp_handler.setPackage(token.first);
    token = cp_handler.classref2class(token.second->super_class_ref);
  }
  throw Exceptions::SecurityException;
}

/**
 * Get a class public or package method offset from a class' method token.
 *
 * @param[method_token] method token to resolve.
 *
 * @return the public or package virtual method offset in the Method component.
 */
std::pair<Package, const uint16_t> Class_Handler::getMethodOffset(
    const jc_cap_virtual_method_ref_info virtual_method_ref_info) {
  auto method_ref = remapVirtualMethodRef(this->package,
                                          virtual_method_ref_info);

  if (method_ref.isPublicMethod()) {
    return this->getPublicMethodOffset(method_ref);
  } else { // it's a package method
    return this->getPackageMethodOffset(method_ref);
  }
}

std::pair<Package, const uint16_t> Class_Handler::getMethodOffset(
    const jc_cap_virtual_method_ref_info virtual_method_ref_info,
    const jpackage_ID_t receiver_package, const jclass_index_t receiver_class,
    const bool reverse_embedded_token_for_dynamic_class) {
  // A dynamic receiver uses the importing CAP's public token numbers until
  // resolution reaches its external superclass. Translate at that boundary,
  // not here, or methods such as Applet.select() get translated twice.
  auto method_ref =
      FlashMemory_Handler::isDynamicPackageID(receiver_package)
          ? virtual_method_ref_info
          : remapVirtualMethodRef(this->package, virtual_method_ref_info);
  if (FlashMemory_Handler::isDynamicPackageID(receiver_package) &&
      method_ref.class_ref.isExternalClassRef()) {
    const auto *imported_package = Import_Handler(this->package).getPackageAID(
        method_ref.class_ref.external_classref.package_token & 0x7F);
    if (imported_package->AID_length == 7 &&
        std::memcmp(imported_package->AID,
                    "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
        method_ref.class_ref.external_classref.class_token == 3) {
      method_ref.token &= 0x7F;
    }
  }
  Package concrete_package(receiver_package);
  ConstantPool_Handler cp_handler(concrete_package);
  method_ref.class_ref =
      cp_handler.getClassRefFromClassIndex(receiver_class);

  Class_Handler concrete_class_handler(concrete_package);
  if (method_ref.isPublicMethod()) {
    return concrete_class_handler.getPublicMethodOffset(
        method_ref, reverse_embedded_token_for_dynamic_class);
  }
  return concrete_class_handler.getPackageMethodOffset(method_ref);
}

/*
 * Get a class' implemented interface method offset from a class' package method
 * token.
 *
 * @param[class] class where the method will be resolved.
 * @param[interface] implemented interface.
 * @param[implemented_interface_method_number] implemented interface method
 *                                              number.
 * @return the implemented interface method offset in the method component.
 */
std::pair<Package, const uint16_t>
Class_Handler::getImplementedInterfaceMethodOffset(
    const jc_cap_class_ref class_ref, const jc_cap_class_ref interface,
    const uint8_t implemented_interface_method_number, const bool isArray,
    const Package &interface_package) {
  ConstantPool_Handler cp_handler(this->package);
  ConstantPool_Handler interface_cp(interface_package);
  const auto requested_interface = interface_cp.classref2interface(interface);
  auto claz = (isArray ? this->getObjectClassFromAnObjectRef(class_ref)
                       : cp_handler.classref2class(class_ref));

  for (uint16_t index = 0; index < claz.second->interface_count; ++index) {
    const auto &interfaces = claz.second->interfaces(index);

    // Resolving an external interface changes the resolver's package to the
    // imported package. Resolve every entry against the concrete class's own
    // package so one imported interface cannot redirect later internal refs.
    ConstantPool_Handler implemented_interface_cp(claz.first);
    const auto implemented_interface =
        implemented_interface_cp.classref2interface(interfaces.interface);
    if (implemented_interface.first == requested_interface.first &&
        implemented_interface.second == requested_interface.second) {

      uint8_t public_method_offset =
          interfaces.indexes().at(implemented_interface_method_number);

      if (isArray) {
        return this->doGetPublicMethodOffset(claz.first, claz.second,
                                             public_method_offset);
      } else {
        const jc_cap_virtual_method_ref_info method_ref = {
            .class_ref = class_ref,
            .token = public_method_offset,
        };

        return this->getPublicMethodOffset(method_ref);
      }
    }
  }

  throw Exceptions::SecurityException;
}

/*
 * Get the instance field size for an instantiated class.
 *
 * @param[claz_index] class index used to compute the instance field size
 */
const uint16_t
Class_Handler::getInstanceFieldsSize(const jclass_index_t claz_index) const
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
#ifndef PC_VERSION
  choupi_install_stage(5);
#endif
  auto package = this->package;
  auto claz = ConstantPool_Handler(package).getClassFromClassIndex(claz_index);

  uint16_t instance_size = 0;

  while (true) {
    ConstantPool_Handler cp_handler(package);
    instance_size += (claz->declared_instance_size & 0x00FF);
    if (claz->isObjectClass()) {
      break;
    }
#ifndef PC_VERSION
    choupi_install_stage(6);
    choupi_install_context(NTOHS(claz->super_class_ref.internal_classref));
#endif
    auto pair = cp_handler.classref2class(claz->super_class_ref);
#ifndef PC_VERSION
    choupi_install_stage(7);
#endif
    package = pair.first;
    claz = pair.second;
  }

  return instance_size;
}

} // namespace jcvm
