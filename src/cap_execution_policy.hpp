#ifndef CHOUPI_CAP_EXECUTION_POLICY_HPP
#define CHOUPI_CAP_EXECUTION_POLICY_HPP

#include <cstdint>

namespace jcvm::cap_execution_policy {

// GlobalPlatform CAP loading reserves exactly IDs 56 through 63. All CAP
// bytecode in that range is untrusted, regardless of LOAD authentication.
inline constexpr uint8_t kFirstDynamicPackageId = 56;
inline constexpr uint8_t kDynamicPackageCount = 8;

constexpr bool is_dynamic_package(uint8_t package_id) noexcept {
  return package_id >= kFirstDynamicPackageId &&
         package_id < static_cast<uint8_t>(kFirstDynamicPackageId +
                                            kDynamicPackageCount);
}

} // namespace jcvm::cap_execution_policy

#endif // CHOUPI_CAP_EXECUTION_POLICY_HPP
