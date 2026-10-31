#ifndef PICO2_JCVM_FLASH_LAYOUT_HPP
#define PICO2_JCVM_FLASH_LAYOUT_HPP

#include <cstdint>

// All offsets are relative to the reported 2 MiB RP2350 XIP flash.
namespace flash_layout {
constexpr uint32_t firmware_end_limit = 0x100000;
constexpr uint32_t cap_offset = firmware_end_limit;
constexpr uint32_t cap_slot_count = 8;
constexpr uint32_t cap_slot_size = 0x11000;
constexpr uint32_t cap_end = cap_offset + cap_slot_count * cap_slot_size;
constexpr uint32_t filesystem_offset = cap_end;
constexpr uint32_t filesystem_slot_size = 0x28000;
constexpr uint32_t filesystem_slot_count = 2;
// Copy-on-write CAP registry headers live in the gap after filesystem slot 0.
// One erase sector mirrors each of eight dynamic CAP slot headers.
constexpr uint32_t cap_metadata_mirror_offset =
    filesystem_offset + filesystem_slot_size;
constexpr uint32_t cap_metadata_mirror_sector_size = 0x1000;
constexpr uint32_t cap_metadata_mirror_size =
    cap_slot_count * cap_metadata_mirror_sector_size;
constexpr uint32_t legacy_cap2_offset = 0x1c0000;
// CAP2 stores at most 28 KiB after its 4 KiB header. Preserve those bytes
// even though CAP3 no longer interprets the old package ID or metadata.
constexpr uint32_t legacy_cap2_data_end = legacy_cap2_offset + 0x8000;
constexpr uint32_t filesystem_slot1_offset = legacy_cap2_data_end;
constexpr uint32_t filesystem_end =
    filesystem_slot1_offset + filesystem_slot_size;
constexpr uint32_t legacy_filesystem_offset = 0x100000;
constexpr uint32_t legacy_filesystem_slot_size = 0x40000;

constexpr uint32_t filesystem_slot(uint8_t slot) {
  return slot == 0 ? filesystem_offset : filesystem_slot1_offset;
}
constexpr uint32_t legacy_filesystem_slot(uint8_t slot) {
  return legacy_filesystem_offset + static_cast<uint32_t>(slot) *
                                        legacy_filesystem_slot_size;
}
static_assert(cap_offset >= firmware_end_limit, "CAP overlaps firmware");
static_assert(filesystem_offset >= cap_end, "CAP overlaps filesystem");
static_assert(filesystem_offset + filesystem_slot_size <= legacy_cap2_offset,
              "filesystem slot 0 overlaps preserved CAP2 data");
static_assert(cap_metadata_mirror_offset >=
                  filesystem_offset + filesystem_slot_size &&
              cap_metadata_mirror_offset + cap_metadata_mirror_size <=
                  legacy_cap2_offset,
              "CAP metadata mirror overlaps a reserved flash region");
static_assert(filesystem_slot1_offset >= legacy_cap2_data_end,
              "filesystem slot 1 overlaps preserved CAP2 data");
static_assert(filesystem_end <= 0x200000, "flash layout must fit 2 MiB");
static_assert((cap_offset & 0xfff) == 0 && (cap_slot_size & 0xfff) == 0 &&
                  (filesystem_offset & 0xfff) == 0 &&
                  (filesystem_slot_size & 0xfff) == 0 &&
                  (filesystem_slot1_offset & 0xfff) == 0 &&
                  (legacy_filesystem_offset & 0xfff) == 0 &&
                  (legacy_filesystem_slot_size & 0xfff) == 0,
              "flash regions must be erase-sector aligned");
} // namespace flash_layout

#endif
