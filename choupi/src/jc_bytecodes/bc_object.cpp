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

#include "../context.hpp"
#include "../debug.hpp"
#include "../heap.hpp"
#include "javacard-rom-metadata.h"
#include "../jc_handlers/flashmemory.hpp"
#include "../jc_handlers/jc_class.hpp"
#include "../jc_handlers/jc_cp.hpp"
#include "../jc_handlers/jc_export.hpp"
#include "../jc_handlers/jc_import.hpp"
#include "../jc_types/jc_array.hpp"
#include "../jc_types/jc_instance.hpp"
#include "../stack.hpp"
#include "bytecodes.hpp"
#include "../../src/vm_diagnostics.h"
#include "javacard-class-token-map.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

namespace jcvm {

namespace {
bool shareableProxyCastTarget(Context &context, const uint8_t atype,
                              const uint16_t index) {
  if (atype != 0) {
    return false;
  }
  try {
    const Package package = context.getCurrentPackage();
    ConstantPool_Handler constant_pool(package);
    const auto class_ref = constant_pool.getClassRef(index);
    const auto target = constant_pool.resolveClassref(class_ref, true);
    if (!IS_INTERFACE(target.second)) {
      return false;
    }
    return (reinterpret_cast<const jc_cap_interface_info *>(target.second)
                ->flags & JC_CAP_CLASS_ACC_SHAREABLE) != 0;
  } catch (Exceptions) {
    return false;
  }
}

// NativeKey has one ROM class whose declared interfaces are the union of
// supported key families. JC305 instanceof must refine those interfaces by
// the instance's actual Key.getType() value.
std::optional<bool> nativeKeyInterfaceMatch(
    JC_Instance &instance, const uint8_t source_interface_token) {
  constexpr uint8_t kDesTransientReset = 1;
  constexpr uint8_t kDesTransientDeselect = 2;
  constexpr uint8_t kDesPersistent = 3;
  constexpr uint8_t kRsaPublic = 4;
  constexpr uint8_t kRsaPrivate = 5;
  constexpr uint8_t kRsaCrtPrivate = 6;
  constexpr uint8_t kEcFpPublic = 11;
  constexpr uint8_t kEcFpPrivate = 12;
  constexpr uint8_t kAesTransientReset = 13;
  constexpr uint8_t kAesTransientDeselect = 14;
  constexpr uint8_t kAesPersistent = 15;
  constexpr uint8_t kRsaPrivateTransientReset = 22;
  constexpr uint8_t kRsaPrivateTransientDeselect = 23;
  constexpr uint8_t kRsaCrtTransientReset = 24;
  constexpr uint8_t kRsaCrtTransientDeselect = 25;
  constexpr uint8_t kEcFpPrivateTransientReset = 30;
  constexpr uint8_t kEcFpPrivateTransientDeselect = 31;

  const Package security_package(ROM_PACKAGE_INDEX_JAVACARD_SECURITY);
  if (instance.getPackageID() != security_package.getPackageID()) {
    return std::nullopt;
  }
  const auto native_key_class_offset = HTONS(
      Export_Handler(security_package).getExportedClassOffset(
          ROM_CLASS_EXPORT_TOKEN_JAVACARD_SECURITY_NATIVEKEY));
  if (instance.getClassIndex() != native_key_class_offset) {
    return std::nullopt;
  }

  const uint8_t key_type = static_cast<uint8_t>(instance.getField_Byte(
      ROM_FIELD_INDEX_JAVACARD_SECURITY_NATIVEKEY_NATIVEKEY_TYPE));
  switch (source_interface_token) {
  case jc305_source_javacard_security_interface_SecretKey:
    return key_type == kDesTransientReset ||
           key_type == kDesTransientDeselect || key_type == kDesPersistent ||
           key_type == kAesTransientReset || key_type == kAesTransientDeselect ||
           key_type == kAesPersistent;
  case jc305_source_javacard_security_interface_PrivateKey:
    return key_type == kRsaPrivate || key_type == kRsaCrtPrivate ||
           key_type == kRsaPrivateTransientReset ||
           key_type == kRsaPrivateTransientDeselect ||
           key_type == kRsaCrtTransientReset ||
           key_type == kRsaCrtTransientDeselect || key_type == kEcFpPrivate ||
           key_type == kEcFpPrivateTransientReset ||
           key_type == kEcFpPrivateTransientDeselect;
  case jc305_source_javacard_security_interface_PublicKey:
    return key_type == kRsaPublic || key_type == kEcFpPublic;
  case jc305_source_javacard_security_interface_AESKey:
    return key_type == kAesTransientReset ||
           key_type == kAesTransientDeselect || key_type == kAesPersistent;
  case jc305_source_javacard_security_interface_DESKey:
    return key_type == kDesTransientReset ||
           key_type == kDesTransientDeselect || key_type == kDesPersistent;
  case jc305_source_javacard_security_interface_RSAPublicKey:
    return key_type == kRsaPublic;
  case jc305_source_javacard_security_interface_RSAPrivateKey:
    return key_type == kRsaPrivate || key_type == kRsaCrtPrivate ||
           key_type == kRsaPrivateTransientReset ||
           key_type == kRsaPrivateTransientDeselect ||
           key_type == kRsaCrtTransientReset || key_type == kRsaCrtTransientDeselect;
  case jc305_source_javacard_security_interface_RSAPrivateCrtKey:
    return key_type == kRsaCrtPrivate ||
           key_type == kRsaCrtTransientReset ||
           key_type == kRsaCrtTransientDeselect;
  case jc305_source_javacard_security_interface_ECPublicKey:
    return key_type == kEcFpPublic;
  case jc305_source_javacard_security_interface_ECPrivateKey:
    return key_type == kEcFpPrivate ||
           key_type == kEcFpPrivateTransientReset ||
           key_type == kEcFpPrivateTransientDeselect;
  default:
    return std::nullopt;
  }
}

bool isRuntimeCryptoExceptionFactoryAllocation(
    Context &context,
    const std::pair<jpackage_ID_t, jclass_index_t> instantiated_class) {
  // Profile-selected providers shift class offsets. Resolve both ROM and
  // imported CryptoException views using the generated JC305 class-token map.
  constexpr uint8_t jc305_crypto_exception_class_token = 12;
  const uint8_t crypto_exception_export_token =
      jc305_javacard_security_class_tokens[
          jc305_crypto_exception_class_token];
  static constexpr uint8_t rom_package_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x01};
  static constexpr uint8_t security_package_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};

  if (context.getCurrentPackageID() != instantiated_class.first) {
    return false;
  }

  const auto *header = Package(instantiated_class.first)
                           .getCap()
                           .getHeader();
  if (header == nullptr) {
    return false;
  }
  const auto &package = header->package;
  const bool is_rom_package =
      instantiated_class.first == 0 &&
      package.AID_length == sizeof(rom_package_aid) &&
      std::memcmp(package.AID, rom_package_aid, sizeof(rom_package_aid)) == 0;
  const bool is_security_import =
      package.AID_length == sizeof(security_package_aid) &&
      std::memcmp(package.AID, security_package_aid,
                  sizeof(security_package_aid)) == 0;
  if (!is_rom_package && !is_security_import) {
    return false;
  }

  // The combined ROM framework package does not export security package
  // classes in the framework token namespace. Its runtime-owned
  // CryptoException record has a fixed internal class index; only imported
  // security package references use the generated security export map.
  constexpr jclass_index_t crypto_exception_rom_index = 234;
  if (is_rom_package) {
    return instantiated_class.second == crypto_exception_rom_index;
  }
  const auto crypto_exception_index = HTONS(
      Export_Handler(Package(instantiated_class.first))
          .getExportedClassOffset(crypto_exception_export_token));
  return instantiated_class.second == crypto_exception_index;
}

jref_t runtimeExceptionReference(Context &context,
                                 const uint8_t *exception_package_aid,
                                 const size_t exception_package_aid_length,
                                 const uint8_t class_token) {
  Package applet_package = context.getCurrentPackage();
  Import_Handler imports(applet_package);
  const auto import_component = applet_package.getCap().getImport();
  if (import_component == nullptr) {
    throw Exceptions::RuntimeException;
  }

  for (uint8_t token = 0; token < import_component->count; ++token) {
    const auto *package_info = imports.getPackageAID(token);
    if (package_info->AID_length != exception_package_aid_length ||
        std::memcmp(package_info->AID, exception_package_aid,
                    exception_package_aid_length) != 0) {
      continue;
    }

    const jpackage_ID_t package_id = imports.getPackageIndex(package_info);
    Package java_lang_package(package_id);
    const uint8_t mapped_class_token =
        remap_imported_class_token(*package_info, class_token);
    const jclass_index_t class_index = HTONS(Export_Handler(java_lang_package)
                                                 .getExportedClassOffset(
                                                     mapped_class_token));
    return context.getHeap().addRuntimeException(package_id, class_index);
  }

  throw Exceptions::RuntimeException;
}

jref_t runtimeCryptoExceptionReference(Context &context) {
  static constexpr uint8_t security_package_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};
  Package applet_package(context.getAppletPackageID());
  const auto import_component = applet_package.getCap().getImport();
  if (import_component == nullptr) {
    throw Exceptions::SecurityException;
  }

  Import_Handler imports(applet_package);
  constexpr uint8_t jc305_crypto_exception_class_token = 12;
  for (uint8_t token = 0; token < import_component->count; ++token) {
    const auto *package_info = imports.getPackageAID(token);
    if (package_info->AID_length != sizeof(security_package_aid) ||
        std::memcmp(package_info->AID, security_package_aid,
                    sizeof(security_package_aid)) != 0) {
      continue;
    }

    const jpackage_ID_t package_id = imports.getPackageIndex(package_info);
    Package security_package(package_id);
    const uint8_t mapped_class_token = remap_imported_class_token(
        *package_info, jc305_crypto_exception_class_token);
    const jclass_index_t class_index = HTONS(
        Export_Handler(security_package).getExportedClassOffset(
            mapped_class_token));
    return context.getHeap().addRuntimeCryptoException(package_id,
                                                       class_index);
  }

  throw Exceptions::SecurityException;
}
} // namespace

bool Bytecodes::dispatchJavaException(const Exceptions exception) {
  static constexpr uint8_t java_lang_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x00, 0x01};
  static constexpr uint8_t javacard_framework_aid[] = {
      0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x01};
  const uint8_t *exception_package_aid = java_lang_aid;
  size_t exception_package_aid_length = sizeof(java_lang_aid);
  uint8_t class_token = 0;
  switch (exception) {
  case Exceptions::IndexOutOfBoundsException:
    class_token = 4;
    break;
  case Exceptions::ArrayIndexOutOfBoundsException:
    class_token = 5;
    break;
  case Exceptions::NegativeArraySizeException:
    class_token = 6;
    break;
  case Exceptions::NullPointerException:
    class_token = 7;
    break;
  case Exceptions::ClassCastException:
    class_token = 8;
    break;
  case Exceptions::ArithmeticException:
    class_token = 9;
    break;
  case Exceptions::RuntimeException:
    class_token = 3;
    break;
  case Exceptions::SecurityException:
    class_token = 10;
    break;
  case Exceptions::ArrayStoreException:
    class_token = 11;
    break;
  case Exceptions::SystemException:
    exception_package_aid = javacard_framework_aid;
    exception_package_aid_length = sizeof(javacard_framework_aid);
    class_token = 13;
    break;
  case Exceptions::TransactionException:
    exception_package_aid = javacard_framework_aid;
    exception_package_aid_length = sizeof(javacard_framework_aid);
    class_token = 23;
    break;
  case Exceptions::CryptoException: {
    const jref_t reference = runtimeCryptoExceptionReference(this->context);
    const uint16_t reason =
        this->context.consumePendingCryptoExceptionReason();
    this->context.getHeap().getInstance(reference)->setField_Short(
        0, static_cast<jshort_t>(reason));
    this->doThrow(reference);
    return true;
  }
  case Exceptions::APDUException:
    exception_package_aid = javacard_framework_aid;
    exception_package_aid_length = sizeof(javacard_framework_aid);
    class_token = 12;
    break;
  default:
    return false;
  }

  const jref_t reference = runtimeExceptionReference(
      this->context, exception_package_aid, exception_package_aid_length,
      class_token);
  if (exception == Exceptions::SystemException) {
    const auto reason = this->context.consumePendingSystemExceptionReason();
    this->context.getHeap().getInstance(reference)->setField_Short(
        0, static_cast<jshort_t>(reason));
  } else if (exception == Exceptions::TransactionException) {
    const auto reason =
        this->context.consumePendingTransactionExceptionReason();
    this->context.getHeap().getInstance(reference)->setField_Short(
        0, static_cast<jshort_t>(reason));
  } else if (exception == Exceptions::APDUException) {
    // APDU state-machine violations are APDUException.ILLEGAL_USE.
    this->context.getHeap().getInstance(reference)->setField_Short(0, 1);
  }
  this->doThrow(reference);
  return true;
}

/**
 * Create new object
 *
 * Format:
 *   new
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   new = 143 (0x8f)
 *
 * Stack:
 *   ...-> ..., objectref
 *
 * Description:
 *
 *   The unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The item at that
 *   index in the constant pool must be of type CONSTANT_Classref (§6.8.1
 *   CONSTANT_Classref), a reference to a class or interface type. The
 *   reference is resolved and must result in a class type (it must not
 *   result in an interface type). Memory for a new instance of that class
 *   is allocated from the heap, and the instance variables of the new
 *   object are initialized to their default initial values. The objectref,
 *   a reference to the instance, is pushed onto the operand stack.
 *
 * Notes:
 *
 *   The new instruction does not completely create a new instance; instance
 *   creation is not completed until an instance initialization method has
 *   been invoked on the uninitialized instance.
 *
 */
void Bytecodes::bc_new() {
  Context &context = this->context;
  Stack &stack = context.getStack();
  Heap &heap = context.getHeap();
  pc_t &pc = stack.getPC();

  uint16_t index = pc.getNextShort();

  TRACE_JCVM_DEBUG("NEW 0x%02X", index);

#ifndef PC_VERSION
  choupi_install_stage(1);
  choupi_install_context(index);
#endif
  ConstantPool_Handler cp(context.getCurrentPackage());
  auto instantiated_class = cp.getClassInformation(index);
#ifdef PC_VERSION
  if (const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
      trace_setting != nullptr && std::strcmp(trace_setting, "1") == 0) {
    std::fprintf(stderr, "CHOUPI:NEW_RESOLVED cp=%u package=%u class=%u\n",
                 static_cast<unsigned>(index),
                 static_cast<unsigned>(instantiated_class.first),
                 static_cast<unsigned>(instantiated_class.second));
  }
#endif
#ifndef PC_VERSION
  choupi_install_stage(2);
  choupi_install_context(
      (static_cast<uint32_t>(instantiated_class.first) << 16) |
      instantiated_class.second);
#endif
  const bool is_runtime_crypto_exception =
      isRuntimeCryptoExceptionFactoryAllocation(context, instantiated_class);
#ifdef PC_VERSION
  if (const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
      trace_setting != nullptr && std::strcmp(trace_setting, "1") == 0) {
    std::fprintf(stderr, "CHOUPI:NEW_RUNTIME_CRYPTO_EXCEPTION value=%u\n",
                 is_runtime_crypto_exception ? 1u : 0u);
  }
#endif
  const jref_t objectref = is_runtime_crypto_exception
                               ? heap.addRuntimeCryptoException(
                                     instantiated_class.first,
                                     instantiated_class.second)
                               : heap.addInstance(instantiated_class.first,
                                                  instantiated_class.second);
#ifndef PC_VERSION
  choupi_install_stage(3);
#endif
  stack.push_Reference(objectref);

  return;
}

/**
 * Check if two object are type-compatitble
 *
 * @param[objectref]
 * @param[atype] Element object type in case of array.
 * @param[index] constant pool classref to compare to objecref
 */
jbool_t Bytecodes::docheck(const jref_t objectref, const uint8_t atype,
                           const jc_cp_offset_t index) {
  Context &context = this->context;
  Heap &heap = context.getHeap();

  if (objectref.isNullPointer()) {
    throw Exceptions::SecurityException;
  }

  if (atype == 0) {
    const auto source_package = context.getCurrentPackage();
    auto type_out_classref =
        ConstantPool_Handler(source_package).getClassRef(index);
    ConstantPool_Handler type_out_cp(source_package);
    auto [type_out_package, type_out_class] =
        type_out_cp.resolveClassref(type_out_classref);
    auto interface_candidate = std::make_pair(
        source_package, static_cast<const uint8_t *>(nullptr));

    // CHECKCAST's Classref does not say which export namespace its token uses.
    // The class token can resolve successfully to an unrelated class that
    // shares the numeric token with an imported interface (for example,
    // OwnerPINx). Resolve the interface namespace independently and test both
    // candidates against the runtime object below.
    try {
      ConstantPool_Handler interface_type_out_cp(source_package);
      auto candidate =
          interface_type_out_cp.resolveClassref(type_out_classref, true);
      if (IS_INTERFACE(candidate.second)) {
        interface_candidate = candidate;
      }
    } catch (Exceptions exception) {
      // Ordinary class references need not have a corresponding interface
      // token in the imported package.
      if (exception != SecurityException) {
        throw;
      }
    }
    if (IS_INTERFACE(type_out_class) && interface_candidate.second == nullptr) {
      interface_candidate = std::make_pair(type_out_package, type_out_class);
    }

    // Java Card arrays are Object instances but do not inherit applet classes
    // or implement arbitrary interfaces. Do not pass array references to
    // Heap::getInstance(), which correctly rejects them as non-instance refs.
    if (objectref.isArray()) {
      return reinterpret_cast<const jc_cap_class_info *>(type_out_class)
                     ->isObjectClass()
                 ? TRUE
                 : FALSE;
    }

    auto instance = heap.getInstance(objectref);
    const Package type_in_package(instance->getPackageID());
    const auto *type_in_class =
        ConstantPool_Handler(type_in_package)
            .getClassFromClassIndex(instance->getClassIndex());
    auto type_in_ref = std::make_pair(
        type_in_package, reinterpret_cast<const uint8_t *>(type_in_class));

    // CHECKCAST accepts class and interface references. The standard
    // resolution supplies the class candidate; when that candidate does not
    // match, also try the interface candidate from the independent token map.
    auto type_out_ref = std::make_pair(type_out_package, type_out_class);
    const auto target_interface =
        interface_candidate.second != nullptr
            ? interface_candidate
            : (IS_INTERFACE(type_out_class)
                   ? type_out_ref
                   : std::make_pair(
                         type_out_package,
                         static_cast<const uint8_t *>(nullptr)));
    if (target_interface.second != nullptr) {
      if (usesJC305CompatibilityTokens(source_package) &&
          type_out_classref.isExternalClassRef()) {
        const auto *imported_package = Import_Handler(source_package).getPackageAID(
            type_out_classref.external_classref.package_token & 0x7f);
        constexpr uint8_t security_aid[] = {
            0xA0, 0x00, 0x00, 0x00, 0x62, 0x01, 0x02};
        if (imported_package != nullptr && imported_package->AID_length == 7 &&
            std::memcmp(imported_package->AID, security_aid,
                        sizeof(security_aid)) == 0) {
          if (const auto native_key_result = nativeKeyInterfaceMatch(
                  *instance, type_out_classref.external_classref.class_token);
              native_key_result.has_value()) {
            return *native_key_result ? TRUE : FALSE;
          }
        }
      }
    }
    const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
    const bool trace_cast = trace_setting != nullptr &&
                            std::strcmp(trace_setting, "1") == 0;
    if (trace_cast) {
      std::fprintf(stderr,
                   "CHOUPI:CHECKCAST cp=%u src_package=%u src_ext=%u "
                   "src_pkg_token=%u src_class_token=%u in_package=%u "
                   "in_class=%u out_package=%u out_interface=%u\n",
                   static_cast<unsigned>(index),
                   static_cast<unsigned>(source_package.getPackageID()),
                   type_out_classref.isExternalClassRef() ? 1u : 0u,
                   type_out_classref.isExternalClassRef()
                       ? static_cast<unsigned>(type_out_classref.external_classref.package_token)
                       : 0u,
                   type_out_classref.isExternalClassRef()
                       ? static_cast<unsigned>(type_out_classref.external_classref.class_token)
                       : 0u,
                   static_cast<unsigned>(type_in_package.getPackageID()),
                   static_cast<unsigned>(instance->getClassIndex()),
                   static_cast<unsigned>(type_out_package.getPackageID()),
                   IS_INTERFACE(type_out_class) ? 1u : 0u);
    }
    try {
      const bool class_result =
          Class_Handler::docheckcast(type_in_ref, type_out_ref);
      bool interface_result = false;
      if (!class_result && interface_candidate.second != nullptr &&
          (interface_candidate.first != type_out_package ||
           interface_candidate.second != type_out_class)) {
        interface_result =
            Class_Handler::docheckcast(type_in_ref, interface_candidate);
      }
      if (trace_cast) {
        std::fprintf(stderr,
                     "CHOUPI:CHECKCAST_RESULT cp=%u class_result=%u "
                     "interface_result=%u\n",
                     static_cast<unsigned>(index), class_result ? 1u : 0u,
                     interface_result ? 1u : 0u);
      }
      return (class_result || interface_result) ? TRUE : FALSE;
    } catch (Exceptions exception) {
      if (trace_cast) {
        std::fprintf(stderr,
                     "CHOUPI:CHECKCAST_EXCEPTION cp=%u code=%u\n",
                     static_cast<unsigned>(index),
                     static_cast<unsigned>(exception));
      }
      throw;
    }

  } else {
    auto array = heap.getArray(objectref);

    jc_array_type array_type;
    switch (atype) {
    case 10:
      array_type = jc_array_type::JAVA_ARRAY_T_BOOLEAN;
      break;
    case 11:
      array_type = jc_array_type::JAVA_ARRAY_T_BYTE;
      break;
    case 12:
      array_type = jc_array_type::JAVA_ARRAY_T_SHORT;
      break;
    case 13:
#ifdef JCVM_INT_SUPPORTED
      array_type = jc_array_type::JAVA_ARRAY_T_INT;
      break;
#else
      throw Exceptions::SecurityException;
#endif /* JCVM_INT_SUPPORTED */
    case 14:
      array_type = jc_array_type::JAVA_ARRAY_T_REFERENCE;
      break;
    default:
      throw Exceptions::SecurityException;
    }

    if (array->getType() != array_type) {
      return FALSE;
    }

    switch (array_type) {
    case jc_array_type::JAVA_ARRAY_T_BOOLEAN:
    case jc_array_type::JAVA_ARRAY_T_BYTE:
    case jc_array_type::JAVA_ARRAY_T_SHORT:
#ifdef JCVM_INT_SUPPORTED
    case jc_array_type::JAVA_ARRAY_T_INT:
#endif /* JCVM_INT_SUPPORTED */
    {

#ifdef JCVM_DYNAMIC_CHECKS_CAP

      // Security check
      if (index != 0) {
        throw Exceptions::SecurityException;
      }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

      // Ok, the cast is allowed.
      return TRUE;
    }

    case jc_array_type::JAVA_ARRAY_T_REFERENCE: {
      auto type_in = array->getReferenceType();
      auto type_out = index;

      auto current_package = context.getCurrentPackage();
      ConstantPool_Handler constantpool_handler(current_package);

      // Native-created global Object[] arrays use 0xFFFF to denote the
      // java.lang.Object component. Resolve the requested component and allow
      // the cast only when it is Object itself; arbitrary reference-array
      // casts would lose the runtime's component-type check.
      if (type_in == 0xFFFF) {
        auto array_reference_classref =
            constantpool_handler.getClassRef(type_out);
        auto array_reference_class =
            constantpool_handler.classref2class(array_reference_classref);
        return array_reference_class.second->isObjectClass() ? TRUE : FALSE;
      }

      auto type_in_ref = constantpool_handler.getClassRef(type_in);
      auto type_out_ref = constantpool_handler.getClassRef(type_out);

      return Class_Handler::docheckcast(
          constantpool_handler.resolveClassref(type_in_ref),
          constantpool_handler.resolveClassref(type_out_ref));
    }
    }
  }

  throw Exceptions::SecurityException;
}

/**
 * Check whether object is of given type
 *
 * Format:
 *   checkcast
 *   atype
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   checkcast = 148 (0x94)
 *
 * Stack:
 *   ..., objectref -> ..., objectref
 *
 * Description:
 *
 *   The unsigned byte atype is a code that indicates if the type against
 *   which the object is being checked is an array type or a class type. It
 *   must take one of the following values or zero:
 *
 *   +-------------+-------+
 *   | Array Type  | atype |
 *   +-------------+-------+
 *   | T_BOOLEAN   |    10 |
 *   | T_BYTE      |    11 |
 *   | T_SHORT     |    12 |
 *   | T_INT       |    13 |
 *   | T_REFERENCE |    14 |
 *   +-------------+-------+
 *
 *   If the value of atype is 10, 11, 12, or 13, the values of the
 *   indexbyte1 and indexbyte2 must be zero, and the value of atype
 *   indicates the array type against which to check the object. Otherwise
 *   the unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (§3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The item at that
 *   index in the constant pool must be of type CONSTANT_Classref (§6.8.1
 *   CONSTANT_Classref), a reference to a class or interface type. The
 *   reference is resolved. If the value of atype is 14, the object is
 *   checked against an array type that is an array of object references of
 *   the type of the resolved class. If the value of atype is zero, the
 *   object is checked against a class or interface type that is the
 *   resolved class.
 *
 *   The objectref must be of type reference. If objectref is null or can be
 *   cast to the specified array type or the resolved class or interface
 *   type, the operand stack is unchanged; otherwise the checkcast
 *   instruction throws a ClassCastException.
 *
 *   The following rules are used to determine whether an objectref that is
 *   not null can be cast to the resolved type: if S is the class of the
 *   object referred to by objectref and T is the resolved class, array or
 *   interface type, checkcast determines whether objectref can be cast to
 *   type T as follows:
 *
 *   - If S is a class type, then:
 *       + If T is a class type, then S must be the same class as T, or S
 *         must be a subclass of T;
 *       + If T is an interface type, then S must implement interface T.
 *   - If S is an interface type[15], then:
 *       + If T is a class type, then T must be Object (§2.2.1.4
 *         Unsupported Classes);
 *       + If T is an interface type, T must be the same interface as S
 *         or a superinterface of S.
 *   - If S is an array type, namely the type SC[], that is, an array of
 *     components of type SC, then:
 *       + If T is a class type, then T must be Object.
 *       + If T is an array type, namely the type TC[], an array of
 *         components of type TC, then one of the following must be
 *         true:
 *           * TC and SC are the same primitive type (§3.1 Data Types
 *             and Values).
 *           * TC and SC are reference types[16] (§3.1 Data Types and
 *             Values) with type SC assignable to TC, by these rules.
 *       + If T is an interface type, T must be one of the interfaces
 *         implemented by arrays.
 *
 *   15: When both S and T are arrays of reference types, this algorithm is
 *   applied recursively using the types of the arrays, namely SC and TC. In
 *   the recursive call, S, which was SC in the original call, may be an
 *   interface type. This rule can only be reached in this manner. Similarly
 *   ,in the recursive call, T, which was TC in the original call, may be an
 *   interface type.
 *
 *   16: This version of the Java Card virtual machine specification does
 *   not support multi-dimensional arrays. Therefore, neither SC or TC can
 *   be an array type.
 *
 * Runtime Exception:
 *
 *   If objectref cannot be cast to the resolved class, array, or interface
 *   type, the checkcast instruction throws a ClassCastException.
 *
 * Notes:
 *
 *   The checkcast instruction is fundamentally very similar to the
 *   instanceof instruction. It differs in its treatment of null, its
 *   behavior when its test fails (checkcast throws an exception, instanceof
 *   pushes a result code), and its effect on the operand stack.
 *
 *   In some circumstances, the checkcast instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 *   If a virtual machine does not support the int data type, the value of
 *   atype may not be 13 (array type = T_INT).
 *
 */
void Bytecodes::bc_checkcast() {
  Context &context = this->context;
  Stack &stack = this->context.getStack();
  pc_t &pc = stack.getPC();

  uint8_t atype = pc.getNextByte();
  uint16_t index = pc.getNextShort();
  jref_t objectref = stack.pop_Reference();

  TRACE_JCVM_DEBUG("CHECKCAST 0x%02X 0x%02X", atype, index);

  if (context.isShareableProxy(objectref)) {
    if (!shareableProxyCastTarget(context, atype, index)) {
      throw Exceptions::ClassCastException;
    }
    stack.push_Reference(objectref);
    return;
  }

  if (objectref.isNullPointer() == FALSE) {
    if (this->docheck(objectref, atype, index) == FALSE) {
      throw Exceptions::ClassCastException;
    }
  }

  stack.push_Reference(objectref);

  return;
}

/**
 * Determine if object is of given type
 *
 * Format:
 *   instanceof
 *   atype
 *   indexbyte1
 *   indexbyte2
 *
 * Forms:
 *   instanceof = 149 (0x95)
 *
 * Stack:
 *   ..., objectref -> ..., result
 *
 * Description:
 *
 *   The unsigned byte atype is a code that indicates if the type against
 *   which the object is being checked is an array type or a class type. It
 *   must take one of the following values or zero:
 *
 *   +-------------+-------+
 *   | Array Type  | atype |
 *   +-------------+-------+
 *   | T_BOOLEAN   |    10 |
 *   | T_BYTE      |    11 |
 *   | T_SHORT     |    12 |
 *   | T_INT       |    13 |
 *   | T_REFERENCE |    14 |
 *   +-------------+-------+
 *
 *   If the value of atype is 10, 11, 12, or 13, the values of the
 *   indexbyte1 and indexbyte2 must be zero, and the value of atype
 *   indicates the array type against which to check the object. Otherwise
 *   the unsigned indexbyte1 and indexbyte2 are used to construct an index
 *   into the constant pool of the current package (3.5 Frames), where the
 *   value of the index is (indexbyte1 << 8) | indexbyte2. The item at that
 *   index in the constant pool must be of type CONSTANT_Classref (§6.8.1
 *   CONSTANT_Classref), a reference to a class or interface type. The
 *   reference is resolved. If the value of atype is 14, the object is
 *   checked against an array type that is an array of object references of
 *   the type of the resolved class. If the value of atype is zero, the
 *   object is checked against a class or interface type that is the
 *   resolved class.
 *
 *   The objectref must be of type reference. It is popped from the operand
 *   stack. If objectref is not null and is an instance of the resolved
 *   class, array or interface, the instanceof instruction pushes a short
 *   result of 1 on the operand stack. Otherwise it pushes a short result of
 *   0.
 *
 *   The following rules are used to determine whether an objectref that is
 *   not null is an instance of the resolved type: if S is the class of the
 *   object referred to by objectref and T is the resolved class, array or
 *   interface type, instanceof determines whether objectref is an instance
 *   of T as follows:
 *
 *   - If S is a class type, then:
 *       + If T is a class type, then S must be the same class as T, or S
 *         must be a subclass of T;
 *       + If T is an interface type, then S must implement interface T.
 *   - If S is an interface type[20], then:
 *       + If T is a class type, then T must be Object (§2.2.1.4
 *         Unsupported Classes);
 *       + If T is an interface type, T must be the same interface as S or
 *         a superinterface of S.
 *   - If S is an array type, namely the type SC[], that is, an array of
 *     components of type SC, then:
 *       + If T is a class type, then T must be Object.
 *       + If T is an array type, namely the type TC[], an array of
 *         components of type TC, then one of the following must be true:
 *           * TC and SC are the same primitive type (§3.1 Data Types and
 *             Values).
 *           * TC and SC are reference types[21] (§3.1 Data Types and
 *             Values) with type SC assignable to TC, by these rules.
 *       + If T is an interface type, T must be one of the interfaces
 *         implemented by arrays.
 *
 *   20: When both S and T are arrays of reference types, this algorithm is
 *   applied recursively using the types of the arrays, namely SC and TC. In
 *   the recursive call, S, which was SC in the original call, may be an
 *   interface type. This rule can only be reached in this manner.
 *   Similarly, in the recursive call, T, which was TC in the original call,
 *   may be an interface type.
 *
 *   21: This version of the Java Card virtual machine specification does
 *   not support multi-dimensional arrays. Therefore, neither SC or TC can
 *   be an array type.
 *
 * Notes:
 *
 *   The instanceof instruction is fundamentally very similar to the
 *   checkcast instruction. It differs in its treatment of null, its
 *   behavior when its test fails (checkcast throws an exception, instanceof
 *   pushes a result code), and its effect on the operand stack.
 *
 *   In some circumstances, the instanceof instruction may throw a
 *   SecurityException if the current context (§3.4 Contexts) is not the
 *   owning context (§3.4 Contexts) of the object referenced by objectref.
 *   The exact circumstances when the exception will be thrown are specified
 *   in Chapter 6 of the Runtime Environment Specification, Java Card 3
 *   Platform, v3.0.5, Classic Edition.
 *
 *   If a virtual machine does not support the int data type, the value of
 *   atype may not be 13 (array type = T_INT).
 */
void Bytecodes::bc_instanceof() {
  Context &context = this->context;
  Stack &stack = this->context.getStack();
  pc_t &pc = stack.getPC();

  uint8_t atype = pc.getNextByte();
  uint16_t index = pc.getNextShort();
  jref_t objectref = stack.pop_Reference();

  TRACE_JCVM_DEBUG("INSTANCEOF 0x%02X 0x%02X", atype, index);

  if (context.isShareableProxy(objectref)) {
    stack.push_Byte(shareableProxyCastTarget(context, atype, index) ? TRUE
                                                                   : FALSE);
    return;
  }

  const jbool_t result = objectref.isNullPointer()
                             ? FALSE
                             : docheck(objectref, atype, index);
  stack.push_Byte(result);

  return;
}

} // namespace jcvm
