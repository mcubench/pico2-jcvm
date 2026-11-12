#ifndef PICO2_JCVM_CAP_IMPORT_REFERENCE_VALIDATION_HPP
#define PICO2_JCVM_CAP_IMPORT_REFERENCE_VALIDATION_HPP

#include "jc_handlers/jc_cap.hpp"

#include <cstddef>
#include <cstdint>

namespace cap_import_reference_validation {

inline bool valid_import_package_token(const uint8_t package_token,
                                      const std::size_t import_count) noexcept {
  return (package_token & 0x80u) == 0 ||
         (package_token & 0x7fu) < import_count;
}

inline bool valid_class_import_tokens(const jcvm::JC_Cap &cap) noexcept {
  const auto *class_component = cap.getClass();
  const auto *header = cap.getHeader();
  const auto *imports = cap.getImport();
  if (class_component == nullptr || header == nullptr || imports == nullptr)
    return false;

  const std::size_t import_count = imports->count;
  const auto records = class_component->claz();
  const std::size_t records_size = records.size();
  std::size_t offset = 0;
  if (header->minor_version >= 2) {
    if (!class_component->hasValidSignaturePool()) return false;
    offset = class_component->classInfoStartOffset();
    if (offset > records_size) return false;
  }

  while (offset < records_size) {
    const std::size_t remaining = records_size - offset;
    const uint8_t *record = records.data() + offset;
    if (remaining < 1) return false;

    if ((record[0] & 0x80u) != 0) {
      if (remaining < sizeof(jcvm::jc_cap_interface_info)) return false;
      const auto *interface_info =
          reinterpret_cast<const jcvm::jc_cap_interface_info *>(record);
      const auto references = interface_info->super_interfaces();
      if (references.size() >
          (remaining - sizeof(jcvm::jc_cap_interface_info)) /
              sizeof(jcvm::jc_cap_class_ref)) {
        return false;
      }
      for (std::size_t index = 0; index < references.size(); ++index) {
        const auto &reference = references[index];
        if (reference.isExternalClassRef() &&
            reference.internal_classref != 0xffff &&
            !valid_import_package_token(
                reference.external_classref.package_token, import_count)) {
          return false;
        }
      }
      const std::size_t record_size = interface_info->getSize();
      if (record_size < sizeof(jcvm::jc_cap_interface_info) ||
          record_size > remaining) {
        return false;
      }
      offset += record_size;
      continue;
    }

    if (remaining < sizeof(jcvm::jc_cap_class_info)) return false;
    const auto *class_info =
        reinterpret_cast<const jcvm::jc_cap_class_info *>(record);
    if (!class_info->isObjectClass() &&
        class_info->super_class_ref.isExternalClassRef() &&
        !valid_import_package_token(
            class_info->super_class_ref.external_classref.package_token,
            import_count)) {
      return false;
    }
    const std::size_t record_size = class_info->getSize();
    if (record_size < sizeof(jcvm::jc_cap_class_info) ||
        record_size > remaining) {
      return false;
    }
    offset += record_size;
  }
  return offset == records_size;
}

} // namespace cap_import_reference_validation

#endif // PICO2_JCVM_CAP_IMPORT_REFERENCE_VALIDATION_HPP
