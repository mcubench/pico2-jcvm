#ifndef PICO2_JCVM_CAP_STORE_HPP
#define PICO2_JCVM_CAP_STORE_HPP

#include <cstddef>
#include <cstdint>

bool cap_store_initialize() noexcept;
bool cap_store_find_package(const uint8_t *package_aid,
                            uint8_t package_aid_length,
                            uint8_t &package_id) noexcept;
bool cap_store_write_package(uint8_t package_id, const uint8_t *package_aid,
                             uint8_t package_aid_length, const uint8_t *data,
                             size_t length) noexcept;
bool cap_store_delete_package(uint8_t package_id) noexcept;
const uint8_t *cap_store_data_for_package(uint8_t package_id,
                                          uint32_t *length) noexcept;
bool cap_store_package_aid(uint8_t package_id, uint8_t *aid,
                           uint8_t &aid_length) noexcept;
bool cap_store_allocate_applet_owner(uint8_t &owner_id) noexcept;
bool cap_store_applet_owner(const uint8_t *instance_aid,
                            uint8_t instance_aid_length,
                            uint8_t &owner_id) noexcept;
bool cap_store_mark_package_installed(uint8_t package_id,
                                      const uint8_t *applet_aid,
                                      uint8_t applet_aid_length,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index) noexcept;
bool cap_store_mark_package_installed(uint8_t package_id,
                                      const uint8_t *applet_aid,
                                      uint8_t applet_aid_length,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index,
                                      uint8_t owner_id) noexcept;
bool cap_store_remove_package_instance(uint8_t package_id,
                                       const uint8_t *instance_aid,
                                       uint8_t instance_aid_length) noexcept;
bool cap_store_find_instance(const uint8_t *instance_aid,
                             uint8_t instance_aid_length,
                             uint8_t &package_id, uint8_t *applet_aid,
                             uint8_t &applet_aid_length,
                             uint16_t &class_index) noexcept;
uint8_t cap_store_package_instance_count(uint8_t package_id) noexcept;
bool cap_store_package_instance_at(uint8_t package_id, uint8_t index,
                                   uint8_t *applet_aid,
                                   uint8_t &applet_aid_length,
                                   uint8_t *instance_aid,
                                   uint8_t &instance_aid_length,
                                   uint16_t &class_index) noexcept;

// Legacy single-package wrappers retained while the card-manager call sites
// are migrated to the package-indexed storage interface.
bool cap_store_write(const uint8_t *data, size_t length) noexcept;
bool cap_store_delete() noexcept;
bool cap_store_mark_installed(const uint8_t *instance_aid,
                              uint8_t instance_aid_length,
                              uint16_t applet_class_index) noexcept;
const uint8_t *cap_store_data(uint32_t *length) noexcept;
bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept;
bool cap_store_applet_class_index(uint16_t &class_index) noexcept;

// C-linkage adapter functions implemented by the selected card-storage
// backend and consumed by the JCVM.
#ifdef __cplusplus
extern "C" {
#endif
uint8_t choupi_store_cap_package(uint8_t package_id,
                                 const uint8_t *package_aid,
                                 uint8_t package_aid_length,
                                 const uint8_t *data, uint32_t length);
uint8_t choupi_store_cap_instance(uint8_t package_id,
                                  const uint8_t *applet_aid,
                                  uint8_t applet_aid_length,
                                  const uint8_t *instance_aid,
                                  uint8_t instance_aid_length,
                                  uint16_t applet_class_index,
                                  uint8_t owner_id);
const uint8_t *choupi_cap_store_data(uint32_t *length);
const uint8_t *choupi_cap_store_data_for_package(uint8_t package_id,
                                                uint32_t *length);
#ifdef PC_VERSION
const uint8_t *choupi_pc_rom_package_data(uint8_t package_id,
                                          uint32_t *length);
#endif
#ifdef __cplusplus
}
#endif

#endif
