#ifndef PICO2_JCVM_CAP_PACKAGE_DEPENDENCIES_HPP
#define PICO2_JCVM_CAP_PACKAGE_DEPENDENCIES_HPP

#include "cap_execution_policy.hpp"
#include "cap_store.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_handlers/package.hpp"

#include <cstdint>
#include <cstring>

namespace cap_package_dependencies {

enum class CheckResult : uint8_t {
  no_dependents,
  dependent_package_exists,
  registry_error,
};

inline CheckResult findImporter(const uint8_t package_id) noexcept {
  uint8_t target_aid[16]{};
  uint8_t target_aid_length = 0;
  if (!cap_store_package_aid(package_id, target_aid, target_aid_length) ||
      target_aid_length < 5 || target_aid_length > sizeof(target_aid)) {
    return CheckResult::registry_error;
  }

  try {
    for (uint8_t offset = 0;
         offset < jcvm::cap_execution_policy::kDynamicPackageCount; ++offset) {
      const uint8_t candidate_id = static_cast<uint8_t>(
          jcvm::cap_execution_policy::kFirstDynamicPackageId + offset);
      if (candidate_id == package_id) continue;

      uint32_t candidate_length = 0;
      if (cap_store_data_for_package(candidate_id, &candidate_length) ==
          nullptr) {
        continue;
      }
      if (candidate_length == 0) return CheckResult::registry_error;

      const jcvm::Package candidate(candidate_id);
      const jcvm::JC_Cap cap = candidate.getCap();
      const jcvm::jc_cap_import_component *imports_component = cap.getImport();
      if (imports_component == nullptr) continue;

      jcvm::Import_Handler imports(candidate);
      for (uint8_t index = 0; index < imports_component->count; ++index) {
        const jcvm::jc_cap_package_info *imported =
            imports.getPackageAID(index);
        if (imported == nullptr || imported->AID_length < 5 ||
            imported->AID_length > 16) {
          return CheckResult::registry_error;
        }
        if (imported->AID_length == target_aid_length &&
            std::memcmp(imported->AID, target_aid, target_aid_length) == 0) {
          return CheckResult::dependent_package_exists;
        }
      }
    }
  } catch (...) {
    return CheckResult::registry_error;
  }
  return CheckResult::no_dependents;
}

} // namespace cap_package_dependencies

#endif // PICO2_JCVM_CAP_PACKAGE_DEPENDENCIES_HPP
