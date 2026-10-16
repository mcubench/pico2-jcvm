#ifndef PICO2_JCVM_CAP_STORE_HPP
#define PICO2_JCVM_CAP_STORE_HPP

#include <cstddef>
#include <cstdint>

bool cap_store_initialize() noexcept;
bool cap_store_write(const uint8_t *data, size_t length) noexcept;
bool cap_store_delete() noexcept;
bool cap_store_mark_installed(const uint8_t *instance_aid,
                              uint8_t instance_aid_length,
                              uint16_t applet_class_index) noexcept;
const uint8_t *cap_store_data(uint32_t *length) noexcept;
bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept;
bool cap_store_applet_class_index(uint16_t &class_index) noexcept;

#endif
