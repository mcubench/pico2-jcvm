#pragma once

#include <cstdint>

extern "C" {
int fs_length(...);
int fs_read(...);
int fs_read_inplace(...);
int fs_write(...);
uint8_t fs_read_1b_at(const uint8_t *tag, uint8_t tag_length,
                      uint32_t offset, uint8_t *result);
int fs_read_2b_at(...);
int fs_read_4b_at(...);
int fs_write_1b_at(...);
int fs_write_2b_at(...);
int fs_write_4b_at(...);
void path_applet_field(...);
void path_cap(...);
void path_package_list(...);
void path_static(...);
}
