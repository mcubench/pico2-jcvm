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

#include "jc_import.hpp"
#include "../debug.hpp"
#include "../jc_cap/jc_cap_import.hpp"
#include "jc_cp.hpp"
#include "jc_remote_interface_validation.hpp"
#include "flashmemory.hpp"

#include <cstring>

namespace jcvm {

namespace {
bool imported_aid_matches_resident(const jc_cap_package_info &imported,
                                   const jc_cap_package_info &resident) {
  if (imported.AID_length == resident.AID_length &&
      std::memcmp(imported.AID, resident.AID, imported.AID_length) == 0) {
    return true;
  }

  // CHOUPI's generated JC 3.0.5 ROM packages use an extra 0x02 namespace
  // component in these two local AIDs. CAPs built with the standard JC305
  // exports import their canonical AIDs instead. These explicit aliases let
  // those imports resolve to the same implemented ROM packages.
  static constexpr uint8_t apdu_util_import_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x62, 0x02, 0x09, 0x01};
  static constexpr uint8_t apdu_util_resident_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x62, 0x02, 0x02, 0x08, 0x09, 0x01};
  static constexpr uint8_t sensitive_result_import_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x62, 0x02, 0x05};
  static constexpr uint8_t sensitive_result_resident_aid[] = {
      0xa0, 0x00, 0x00, 0x00, 0x62, 0x02, 0x02, 0x05};

  const bool apdu_util_alias =
      imported.AID_length == sizeof(apdu_util_import_aid) &&
      resident.AID_length == sizeof(apdu_util_resident_aid) &&
      std::memcmp(imported.AID, apdu_util_import_aid,
                  sizeof(apdu_util_import_aid)) == 0 &&
      std::memcmp(resident.AID, apdu_util_resident_aid,
                  sizeof(apdu_util_resident_aid)) == 0;
  const bool sensitive_result_alias =
      imported.AID_length == sizeof(sensitive_result_import_aid) &&
      resident.AID_length == sizeof(sensitive_result_resident_aid) &&
      std::memcmp(imported.AID, sensitive_result_import_aid,
                  sizeof(sensitive_result_import_aid)) == 0 &&
      std::memcmp(resident.AID, sensitive_result_resident_aid,
                  sizeof(sensitive_result_resident_aid)) == 0;
  return apdu_util_alias || sensitive_result_alias;
}
} // namespace

/*
 * Get import package AID from index.
 *
 * @param[index] package index
 * @return the AID of the package.
 */
const jc_cap_package_info *Import_Handler::getPackageAID(const uint8_t index)
#if !defined(JCVM_ARRAY_SIZE_CHECK)
    noexcept
#endif
{
  const JC_Cap &cap = this->package.getCap();
  const jc_cap_import_component *import = cap.getImport();

#ifdef JCVM_ARRAY_SIZE_CHECK

  if (import->count <= index) {

    TRACE_JCVM_DEBUG(
        "Imported package index (%u) > imported packages number (%u)", index,
        import->count);

    throw Exceptions::SecurityException;
  }

#endif /* JCVM_ARRAY_SIZE_CHECK */

  uint16_t pos = 0;
  const jc_cap_package_info *package_info;

  for (uint8_t foo = 0; foo <= index; foo++) {
    package_info = reinterpret_cast<const jc_cap_package_info *>(
        &import->imported_packages[pos]);
    pos += package_info->size();
  }

  return package_info;
}

/*
 * Get the package index in the flash.
 *
 * @param[pinfo] package info which contains the package information to find.
 */
const jpackage_ID_t
Import_Handler::getPackageIndex(const jc_cap_package_info *pinfo) {
  for (jpackage_ID_t index = 0; index < JCVM_MAX_PACKAGES; index++) {
    if (!FlashMemory_Handler::isPackageExist(index)) {
      continue;
    }

    Package package = Package(index);
    JC_Cap cap = package.getCap();

    const jc_cap_package_info &pinfo_to_compare = cap.getHeader()->package;

    // A package import records the version used to convert the client CAP.
    // Java Card linking permits a resident implementation with the same
    // major version and an equal or newer minor version. Exact equality here
    // rejects valid imports such as JC305 javacard.security 1.0 against the
    // embedded compatible 1.6 package.
    if (pinfo->major_version == pinfo_to_compare.major_version &&
        pinfo->minor_version <= pinfo_to_compare.minor_version &&
        imported_aid_matches_resident(*pinfo, pinfo_to_compare)) {
      return index;
    }
  }

  throw Exceptions::RuntimeException;
}

/*
 * Get the package index in the flash from import component offset.
 *
 * @param[offset] Offset of the package information in the current import
 * component
 */
const jpackage_ID_t
Import_Handler::getPackageIndexFromOffset(const uint8_t offset) {
  auto aid = this->getPackageAID(offset);
  return this->getPackageIndex(aid);
}

void Import_Handler::validateRemoteInterfaceReferences() {
  const JC_Cap cap = this->package.getCap();
  const jc_cap_header_component *header = cap.getHeader();
  if (header == nullptr || header->major_version != 2 ||
      header->minor_version < 2) {
    return;
  }
  ConstantPool_Handler resolver(this->package);
  Import_Handler imported_packages(this->package);
  const bool use_jc305_compatibility_tokens =
      usesJC305CompatibilityTokens(this->package);
  validateRemoteInterfaceTargets(
      cap.getClass(), [&resolver, &imported_packages,
                       use_jc305_compatibility_tokens](
                          const jc_cap_class_ref reference) {
        const uint8_t import_token =
            reference.external_classref.package_token & 0x7f;
        const auto *imported_package =
            imported_packages.getPackageAID(import_token);
        const uint8_t export_token =
            use_jc305_compatibility_tokens
                ? remap_imported_interface_token(
                      *imported_package,
                      reference.external_classref.class_token)
                : reference.external_classref.class_token;
        const jpackage_ID_t target_id =
            imported_packages.getPackageIndex(imported_package);
        const jc_cap_export_component *exports =
            Package(target_id).getCap().getExport();
        if (exports == nullptr || export_token >= exports->class_count) {
          return false;
        }

        const auto resolved = resolver.classref2interface(reference);
        return resolved.second != nullptr &&
               !IS_CLASS(reinterpret_cast<const uint8_t *>(resolved.second));
      });
}

} // namespace jcvm
