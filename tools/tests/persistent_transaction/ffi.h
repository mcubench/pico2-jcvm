#pragma once

#include <cstdint>

extern "C" {
uint8_t fs_exists(const uint8_t *tag, uint8_t tag_length);
uint8_t fs_length(const uint8_t *tag, uint8_t tag_length, uint32_t *length);
uint8_t fs_read(const uint8_t *tag, uint8_t tag_length, uint8_t *data,
                uint32_t length);
uint8_t fs_write(const uint8_t *tag, uint8_t tag_length, const uint8_t *data,
                 uint32_t length);
uint8_t fs_erase(const uint8_t *tag, uint8_t tag_length);
uint8_t fs_read_inplace(const uint8_t *tag, uint8_t tag_length,
                        const uint8_t **data, uint32_t *length);
uint8_t fs_read_1b_at(const uint8_t *tag, uint8_t tag_length,
                      uint32_t offset, uint8_t *value);
uint8_t fs_read_2b_at(const uint8_t *tag, uint8_t tag_length,
                      uint32_t offset, uint16_t *value);
uint8_t fs_read_4b_at(const uint8_t *tag, uint8_t tag_length,
                      uint32_t offset, uint32_t *value);
uint8_t fs_write_1b_at(const uint8_t *tag, uint8_t tag_length,
                       uint32_t offset, uint8_t value);
uint8_t fs_write_2b_at(const uint8_t *tag, uint8_t tag_length,
                       uint32_t offset, uint16_t value);
uint8_t fs_write_4b_at(const uint8_t *tag, uint8_t tag_length,
                       uint32_t offset, uint32_t value);
void path_package_list(uint8_t (*tag)[128], uint8_t *length);
void path_cap(uint8_t package, uint8_t (*tag)[128], uint8_t *length);
void path_static(uint8_t package, uint8_t static_id, uint8_t (*tag)[128],
                 uint8_t *length);
void path_applet_field(uint8_t applet, uint8_t package, uint8_t class_index,
                       uint8_t field, uint8_t (*tag)[128], uint8_t *length);
void path_applet_field16(uint8_t applet, uint8_t package,
                         uint16_t class_index, uint8_t field,
                         uint8_t (*tag)[128], uint8_t *length);
}
