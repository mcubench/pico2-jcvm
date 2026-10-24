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

#include "jc_cp.hpp"
#include "../debug.hpp"
#include "jc_class.hpp"
#include "jc_export.hpp"
#include "jc_import.hpp"
#include "flashmemory.hpp"
#include "javacard-class-token-map.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifndef PC_VERSION
extern "C" void choupi_install_stage(uint8_t stage);
extern "C" void choupi_install_context(uint32_t context);
#endif

namespace jcvm {

bool usesJC305CompatibilityTokens(const Package &source_package) noexcept {
  // These translations describe imports in the uploaded JCAlgTest CAP. ROM
  // mask API CAPs use their own export tokens and must resolve them unchanged.
  return FlashMemory_Handler::isDynamicPackageID(
      source_package.getPackageID());
}

// JCAlgTest 1.8.2 was converted with the JC 3.0.5 API export files. The
// embedded CHOUPI ROM-mask API uses the same package AIDs and versions but
// assigns different export tokens to several classes. Translate those
// imported tokens to the class tokens in the embedded API before consulting
// its Export component.
uint8_t remap_imported_class_token(const jc_cap_package_info &package_info,
                                   const uint8_t token) noexcept {
  const uint8_t *mapping = nullptr;
  size_t mapping_size = 0;

  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID, "\xA0\x00\x00\x00\x62\x00\x01", 7) ==
          0) {
    mapping = jc305_java_lang_class_tokens;
    mapping_size = sizeof(jc305_java_lang_class_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0) {
    mapping = jc305_javacard_framework_class_tokens;
    mapping_size = sizeof(jc305_javacard_framework_class_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0) {
    mapping = jc305_javacard_security_class_tokens;
    mapping_size = sizeof(jc305_javacard_security_class_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x02\x01", 7) == 0) {
    mapping = jc305_javacardx_crypto_class_tokens;
    mapping_size = sizeof(jc305_javacardx_crypto_class_tokens);
  }

  if (mapping == nullptr || token >= mapping_size) {
    return token;
  }
  return mapping[token];
}

// JC305 export files assign interface tokens independently from class tokens.
// Map the framework interfaces to the generated ROM API's interface tokens.
uint8_t remap_imported_interface_token(
    const jc_cap_package_info &package_info, const uint8_t token) noexcept {
  const uint8_t *mapping = nullptr;
  size_t mapping_size = 0;
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID, "\xA0\x00\x00\x00\x62\x00\x01", 7) ==
          0) {
    mapping = jc305_java_lang_interface_tokens;
    mapping_size = sizeof(jc305_java_lang_interface_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0) {
    mapping = jc305_javacard_framework_interface_tokens;
    mapping_size = sizeof(jc305_javacard_framework_interface_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0) {
    mapping = jc305_javacard_security_interface_tokens;
    mapping_size = sizeof(jc305_javacard_security_interface_tokens);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x02\x01", 7) == 0) {
    mapping = jc305_javacardx_crypto_interface_tokens;
    mapping_size = sizeof(jc305_javacardx_crypto_interface_tokens);
  }

  if (mapping == nullptr || token >= mapping_size) {
    return token;
  }
  return mapping[token];
}

uint8_t remap_imported_static_method_token(
    const jc_cap_package_info &package_info, const uint8_t class_token,
    const uint8_t method_token) noexcept {
  static constexpr uint8_t framework_jcsystem_methods[] = {
      11, 10, 12, 8,  19, 15, 16, 13, 14, 7,  1,  9,
      2,  3,  5,  4,  17, 20, 21, 22, 23, 6,  18};
  static constexpr uint8_t framework_sensitivearrays_methods[] = {
      1, 2, 3, 4, 5};
  static constexpr uint8_t java_lang_object_methods[] = {0};
  static constexpr uint8_t framework_apdu_methods[] = {1, 2, 3, 7, 4, 5, 6};
  static constexpr uint8_t framework_util_methods[] = {4, 0, 1, 3, 6, 5, 7, 2};
  static constexpr uint8_t security_digest_methods[] = {0, 1, 1};
  static constexpr uint8_t security_signature_methods[] = {0, 1, 1};
  static constexpr uint8_t crypto_cipher_methods[] = {0, 1, 1};
  static constexpr uint8_t sensitive_result_methods[] = {
      1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  static constexpr uint8_t apdu_util_methods[] = {1, 2, 3, 4, 5};

  const uint8_t *mapping = nullptr;
  size_t mapping_size = 0;
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 8) {
    mapping = framework_jcsystem_methods;
    mapping_size = sizeof(framework_jcsystem_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
             class_token == 22) {
    // The embedded class has a constructor at token 0, while the JC305
    // export assigns SensitiveArrays' five static methods tokens 0–4.
    mapping = framework_sensitivearrays_methods;
    mapping_size = sizeof(framework_sensitivearrays_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x00\x01", 7) == 0 &&
             class_token == 0) {
    mapping = java_lang_object_methods;
    mapping_size = sizeof(java_lang_object_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
             class_token == 10) {
    mapping = framework_apdu_methods;
    mapping_size = sizeof(framework_apdu_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
             class_token == 16) {
    mapping = framework_util_methods;
    mapping_size = sizeof(framework_util_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
             class_token == 11) {
    mapping = security_digest_methods;
    mapping_size = sizeof(security_digest_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
             class_token == 15) {
    mapping = security_signature_methods;
    mapping_size = sizeof(security_signature_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x02\x01", 7) == 0 &&
             class_token == 1) {
    mapping = crypto_cipher_methods;
    mapping_size = sizeof(crypto_cipher_methods);
  } else if (package_info.AID_length == 7 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x02\x05", 7) == 0 &&
             class_token == 0) {
    // JC305 exports SensitiveResult's ten static calls at tokens 0–9;
    // CHOUPI's local class includes a constructor at token 0.
    mapping = sensitive_result_methods;
    mapping_size = sizeof(sensitive_result_methods);
  } else if (package_info.AID_length == 8 &&
             std::memcmp(package_info.AID,
                         "\xA0\x00\x00\x00\x62\x02\x09\x01", 8) == 0 &&
             class_token == 0) {
    // APDUUtil has the same constructor-induced token offset.
    mapping = apdu_util_methods;
    mapping_size = sizeof(apdu_util_methods);
  }

  if (mapping == nullptr || method_token >= mapping_size) {
    return method_token;
  }
  return mapping[method_token];
}

uint8_t remap_imported_constructor_method_token(
    const jc_cap_package_info &package_info, const uint8_t class_token,
    const uint8_t method_token) noexcept {
  const bool is_security_package =
      package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0;
  const bool is_crypto_package =
      package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x02\x01", 7) == 0;
  // JC305's javacard.security.MessageDigest import uses method token 1 for
  // its protected no-argument constructor. The embedded ROM export places
  // that constructor at token 2; tokens 0 and 1 have an extra argument.
  // Signature and Cipher use the same JC305-import token / embedded-export
  // offset pair. Keep class tokens package scoped because both packages
  // contain unrelated class tables.
  if (((is_security_package &&
        (class_token == 11 || class_token == 15)) ||
       (is_crypto_package && class_token == 1)) &&
      method_token == 1) {
    return 2;
  }
  return method_token;
}

uint8_t remap_imported_virtual_method_token(
    const jc_cap_package_info &package_info, const uint8_t class_token,
    const uint8_t method_token) noexcept {
  static constexpr uint8_t framework_applet_methods[] = {
      0, 1, 2, 3, 5, 6, 4, 7};
  // JC305's OwnerPIN export orders its public methods differently from the
  // embedded implementation. Translate the imported virtual token before
  // dispatching against the embedded class table.
  static constexpr uint8_t framework_ownerpin_methods[] = {
      0, 4, 3, 1, 6, 7, 9, 2, 8};
  // JC305 orders AID's public methods differently from the embedded
  // framework API. In particular, JC305 token 3 is getBytes([BS)B, while
  // embedded token 3 is partialEquals([BSB)Z (which takes one more word).
  static constexpr uint8_t framework_aid_virtual_methods[] = {
      0, 4, 2, 1, 3, 5};
  // The JC305 APDU export uses a different virtual-method order from the
  // embedded CHOUPI framework API. Tokens below are from the JC305 APDU
  // export; values are the corresponding CHOUPI framework APDU tokens.
  static constexpr uint8_t framework_apdu_virtual_methods[] = {
      0, 1, 2, 6, 8, 9, 7, 3, 10, 5, 4, 11, 12, 13, 14, 15, 16, 17};
  // JC 3.0.5 assigns MessageDigest's virtual methods a different order from
  // the embedded API. Translate the imported method token before dispatch.
  static constexpr uint8_t security_digest_virtual_methods[] = {
      0, 3, 1, 2, 5, 4};
  // JC305 places RandomData.setSeed before nextBytes, while the embedded API
  // export places nextBytes before setSeed. Translate both RandomData and its
  // OneShot subclass so a void setSeed call cannot dispatch to short-returning
  // nextBytes and leave an operand on the caller's stack.
  static constexpr uint8_t security_random_data_virtual_methods[] = {
      0, 1, 3, 2, 4, 5};
  // The JC305 javacardx.crypto.Cipher export orders doFinal/getAlgorithm/
  // init/init/update/getCipherAlgorithm/getPaddingAlgorithm differently from
  // the embedded CHOUPI export. Keep the inherited Object.equals token zero;
  // translate the remaining JC305 tokens to their embedded method tokens.
  static constexpr uint8_t crypto_cipher_virtual_methods[] = {
      0, 6, 3, 1, 2, 7, 4, 5};
  // The JC305 Signature export begins with getAlgorithm/getLength, while
  // CHOUPI's embedded API places its init and digest configuration methods
  // first. Translate imported tokens before dispatching against the embedded
  // Signature class (and dynamic subclasses overriding these methods).
  static constexpr uint8_t security_signature_virtual_methods[] = {
      0, 4, 8, 1, 2, 10, 9, 12, 3, 11, 13, 5, 6, 7};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 11 &&
      method_token < sizeof(security_digest_virtual_methods)) {
    return security_digest_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 15 &&
      method_token < sizeof(security_signature_virtual_methods)) {
    return security_signature_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      (class_token == 14 || class_token == 32) &&
      method_token < sizeof(security_random_data_virtual_methods)) {
    return security_random_data_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x02\x01", 7) == 0 &&
      class_token == 1 &&
      method_token < sizeof(crypto_cipher_virtual_methods)) {
    return crypto_cipher_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 9 &&
      method_token < sizeof(framework_ownerpin_methods)) {
    return framework_ownerpin_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 6 &&
      method_token < sizeof(framework_aid_virtual_methods)) {
    return framework_aid_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 10 &&
      method_token < sizeof(framework_apdu_virtual_methods)) {
    return framework_apdu_virtual_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 3 &&
      method_token < sizeof(framework_applet_methods)) {
    return framework_applet_methods[method_token];
  }
  return method_token;
}

uint8_t remap_embedded_virtual_method_token(
    const jc_cap_package_info &package_info, const uint8_t class_token,
    const uint8_t method_token) noexcept {
  // Imported CAP tokens map to the embedded API order. Calls originating in
  // the embedded ROM and targeting an applet override need the inverse map.
  for (uint16_t imported_token = 0; imported_token < 0x80; ++imported_token) {
    if (remap_imported_virtual_method_token(
            package_info, class_token,
            static_cast<uint8_t>(imported_token)) == method_token) {
      return static_cast<uint8_t>(imported_token);
    }
  }
  return method_token;
}

uint8_t remap_imported_interface_method_token(
    const jc_cap_package_info &package_info, const uint8_t class_token,
    const uint8_t method_token) noexcept {
  // The JC305 Key interface orders methods as clearKey, getSize, getType,
  // isInitialized. The embedded API order is isInitialized, clearKey,
  // getType, getSize. Keep this scoped to the Key interface (class token 0)
  // because other security interfaces have their own token maps below.
  static constexpr uint8_t security_key_interface_methods[] = {1, 3, 2, 0};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 0 &&
      method_token < sizeof(security_key_interface_methods)) {
    return security_key_interface_methods[method_token];
  }
  // JC305's DESKey interface assigns getKey=4 and setKey=5. The embedded API
  // export declares setKey before getKey, so swap those two interface tokens.
  static constexpr uint8_t security_des_key_interface_methods[] = {
      0, 1, 2, 3, 5, 4};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 10 &&
      method_token < sizeof(security_des_key_interface_methods)) {
    return security_des_key_interface_methods[method_token];
  }
  // JC305's AESKey interface orders inherited Key methods as clearKey,
  // getSize, getType, isInitialized. The embedded API orders those as
  // isInitialized, clearKey, getType, getSize. Its own setKey/getKey tokens
  // already match at 4/5.
  static constexpr uint8_t security_aes_key_interface_methods[] = {
      1, 3, 2, 0, 4, 5};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 20 &&
      method_token < sizeof(security_aes_key_interface_methods)) {
    return security_aes_key_interface_methods[method_token];
  }
  // JC305 RSA interfaces place the setters and getters in a different order
  // from the ROM exports. The JC305 CAP calls setModulus as token 7 and
  // setExponent as token 6; the embedded RSAPublicKey/RSAPrivateKey order is
  // setModulus at 4 and setExponent at 5. RSA CRT uses its own declaration
  // order, so keep each map scoped to its imported interface class token.
  static constexpr uint8_t security_rsa_public_private_interface_methods[] = {
      1, 3, 2, 0, 7, 6, 5, 4};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      (class_token == 8 || class_token == 9) &&
      method_token < sizeof(security_rsa_public_private_interface_methods)) {
    return security_rsa_public_private_interface_methods[method_token];
  }
  static constexpr uint8_t security_rsa_crt_interface_methods[] = {
      1, 3, 2, 0, 11, 12, 9, 13, 10, 6, 7, 4, 8, 5};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x02", 7) == 0 &&
      class_token == 7 &&
      method_token < sizeof(security_rsa_crt_interface_methods)) {
    return security_rsa_crt_interface_methods[method_token];
  }
  // The reference JC305 export orders OwnerPINx.check before
  // getTriesRemaining, while this API's generated interface order follows
  // PIN and places getTriesRemaining first. Remap both related interfaces.
  static constexpr uint8_t framework_ownerpinx_interface_methods[] = {
      1, 0, 2, 3, 4, 5, 6, 7, 8};
  static constexpr uint8_t framework_pin_interface_methods[] = {
      1, 0, 2, 3};
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      class_token == 1 &&
      method_token < sizeof(framework_pin_interface_methods)) {
    return framework_pin_interface_methods[method_token];
  }
  if (package_info.AID_length == 7 &&
      std::memcmp(package_info.AID,
                  "\xA0\x00\x00\x00\x62\x01\x01", 7) == 0 &&
      (class_token == 19 || class_token == 20) &&
      method_token < sizeof(framework_ownerpinx_interface_methods)) {
    return framework_ownerpinx_interface_methods[method_token];
  }
  return method_token;
}

/**
 * Get a cp_entry
 *
 * @param[offset] index of the cp_entry
 *
 * @return Get associated Constant Pool offset.
 */
const jc_cap_constant_pool_info
ConstantPool_Handler::getCPEntry(const jc_cp_offset_t offset)
#ifndef JCVM_ARRAY_SIZE_CHECK
    noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
{
  if (offset >= 64) {
    TRACE_JCVM_DEBUG("CP lookup index=%u count=%u package=%u",
                     static_cast<unsigned>(offset),
                     static_cast<unsigned>(HTONS(
                         this->package.getCap().getConstantPool()->count)),
                     static_cast<unsigned>(this->package.getPackageID()));
  }
  return this->package.getCap().getConstantPool()->constantpool()[offset];
}

/*
 * Get Class Information (Package & Class indexes) from current constant pool
 * offset.
 *
 * @param[offset] Current constant pool offset
 *
 * @return Class Information (Package & Class indexes) from current constant
 * pool offset.
 */
std::pair<jpackage_ID_t, jclass_index_t>
ConstantPool_Handler::getClassInformation(const jc_cp_offset_t offset)
#ifndef JCVM_ARRAY_SIZE_CHECK
    noexcept
#endif /* JCVM_ARRAY_SIZE_CHECK */
{
  jpackage_ID_t package;
  jclass_index_t claz;

  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);
#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_CLASSREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  auto classref = cp_entry.info.class_ref_info.class_ref;

  if (classref.isExternalClassRef()) {
    Import_Handler importHandler(this->package);
    const uint8_t package_token =
        classref.external_classref.package_token & 0x7F;
    const jc_cap_package_info *imported_package =
        importHandler.getPackageAID(package_token);
    package = importHandler.getPackageIndex(imported_package);

    Package importedPackage(package);
    Export_Handler exportHandler(importedPackage);
    const uint8_t imported_class_token =
        classref.external_classref.class_token;
    const uint8_t class_token =
        usesJC305CompatibilityTokens(this->package)
            ? remap_imported_class_token(*imported_package,
                                         imported_class_token)
            : imported_class_token;
    claz = exportHandler.getExportedClassOffset(class_token);
  } else // is internal classref
  {
    package = this->package.getPackageID();
    claz = classref.internal_classref;
  }

  return std::make_pair(package, HTONS(claz));
}

/*
 * Converting constant pool offset to jc_class_ref
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a jc_class_ref structure.
 */
const jc_cap_class_ref
ConstantPool_Handler::getClassRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_CLASSREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.class_ref_info.class_ref;
}

/*
 * Convert class index to jc_cap_class_info *
 *
 * @param[claz_index] class index
 * @return associated pointer to class info
 */
const jc_cap_class_info *
ConstantPool_Handler::getClassFromClassIndex(const jclass_index_t claz_index)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_class_info *claz_info = nullptr;
  auto classes = this->getPackage().getCap().getClass()->claz();

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_INTERFACE(&classes[claz_index])) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  claz_info =
      reinterpret_cast<const jc_cap_class_info *>(classes.data() + claz_index);

  return claz_info;
}

/*
 * Convert class index to internal class ref
 *
 * @param[claz_index] class index
 * @return internal class ref
 */
const jc_cap_class_ref ConstantPool_Handler::getClassRefFromClassIndex(
    const jclass_index_t claz_index) {
  // Instance class indexes are CAP Class component byte offsets, not the
  // ordinal position of the class/interface record. Store the offset in CAP
  // byte order so resolveClassref() converts it back to the host integer.
  jc_cap_class_ref classref;
  classref.internal_classref = HTONS(claz_index);
  return classref;
}

/*
 * Converting constant pool offset to struct jc_cap_instance_field_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_instance_field_ref_info structure.
 */
const jc_cap_instance_field_ref_info
ConstantPool_Handler::getInstanceFieldRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_INSTANCEFIELDREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.instance_field_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_virtual_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_virtual_method_ref_info structure.
 */
const jc_cap_virtual_method_ref_info
ConstantPool_Handler::getVirtualMethodRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_VIRTUALMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.virtual_method_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_super_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_super_method_ref_info structure.
 */
const jc_cap_super_method_ref_info
ConstantPool_Handler::getSuperMethodRef(const jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_SUPERMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.super_method_ref_info;
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_class_info structure.
 *
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_class_info structure.
 */
std::pair<Package, const jc_cap_class_info *>
ConstantPool_Handler::classref2class(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  auto claz = this->resolveClassref(classref);
#ifndef PC_VERSION
  choupi_install_stage(23);
  choupi_install_context(static_cast<uint32_t>(claz.first.getPackageID()));
  choupi_install_stage(25);
#endif

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  const bool is_interface = IS_INTERFACE(claz.second);
#ifndef PC_VERSION
  choupi_install_stage(26);
  const uint32_t classref_context =
      (static_cast<uint32_t>(claz.first.getPackageID()) << 24) |
      (static_cast<uint32_t>(classref.external_classref.package_token) << 16) |
      (static_cast<uint32_t>(classref.external_classref.class_token) << 8) |
      (is_interface ? 1u : 0u);
  choupi_install_context(classref_context);
#endif
  if (is_interface) { // Classref is an interface!
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

#ifndef PC_VERSION
  choupi_install_stage(24);
#endif
  return std::make_pair(
      claz.first, reinterpret_cast<const jc_cap_class_info *>(claz.second));
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_interface_info
 * structure.
 *
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_interface_info structure.
 */
std::pair<Package, const jc_cap_interface_info *>
ConstantPool_Handler::classref2interface(const jc_cap_class_ref classref)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  auto interface = this->resolveClassref(classref, true);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (IS_CLASS(interface.second)) { // Classref is a class!
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return std::make_pair(
      interface.first,
      reinterpret_cast<const jc_cap_interface_info *>(interface.second));
}

/*
 * Converting a union jc_cap_class_ref to struct jc_cap_class or struct
 * jc_cap_interface structure.
 *
 * @param[cap] the CAP file used to resolve the classref entry.
 * @param[classref] classref which will be translated.
 * @return a struct jc_cap_class or struct jc_cap_interface structure.
 */
std::pair<Package, const uint8_t *>
ConstantPool_Handler::resolveClassref(const jc_cap_class_ref classref,
                                      const bool is_interface)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
#ifndef PC_VERSION
  choupi_install_stage(11);
#endif
  const bool use_jc305_compatibility_tokens =
      usesJC305CompatibilityTokens(this->package);
  Package &package = this->package;
  auto cap = this->package.getCap();

  uint16_t class_token;
  const uint8_t *classFound = nullptr;

  // Is it an external class_ref?
  if (classref.isExternalClassRef()) {
    Import_Handler import_handler(package);

    //  High byte is set to one => clear it
    const uint8_t imported_package_token =
        classref.external_classref.package_token & 0x7F;
#ifndef PC_VERSION
    choupi_install_stage(12);
    choupi_install_context(
        (static_cast<uint32_t>(imported_package_token) << 8) |
        classref.external_classref.class_token);
#endif
    const jc_cap_package_info *imported_package_ID =
        import_handler.getPackageAID(imported_package_token);
#ifndef PC_VERSION
    choupi_install_stage(13);
#endif
#ifndef PC_VERSION
    choupi_install_stage(14);
#endif
    auto package_index = import_handler.getPackageIndex(imported_package_ID);
#ifndef PC_VERSION
    choupi_install_stage(15);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) |
        classref.external_classref.class_token);
#endif

    package = Package(package_index);
    cap = package.getCap();

    const uint8_t imported_class_token =
        BYTE_TO_SHORT(classref.external_classref.class_token);
    // JC305 class references use a compatibility map because the embedded
    // ROM API was assembled from a different export ordering. Interface
    // tokens have their own namespace and must retain their interface index.
    if (use_jc305_compatibility_tokens) {
      class_token = is_interface
                        ? remap_imported_interface_token(*imported_package_ID,
                                                         imported_class_token)
                        : remap_imported_class_token(*imported_package_ID,
                                                     imported_class_token);
    } else {
      class_token = imported_class_token;
    }
#ifndef PC_VERSION
    choupi_install_stage(18);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) | class_token);
#endif
    Export_Handler export_handler(package);
    class_token = HTONS(export_handler.getExportedClassOffset(class_token));
#ifndef PC_VERSION
    choupi_install_stage(19);
    choupi_install_context(
        (static_cast<uint32_t>(package_index) << 16) | class_token);
#endif
  } else { // is internal class_ref
    class_token = HTONS(classref.internal_classref);
  }

#ifndef PC_VERSION
  choupi_install_stage(16);
  choupi_install_context((static_cast<uint32_t>(package.getPackageID()) << 16) |
                         class_token);
#endif
  classFound = &(cap.getClass()->claz()[class_token]);
#ifdef PC_VERSION
  if (const char *trace_setting = std::getenv("CHOUPI_VM_TRACE_EXCEPTIONS");
      trace_setting != nullptr && std::strcmp(trace_setting, "1") == 0 &&
      classref.isExternalClassRef() && is_interface) {
    const auto *class_base = cap.getClass()->claz().data();
    std::fprintf(stderr,
                 "CHOUPI:INTERFACE_RESOLVE package=%u src_token=%u class_index=%u is_interface=%u ptr_offset=%td\n",
                 static_cast<unsigned>(this->package.getPackageID()),
                 static_cast<unsigned>(classref.external_classref.class_token),
                 static_cast<unsigned>(class_token),
                 IS_INTERFACE(classFound) ? 1u : 0u,
                 classFound - class_base);
  }
#endif
#ifndef PC_VERSION
  choupi_install_stage(17);
  choupi_install_stage(20);
  choupi_install_context((static_cast<uint32_t>(package.getPackageID()) << 16) |
                         class_token);
#endif

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  uint16_t classref_length = sizeof(jc_cap_class_info);

  if (IS_INTERFACE(classFound)) {
    classref_length = sizeof(struct jc_cap_interface_info);
    classref_length +=
        sizeof(union jc_cap_class_ref) *
        (((struct jc_cap_interface_info *)classFound)->interface_count);
  }

  if ((class_token + classref_length) > HTONS(cap.getClass()->size)) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

#ifndef PC_VERSION
  choupi_install_stage(21);
#endif
  auto resolved = std::make_pair(package, classFound);
#ifndef PC_VERSION
  choupi_install_stage(22);
#endif
  return resolved;
};

/*
 * Converting constant pool offset to struct jc_cap_static_field_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_static_field_ref_info structure.
 */
const jc_cap_static_field_ref_info
ConstantPool_Handler::getStaticFieldRefInfo(jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_STATICFIELDREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.static_field_ref_info;
}

/*
 * Converting constant pool offset to struct jc_cap_static_method_ref_info
 *
 * @param[offset] constant pool index where the type is define.
 *
 * @return a struct jc_cap_static_method_ref_info structure.
 */
const jc_cap_static_method_ref_info
ConstantPool_Handler::getStaticMethodRefInfo(jc_cp_offset_t offset)
#if !defined(JCVM_ARRAY_SIZE_CHECK) && !defined(JCVM_DYNAMIC_CHECKS_CAP)
    noexcept
#endif
{
  const jc_cap_constant_pool_info cp_entry = this->getCPEntry(offset);

#ifdef JCVM_DYNAMIC_CHECKS_CAP

  if (cp_entry.tag != JC_CP_TAG_CONSTANT_STATICMETHODREF) {
    throw Exceptions::SecurityException;
  }

#endif /* JCVM_DYNAMIC_CHECKS_CAP */

  return cp_entry.info.static_method_ref_info;
}

} // namespace jcvm
