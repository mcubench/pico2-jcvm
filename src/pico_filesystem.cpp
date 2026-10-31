#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ffi.h"
#include "jc_utils.hpp"
#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"
#include "cap_store.hpp"
#include "flash_layout.hpp"
#include "jc_handlers/flashmemory.hpp"

extern "C" const uint8_t choupi_rommask_start[];
extern "C" const uint8_t choupi_rommask_end[];
extern "C" const uint8_t __flash_binary_end[];
#ifdef RP2350_FS_OVERLAY_TEST
extern "C" const uint8_t *rp2350_fs_test_flash_base(void);
#endif
extern "C" uint8_t choupi_persistent_write_transaction_active(void);

namespace {
constexpr size_t kRomFsOffset = 0x20000;
constexpr size_t kMaxOverlayFiles = 8192;
constexpr uint32_t kFilesystemSlot0 = flash_layout::filesystem_slot(0);
constexpr uint32_t kFilesystemSlotSize = flash_layout::filesystem_slot_size;
constexpr uint32_t kFilesystemSlot1 = flash_layout::filesystem_slot(1);
constexpr uint32_t kFilesystemHeaderSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kFilesystemPayloadOffset = kFilesystemHeaderSize;
constexpr uint32_t kFilesystemXipBase = 0x10000000;
constexpr uint32_t kFilesystemMagic = 0x3253464a; // "JFS2"
constexpr uint16_t kFilesystemVersion = 4;
constexpr uint16_t kFilesystemLegacyVersion = 2;
constexpr uint16_t kFilesystemPreviousVersion = 3;

struct OverlayFile {
  uint16_t hash = 0;
  uint8_t tag_length = 0;
  uint8_t flags = 0;
  uint16_t offset = 0;
  uint16_t size = 0;
};
static_assert(sizeof(OverlayFile) == 8, "overlay index entry must be 8 bytes");
struct __attribute__((packed)) FilesystemRecord {
  uint16_t slot;
  uint16_t hash;
  uint8_t tag_length;
  uint8_t flags;
  uint16_t offset;
  uint16_t size;
};
static_assert(sizeof(FilesystemRecord) == 10,
              "legacy filesystem snapshot index record must be 10 bytes");
struct __attribute__((packed)) FilesystemRecordV4 {
  uint16_t hash;
  uint8_t tag_length;
  uint8_t flags;
  uint16_t offset;
  uint16_t size;
};
static_assert(sizeof(FilesystemRecordV4) == 8,
              "v4 filesystem snapshot index record must be 8 bytes");
constexpr size_t kFilesystemBitmapBytes = (kMaxOverlayFiles + 7) / 8;
// Version 4 replaces each 16-bit slot number with a 1-bit index in a fixed
// 1 KiB occupancy bitmap. Together with compact entries this frees about
// 15 KiB at full index occupancy, enabling a 96 KiB data arena in the
// existing 160 KiB copy-on-write slots.
constexpr size_t kMaxOverlayBytes = 96 * 1024;
constexpr uint32_t kOverlayOffsetHighBit = 0x10000;
constexpr uint32_t kOverlayOffsetHigh2Bit = 0x20000;
static_assert(kMaxOverlayBytes <= 0x3ffff,
              "overlay offsets must fit the two-bit high-offset encoding");
constexpr size_t kFilesystemMaxPayload =
    kFilesystemSlotSize - kFilesystemPayloadOffset;
static_assert(kMaxOverlayFiles * sizeof(FilesystemRecordV4) <=
                  kFilesystemMaxPayload,
              "filesystem index alone must fit in a snapshot slot");
size_t overlay_byte_limit(size_t file_count) {
  if (file_count > kMaxOverlayFiles) return 0;
  const size_t index_bytes = kFilesystemBitmapBytes +
                             file_count * sizeof(FilesystemRecordV4);
  if (index_bytes > kFilesystemMaxPayload) return 0;
  return std::min(kMaxOverlayBytes, kFilesystemMaxPayload - index_bytes);
}
bool filesystem_payload_fits(size_t file_count, size_t bytes_used) {
  return bytes_used <= overlay_byte_limit(file_count);
}
static_assert(kFilesystemSlot1 + kFilesystemSlotSize <=
                  PICO_FLASH_SIZE_BYTES,
              "filesystem slots exceed RP2350 flash");
static_assert(kFilesystemSlot0 >= flash_layout::cap_end,
              "filesystem slots overlap the CAP store");

struct __attribute__((packed)) FilesystemHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t generation;
  uint32_t file_count;
  uint32_t bytes_used;
  uint32_t payload_length;
  uint32_t payload_crc32;
  uint32_t header_crc32;
};

static_assert(sizeof(FilesystemHeader) <= FLASH_PAGE_SIZE,
              "filesystem header must fit in one flash page");

constexpr uint8_t kOverlayUsed = 1u << 0;
constexpr uint8_t kOverlayHasRecord = 1u << 1;
constexpr uint8_t kOverlayCompressedTag = 1u << 2;
constexpr uint8_t kOverlayParentTag = 1u << 3;
constexpr uint8_t kOverlaySerialized = 1u << 4;
constexpr uint8_t kOverlayDeleted = 1u << 5;
constexpr uint8_t kOverlayOffsetHigh = 1u << 6;
constexpr uint8_t kOverlayOffsetHigh2 = 1u << 7;
constexpr uint8_t kOverlayOffsetHighMask =
    kOverlayOffsetHigh | kOverlayOffsetHigh2;
constexpr uint8_t kMaxOverlayTagLength = 128;

uint32_t overlay_offset(const OverlayFile &file) {
  return static_cast<uint32_t>(file.offset) |
         ((file.flags & kOverlayOffsetHigh) != 0
              ? kOverlayOffsetHighBit
              : 0u) |
         ((file.flags & kOverlayOffsetHigh2) != 0
              ? kOverlayOffsetHigh2Bit
              : 0u);
}

void set_overlay_offset(OverlayFile &file, uint32_t offset) {
  file.offset = static_cast<uint16_t>(offset);
  file.flags &= static_cast<uint8_t>(~kOverlayOffsetHighMask);
  if ((offset & kOverlayOffsetHighBit) != 0) {
    file.flags |= kOverlayOffsetHigh;
  }
  if ((offset & kOverlayOffsetHigh2Bit) != 0) {
    file.flags |= kOverlayOffsetHigh2;
  }
}

alignas(FLASH_PAGE_SIZE) OverlayFile overlay[kMaxOverlayFiles];
size_t overlay_file_count = 0;
alignas(FLASH_PAGE_SIZE) uint8_t overlay_bytes[kMaxOverlayBytes];
// Prefix deletion must reclaim payload bytes before attempting small records
// when the fixed overlay is full. Bucket first by tag length (children before
// parents), then by payload size (largest reclaim first).
uint16_t erase_bucket_head[kMaxOverlayTagLength + 1][17];
uint16_t erase_bucket_next[kMaxOverlayFiles];
size_t overlay_bytes_used = 0;
bool overlay_dirty = false;
bool filesystem_layout_available = false;
uint8_t filesystem_active_slot = 0xff;
uint32_t filesystem_generation = 0;

uint32_t __not_in_flash_func(filesystem_crc32)(const uint8_t *data,
                                               size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

void filesystem_crc32_append(uint32_t &crc, const uint8_t *data,
                             size_t length) {
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
}

uint32_t filesystem_payload_crc32(uint32_t bytes_used) {
  uint32_t crc = 0xffffffffu;
  for (size_t byte_index = 0; byte_index < kFilesystemBitmapBytes;
       ++byte_index) {
    uint8_t bitmap_byte = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const size_t slot = byte_index * 8 + bit;
      if (slot < kMaxOverlayFiles &&
          (overlay[slot].flags & kOverlayUsed) != 0) {
        bitmap_byte |= static_cast<uint8_t>(1u << bit);
      }
    }
    filesystem_crc32_append(crc, &bitmap_byte, sizeof(bitmap_byte));
  }
  for (uint16_t slot = 0; slot < kMaxOverlayFiles; ++slot) {
    if ((overlay[slot].flags & kOverlayUsed) == 0) continue;
    filesystem_crc32_append(
        crc, reinterpret_cast<const uint8_t *>(&overlay[slot]),
        sizeof(FilesystemRecordV4));
  }
  filesystem_crc32_append(crc, overlay_bytes, bytes_used);
  return ~crc;
}

const uint8_t *filesystem_slot_data(uint32_t offset) {
#ifdef RP2350_FS_OVERLAY_TEST
  return rp2350_fs_test_flash_base() + offset;
#else
  return reinterpret_cast<const uint8_t *>(kFilesystemXipBase + offset);
#endif
}

const FilesystemHeader *filesystem_slot_header(uint32_t offset) {
  return reinterpret_cast<const FilesystemHeader *>(filesystem_slot_data(offset));
}

bool filesystem_header_valid(const FilesystemHeader &header) {
  const bool supported_version =
      header.version == kFilesystemLegacyVersion ||
      header.version == kFilesystemPreviousVersion ||
      header.version == kFilesystemVersion;
  const uint32_t maximum_bytes = header.version == kFilesystemLegacyVersion
                                    ? UINT16_MAX
                                : header.version ==
                                          kFilesystemPreviousVersion
                                    ? 77824
                                    : kMaxOverlayBytes;
  const uint32_t record_size = header.version == kFilesystemVersion
                                   ? sizeof(FilesystemRecordV4)
                                   : sizeof(FilesystemRecord);
  const uint32_t bitmap_size = header.version == kFilesystemVersion
                                   ? kFilesystemBitmapBytes
                                   : 0;
  return header.magic == kFilesystemMagic &&
         supported_version &&
         header.file_count <= kMaxOverlayFiles &&
         header.bytes_used <= maximum_bytes &&
         header.payload_length ==
             bitmap_size + header.file_count * record_size +
                     header.bytes_used &&
         header.payload_length <= kFilesystemMaxPayload &&
         header.header_crc32 == filesystem_crc32(
             reinterpret_cast<const uint8_t *>(&header),
             offsetof(FilesystemHeader, header_crc32));
}

bool filesystem_slot_valid(uint32_t offset, uint32_t slot_size,
                           bool require_v4) {
  const auto *header = filesystem_slot_header(offset);
  if (!filesystem_header_valid(*header) ||
      kFilesystemPayloadOffset + header->payload_length > slot_size ||
      (require_v4 ? header->version != kFilesystemVersion
                  : header->version >= kFilesystemVersion)) {
    return false;
  }
  const uint8_t *payload = filesystem_slot_data(offset) + kFilesystemPayloadOffset;
  return filesystem_crc32(payload, header->payload_length) ==
         header->payload_crc32;
}

struct FilesystemStream {
  uint32_t next_page_offset;
  size_t page_used;
  alignas(FLASH_PAGE_SIZE) uint8_t page[FLASH_PAGE_SIZE];
};

void __not_in_flash_func(emit_filesystem_byte)(FilesystemStream *stream,
                                                uint8_t byte) {
  stream->page[stream->page_used++] = byte;
  if (stream->page_used == FLASH_PAGE_SIZE) {
    flash_range_program(stream->next_page_offset, stream->page,
                        FLASH_PAGE_SIZE);
    stream->next_page_offset += FLASH_PAGE_SIZE;
    stream->page_used = 0;
    for (size_t index = 0; index < FLASH_PAGE_SIZE; ++index) {
      stream->page[index] = 0xff;
    }
  }
}

struct FilesystemWriteContext {
  uint32_t generation;
  uint32_t slot_offset;
  uint32_t file_count;
  uint32_t bytes_used;
  uint32_t payload_length;
  uint32_t payload_crc32;
  uint32_t erase_size;
  bool success = false;
};

void __not_in_flash_func(write_filesystem_snapshot)(void *opaque) {
  auto *context = static_cast<FilesystemWriteContext *>(opaque);
  flash_range_erase(context->slot_offset, context->erase_size);
  FilesystemStream stream{context->slot_offset + kFilesystemPayloadOffset, 0,
                          {}};
  for (size_t index = 0; index < FLASH_PAGE_SIZE; ++index) {
    stream.page[index] = 0xff;
  }
  for (size_t byte_index = 0; byte_index < kFilesystemBitmapBytes;
       ++byte_index) {
    uint8_t bitmap_byte = 0;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const size_t slot = byte_index * 8 + bit;
      if (slot < kMaxOverlayFiles &&
          (overlay[slot].flags & kOverlayUsed) != 0) {
        bitmap_byte |= static_cast<uint8_t>(1u << bit);
      }
    }
    emit_filesystem_byte(&stream, bitmap_byte);
  }
  for (uint16_t slot = 0; slot < kMaxOverlayFiles; ++slot) {
    if ((overlay[slot].flags & kOverlayUsed) == 0) continue;
    const auto *record_bytes =
        reinterpret_cast<const uint8_t *>(&overlay[slot]);
    for (size_t index = 0; index < sizeof(FilesystemRecordV4); ++index) {
      emit_filesystem_byte(&stream, record_bytes[index]);
    }
  }
  for (uint32_t index = 0; index < context->bytes_used; ++index) {
    emit_filesystem_byte(&stream, overlay_bytes[index]);
  }
  if (stream.page_used != 0) {
    flash_range_program(stream.next_page_offset, stream.page, FLASH_PAGE_SIZE);
  }

  FilesystemHeader header{};
  header.magic = kFilesystemMagic;
  header.version = kFilesystemVersion;
  header.generation = context->generation;
  header.file_count = context->file_count;
  header.bytes_used = context->bytes_used;
  header.payload_length = context->payload_length;
  header.payload_crc32 = context->payload_crc32;
  header.header_crc32 = filesystem_crc32(
      reinterpret_cast<const uint8_t *>(&header),
      offsetof(FilesystemHeader, header_crc32));
  alignas(FLASH_PAGE_SIZE) uint8_t page[FLASH_PAGE_SIZE];
  for (size_t index = 0; index < sizeof(page); ++index) page[index] = 0xff;
  const auto *header_bytes = reinterpret_cast<const uint8_t *>(&header);
  for (size_t index = 0; index < sizeof(header); ++index) {
    page[index] = header_bytes[index];
  }
  flash_range_program(context->slot_offset, page, FLASH_PAGE_SIZE);
  context->success = true;
}

bool persist_overlay() {
  if (!overlay_dirty) return true;
  if (!filesystem_layout_available) return false;
  if (!filesystem_payload_fits(overlay_file_count, overlay_bytes_used)) {
    return false;
  }
  const uint8_t next_slot = filesystem_active_slot == 0 ? 1 : 0;
  const uint32_t slot_offset =
      next_slot == 0 ? kFilesystemSlot0 : kFilesystemSlot1;
  const uint32_t next_generation = filesystem_generation + 1;
  const uint32_t bytes_used = static_cast<uint32_t>(overlay_bytes_used);
  const uint32_t file_count = static_cast<uint32_t>(overlay_file_count);
  const uint32_t payload_length =
      kFilesystemBitmapBytes + file_count * sizeof(FilesystemRecordV4) +
      bytes_used;
  const uint32_t programmed_payload =
      (payload_length + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);
  const uint32_t erase_size =
      (kFilesystemPayloadOffset + programmed_payload + FLASH_SECTOR_SIZE - 1) &
      ~(FLASH_SECTOR_SIZE - 1);
  if (erase_size > kFilesystemSlotSize) return false;
  FilesystemWriteContext context{next_generation,
                                 slot_offset,
                                 file_count,
                                 bytes_used,
                                 payload_length,
                                 filesystem_payload_crc32(bytes_used),
                                 erase_size,
                                 false};
  const int status = flash_safe_execute(write_filesystem_snapshot, &context,
                                        5000);
  if (status != PICO_OK || !context.success ||
      !filesystem_slot_valid(slot_offset, kFilesystemSlotSize, true)) {
#ifndef RP2350_FS_OVERLAY_TEST
    std::printf("CHOUPI:FS_SYNC_FAIL status=%d slot=%u\r\n", status,
                static_cast<unsigned>(next_slot));
    std::fflush(stdout);
#endif
    return false;
  }
  filesystem_active_slot = next_slot;
  filesystem_generation = next_generation;
  overlay_dirty = false;
  return true;
}

bool restore_overlay_slot(uint32_t offset, uint8_t slot,
                          bool migration_source) {
  for (auto &entry : overlay) entry = OverlayFile{};
  overlay_file_count = 0;
  overlay_bytes_used = 0;
  const auto *header = filesystem_slot_header(offset);
  const uint8_t *payload = filesystem_slot_data(offset) + kFilesystemPayloadOffset;
  bool valid = true;
  if (header->version == kFilesystemVersion) {
    const uint8_t *bitmap = payload;
    payload += kFilesystemBitmapBytes;
    uint32_t restored_count = 0;
    for (uint16_t slot_index = 0; slot_index < kMaxOverlayFiles;
         ++slot_index) {
      if ((bitmap[slot_index / 8] & (1u << (slot_index % 8))) == 0) continue;
      if (restored_count >= header->file_count) {
        valid = false;
        break;
      }
      std::memcpy(&overlay[slot_index], payload, sizeof(OverlayFile));
      payload += sizeof(FilesystemRecordV4);
      if ((overlay[slot_index].flags & kOverlayUsed) == 0) {
        valid = false;
        break;
      }
      ++restored_count;
    }
    if (restored_count != header->file_count) valid = false;
  } else {
    for (uint32_t index = 0; index < header->file_count; ++index) {
      uint16_t slot_index = 0;
      std::memcpy(&slot_index, payload, sizeof(slot_index));
      if (slot_index >= kMaxOverlayFiles ||
          (overlay[slot_index].flags & kOverlayUsed) != 0) {
        valid = false;
        break;
      }
      std::memcpy(&overlay[slot_index], payload + sizeof(slot_index),
                  sizeof(OverlayFile));
      if ((overlay[slot_index].flags & kOverlayUsed) == 0) {
        valid = false;
        break;
      }
      payload += sizeof(FilesystemRecord);
    }
  }
  if (valid) {
    std::memcpy(overlay_bytes, payload, header->bytes_used);
    overlay_bytes_used = header->bytes_used;
    for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
      auto &file = overlay[index];
      const uint32_t offset = overlay_offset(file);
      if ((file.flags & kOverlayUsed) != 0 &&
           ((header->version == kFilesystemLegacyVersion &&
             (file.flags & kOverlayOffsetHighMask) != 0) ||
            (header->version == kFilesystemPreviousVersion &&
             (file.flags & kOverlayOffsetHigh2) != 0) ||
           offset >= header->bytes_used ||
           (overlay_bytes[offset] & 0x7f) > header->bytes_used - offset ||
           file.size > header->bytes_used - offset - 1 -
                           (overlay_bytes[offset] & 0x7f))) {
        valid = false;
        break;
      }
    }
  }
  if (!valid) {
    for (auto &entry : overlay) entry = OverlayFile{};
    overlay_file_count = 0;
    overlay_bytes_used = 0;
    return false;
  }
  overlay_file_count = header->file_count;
  overlay_bytes_used = header->bytes_used;
  overlay_dirty = false;
  filesystem_active_slot = migration_source ? 0xff : slot;
  filesystem_generation = header->generation;
  return true;
}

bool restore_overlay_snapshot(uint32_t offset0, uint32_t offset1,
                              uint32_t slot_size, bool migration_source,
                              bool require_v4) {
  const bool valid0 = filesystem_slot_valid(offset0, slot_size, require_v4);
  const bool valid1 = filesystem_slot_valid(offset1, slot_size, require_v4);
  if (!valid0 && !valid1) return false;
  uint8_t first = valid0 ? 0 : 1;
  if (valid0 && valid1 &&
      static_cast<int32_t>(filesystem_slot_header(offset1)->generation -
                           filesystem_slot_header(offset0)->generation) > 0) {
    first = 1;
  }
  const uint32_t first_offset = first == 0 ? offset0 : offset1;
  if (restore_overlay_slot(first_offset, first, migration_source)) return true;
  const uint8_t other = first == 0 ? 1 : 0;
  if (!(other == 0 ? valid0 : valid1)) return false;
  return restore_overlay_slot(other == 0 ? offset0 : offset1, other,
                              migration_source);
}

void report_fs_write_failure(const char *operation, const uint8_t *tag,
                             uint8_t tag_length, uint32_t length) {
  std::printf("CHOUPI:FS_WRITE_FAIL op=%s tag_len=%u size=%lu files=%u bytes=%lu/%lu tag=",
              operation, static_cast<unsigned>(tag_length),
              static_cast<unsigned long>(length),
              static_cast<unsigned>(overlay_file_count),
              static_cast<unsigned long>(overlay_bytes_used),
              static_cast<unsigned long>(kMaxOverlayBytes));
  for (uint8_t index = 0; index < tag_length && index < 8; ++index) {
    std::printf("%02X", tag[index]);
  }
  std::printf("\r\n");
  std::fflush(stdout);
}

void report_fs_write_rejection(const char *reason, const uint8_t *tag,
                               uint8_t tag_length, uint32_t offset,
                               uint32_t length, uint32_t file_length) {
  std::printf("CHOUPI:FS_WRITE_REJECT reason=%s tag_len=%u offset=%lu size=%lu file_size=%lu files=%u bytes=%lu/%lu tag=",
              reason, static_cast<unsigned>(tag_length),
              static_cast<unsigned long>(offset),
              static_cast<unsigned long>(length),
              static_cast<unsigned long>(file_length),
              static_cast<unsigned>(overlay_file_count),
              static_cast<unsigned long>(overlay_bytes_used),
              static_cast<unsigned long>(kMaxOverlayBytes));
  for (uint8_t index = 0; index < tag_length && index < 8; ++index) {
    std::printf("%02X", tag[index]);
  }
  std::printf("\r\n");
  std::fflush(stdout);
}

bool tags_equal(const uint8_t *left, uint8_t left_length,
                const uint8_t *right, uint8_t right_length) {
  return left_length == right_length &&
         std::memcmp(left, right, left_length) == 0;
}

uint8_t tag_root_length(const uint8_t *tag, uint8_t tag_length) {
  if (tag_length == 0) {
    return 0;
  }
  switch (tag[0]) {
  case 0x00:
    return tag_length >= 1 ? 1 : 0;
  case 0x01:
    return tag_length >= 2 ? 2 : 0;
  case 0x02:
    return tag_length >= 3 ? 3 : 0;
  case 0x03:
    if (tag_length >= 6 && (tag_length & 1u) == 0) return 6;
    return tag_length >= 5 ? 5 : 0;
  default:
    return 0;
  }
}

bool is_compressible_tag(const uint8_t *tag, uint8_t tag_length) {
  const uint8_t root_length = tag_root_length(tag, tag_length);
  if (root_length == 0 || ((tag_length - root_length) & 1) != 0) {
    return false;
  }
  for (uint8_t index = root_length; index < tag_length; index += 2) {
    if (tag[index] != tag[index + 1]) {
      return false;
    }
  }
  return tag_length > root_length;
}

uint8_t stored_tag_length(const OverlayFile &file) {
  return overlay_bytes[overlay_offset(file)] & 0x7F;
}

bool overlay_is_used(const OverlayFile &file) {
  return (file.flags & kOverlayUsed) != 0;
}

bool overlay_has_record(const OverlayFile &file) {
  return (file.flags & kOverlayHasRecord) != 0 &&
         (file.flags & kOverlayDeleted) == 0;
}

bool overlay_has_storage(const OverlayFile &file) {
  return (file.flags & kOverlaySerialized) != 0;
}

bool overlay_compresses_tag(const OverlayFile &file) {
  return (file.flags & kOverlayCompressedTag) != 0;
}

bool stored_tag_matches(const OverlayFile &file, const uint8_t *tag,
                        uint8_t tag_length);

OverlayFile *find_overlay(const uint8_t *tag, uint8_t tag_length,
                          size_t *found_slot = nullptr);

bool find_parent_slot(const uint8_t *tag, uint8_t tag_length,
                      uint16_t *parent_slot) {
  const uint8_t root_length = tag_root_length(tag, tag_length);
  if (root_length == 0 || tag_length < root_length + 2 ||
      tag[tag_length - 2] != tag[tag_length - 1]) {
    return false;
  }
  size_t slot = 0;
  if (find_overlay(tag, tag_length - 2, &slot) == nullptr) {
    return false;
  }
  if (!overlay_has_record(overlay[slot])) {
    return false;
  }
  *parent_slot = static_cast<uint16_t>(slot);
  return true;
}

uint8_t encoded_tag_length(const uint8_t *tag, uint8_t tag_length) {
  if (!is_compressible_tag(tag, tag_length)) {
    return tag_length;
  }
  const uint8_t root_length = tag_root_length(tag, tag_length);
  return static_cast<uint8_t>(root_length + (tag_length - root_length) / 2);
}

uint8_t overlay_key_length(const OverlayFile &file) {
  if ((file.flags & kOverlayParentTag) != 0) {
    return 3;
  }
  if (overlay_compresses_tag(file)) {
    const uint8_t root_length = tag_root_length(
        overlay_bytes + overlay_offset(file) + 1, file.tag_length);
    return static_cast<uint8_t>(root_length + (file.tag_length - root_length) / 2);
  }
  return file.tag_length;
}

bool stored_tag_matches(const OverlayFile &file, const uint8_t *tag,
                        uint8_t tag_length) {
  const uint32_t offset = overlay_offset(file);
  const uint8_t header = overlay_bytes[offset];
  const uint8_t *stored = overlay_bytes + offset + 1;
  if ((header & 0x80) != 0) {
    const uint16_t parent_slot = static_cast<uint16_t>(stored[0]) |
                                 static_cast<uint16_t>(stored[1] << 8);
    if (tag_length < 2 || parent_slot >= kMaxOverlayFiles ||
        !overlay_has_record(overlay[parent_slot]) ||
        overlay[parent_slot].tag_length != tag_length - 2 ||
        tag[tag_length - 2] != tag[tag_length - 1] ||
        stored[2] != tag[tag_length - 1]) {
      return false;
    }
    return stored_tag_matches(overlay[parent_slot], tag, tag_length - 2);
  }
  if (!overlay_compresses_tag(file)) {
    return tags_equal(stored, file.tag_length, tag, tag_length);
  }
  const uint8_t root_length = tag_root_length(tag, tag_length);
  if (root_length == 0 || tag_length <= root_length ||
      ((tag_length - root_length) & 1) != 0 ||
      !tags_equal(stored, root_length, tag, root_length)) {
    return false;
  }
  uint8_t stored_index = root_length;
  for (uint8_t index = root_length; index < tag_length; index += 2) {
    if (tag[index] != tag[index + 1] || stored[stored_index++] != tag[index]) {
      return false;
    }
  }
  return true;
}

uint8_t *overlay_data(OverlayFile &file) {
  return overlay_bytes + overlay_offset(file) + 1 + stored_tag_length(file);
}

uint16_t hash_tag(const uint8_t *tag, uint8_t tag_length) {
  uint32_t hash = 2166136261u;
  for (uint8_t index = 0; index < tag_length; ++index) {
    hash = (hash ^ tag[index]) * 16777619u;
  }
  const uint16_t compact_hash = static_cast<uint16_t>(hash ^ (hash >> 16));
  return compact_hash == 0 ? 1 : compact_hash;
}

OverlayFile *find_overlay(const uint8_t *tag, uint8_t tag_length,
                          size_t *found_slot) {
  const uint16_t hash = hash_tag(tag, tag_length);
  size_t slot = hash % kMaxOverlayFiles;
  OverlayFile *newest_live = nullptr;
  OverlayFile *newest_tombstone = nullptr;
  size_t newest_live_slot = 0;
  size_t newest_tombstone_slot = 0;
  for (size_t probes = 0; probes < kMaxOverlayFiles; ++probes) {
    auto &file = overlay[slot];
    if (!overlay_is_used(file)) {
      break;
    }
    if (file.hash == hash && file.tag_length == tag_length &&
        overlay_has_storage(file)) {
      const bool matches = stored_tag_matches(file, tag, tag_length);
      if (matches) {
        if (overlay_has_record(file)) {
          if (newest_live == nullptr ||
              overlay_offset(file) > overlay_offset(*newest_live)) {
            newest_live = &file;
            newest_live_slot = slot;
          }
        } else if (newest_tombstone == nullptr ||
                   overlay_offset(file) >
                       overlay_offset(*newest_tombstone)) {
          newest_tombstone = &file;
          newest_tombstone_slot = slot;
        }
      }
    }
    slot = (slot + 1) % kMaxOverlayFiles;
  }
  OverlayFile *selected = newest_live != nullptr ? newest_live
                                                  : newest_tombstone;
  if (selected != nullptr && found_slot != nullptr) {
    *found_slot = newest_live != nullptr ? newest_live_slot
                                         : newest_tombstone_slot;
  }
  return selected;
}

OverlayFile *allocate_overlay(const uint8_t *tag, uint8_t tag_length) {
  if (tag_length > 128 ||
      (tag_length > 127 && !is_compressible_tag(tag, tag_length))) {
    return nullptr;
  }
  if (auto *file = find_overlay(tag, tag_length)) {
    if (!overlay_has_record(*file)) {
      file->flags &= static_cast<uint8_t>(~kOverlayParentTag);
      if (is_compressible_tag(tag, tag_length)) {
        file->flags |= kOverlayCompressedTag;
      }
    }
    return file;
  }
  const uint16_t hash = hash_tag(tag, tag_length);
  size_t slot = hash % kMaxOverlayFiles;
  for (size_t probes = 0; probes < kMaxOverlayFiles; ++probes) {
    auto &file = overlay[slot];
    if (!overlay_is_used(file)) {
      file = OverlayFile{};
      file.flags = kOverlayUsed;
      file.hash = hash;
      file.tag_length = tag_length;
      if (is_compressible_tag(tag, tag_length)) {
        file.flags |= kOverlayCompressedTag;
      }
      ++overlay_file_count;
      return &file;
    }
    slot = (slot + 1) % kMaxOverlayFiles;
  }
  return nullptr;
}

bool reconstruct_overlay_tag(uint16_t slot, uint8_t *tag,
                             uint8_t *tag_length,
                             uint8_t depth = 0) {
  if (slot >= kMaxOverlayFiles || tag == nullptr || tag_length == nullptr ||
      depth > 64) {
    return false;
  }
  const OverlayFile &file = overlay[slot];
  if (!overlay_is_used(file) || !overlay_has_storage(file) ||
      file.tag_length == 0 || file.tag_length > 128 ||
      overlay_offset(file) >= overlay_bytes_used) {
    return false;
  }

  const uint32_t offset = overlay_offset(file);
  const uint8_t header = overlay_bytes[offset];
  const uint8_t key_length = header & 0x7f;
  const uint8_t *stored = overlay_bytes + offset + 1;
  if ((header & 0x80) != 0) {
    if (key_length != 3 || file.tag_length < 2) return false;
    const uint16_t parent_slot = static_cast<uint16_t>(stored[0]) |
                                 static_cast<uint16_t>(stored[1] << 8);
    uint8_t parent_length = 0;
    if (!reconstruct_overlay_tag(parent_slot, tag, &parent_length,
                                 static_cast<uint8_t>(depth + 1)) ||
        parent_length + 2 != file.tag_length) {
      return false;
    }
    tag[parent_length] = stored[2];
    tag[parent_length + 1] = stored[2];
    *tag_length = file.tag_length;
    return true;
  }

  if (overlay_compresses_tag(file)) {
    const uint8_t root_length = tag_root_length(stored, file.tag_length);
    if (root_length == 0 || file.tag_length <= root_length ||
        ((file.tag_length - root_length) & 1u) != 0 ||
        key_length != root_length + (file.tag_length - root_length) / 2) {
      return false;
    }
    std::memcpy(tag, stored, root_length);
    uint8_t stored_index = root_length;
    for (uint8_t index = root_length; index < file.tag_length; index += 2) {
      tag[index] = stored[stored_index++];
      tag[index + 1] = tag[index];
    }
  } else {
    if (key_length != file.tag_length) return false;
    std::memcpy(tag, stored, file.tag_length);
  }
  *tag_length = file.tag_length;
  return true;
}

bool append_overlay(OverlayFile &file, const uint8_t *tag, const uint8_t *data,
                    uint32_t length) {
  const bool parent_tag = (file.flags & kOverlayParentTag) != 0;
  const uint8_t key_length = parent_tag
                                 ? 3
                                 : (overlay_compresses_tag(file)
                                        ? static_cast<uint8_t>(
                                              tag_root_length(tag, file.tag_length) +
                                              (file.tag_length -
                                               tag_root_length(tag, file.tag_length)) /
                                                  2)
                                        : file.tag_length);
  if (key_length > 0x7F) {
    return false;
  }
  const size_t old_record_size = overlay_has_storage(file)
                                     ? 1 + stored_tag_length(file) + file.size
                                     : 0;
  const size_t new_record_size = 1 + key_length + length;
  const size_t new_used = overlay_bytes_used - old_record_size + new_record_size;
  if (!filesystem_payload_fits(overlay_file_count, new_used)) {
    return false;
  }
  const size_t old_offset = overlay_offset(file);
  if (old_record_size != 0) {
    std::memmove(overlay_bytes + old_offset,
                 overlay_bytes + old_offset + old_record_size,
                 overlay_bytes_used - old_offset - old_record_size);
    for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
      if (&overlay[index] != &file && overlay_has_storage(overlay[index]) &&
          overlay_offset(overlay[index]) > old_offset) {
        set_overlay_offset(
            overlay[index],
            overlay_offset(overlay[index]) -
                static_cast<uint32_t>(old_record_size));
      }
    }
  }
  set_overlay_offset(file, static_cast<uint32_t>(overlay_bytes_used -
                                                  old_record_size));
  const uint32_t new_offset = overlay_offset(file);
  overlay_bytes[new_offset] = static_cast<uint8_t>(
      key_length | (parent_tag ? 0x80 : 0));
  if (parent_tag) {
    uint16_t parent_slot = 0;
    if (!find_parent_slot(tag, file.tag_length, &parent_slot)) {
      return false;
    }
    overlay_bytes[new_offset + 1] = static_cast<uint8_t>(parent_slot);
    overlay_bytes[new_offset + 2] = static_cast<uint8_t>(parent_slot >> 8);
    overlay_bytes[new_offset + 3] = tag[file.tag_length - 1];
  } else if (overlay_compresses_tag(file)) {
    const uint8_t root_length = tag_root_length(tag, file.tag_length);
    std::memcpy(overlay_bytes + new_offset + 1, tag, root_length);
    uint8_t stored_index = root_length;
    for (uint8_t index = root_length; index < file.tag_length; index += 2) {
      overlay_bytes[new_offset + 1 + stored_index++] = tag[index];
    }
  } else if (file.tag_length != 0) {
    std::memcpy(overlay_bytes + new_offset + 1, tag, file.tag_length);
  }
  if (length != 0) {
    std::memcpy(overlay_bytes + new_offset + 1 + key_length, data,
                length);
  }
  file.size = length;
  file.flags |= kOverlayHasRecord;
  file.flags |= kOverlaySerialized;
  file.flags &= static_cast<uint8_t>(~kOverlayDeleted);
  overlay_bytes_used = new_used;
  return true;
}

bool find_rom_file(const uint8_t *tag, uint8_t tag_length,
                   const uint8_t **data, uint32_t *length) {
  const size_t image_size =
      static_cast<size_t>(choupi_rommask_end - choupi_rommask_start);
  if (image_size <= kRomFsOffset) {
    return false;
  }

  size_t position = kRomFsOffset;
  while (position < image_size) {
    const uint8_t header = choupi_rommask_start[position++];
    if (header == 0 || (header & 0x40) == 0) {
      return false;
    }

    const uint8_t block_tag_length = static_cast<uint8_t>((header >> 1) & 0x1F);
    uint32_t block_length = 0;
    if ((header & 1) != 0) {
      if (image_size - position < 4) {
        return false;
      }
      block_length = (static_cast<uint32_t>(choupi_rommask_start[position]) << 24) |
                     (static_cast<uint32_t>(choupi_rommask_start[position + 1]) << 16) |
                     (static_cast<uint32_t>(choupi_rommask_start[position + 2]) << 8) |
                     choupi_rommask_start[position + 3];
      position += 4;
    } else {
      if (position == image_size) {
        return false;
      }
      block_length = choupi_rommask_start[position++];
    }

    if (block_tag_length > image_size - position ||
        block_length > image_size - position - block_tag_length ||
        image_size - position - block_tag_length - block_length < 1) {
      return false;
    }
    const uint8_t *block_tag = choupi_rommask_start + position;
    const uint8_t *block_data = block_tag + block_tag_length;
    if (tags_equal(block_tag, block_tag_length, tag, tag_length)) {
      *data = block_data;
      *length = block_length;
      return true;
    }
    position += block_tag_length + block_length + 1; // tag, data, CRC-8
  }
  return false;
}

bool find_file(const uint8_t *tag, uint8_t tag_length,
               const uint8_t **data, uint32_t *length) {
  if (auto *file = find_overlay(tag, tag_length)) {
    if (!overlay_has_record(*file)) {
      return false;
    }
    *data = overlay_data(*file);
    *length = file->size;
    return true;
  }
  return find_rom_file(tag, tag_length, data, length);
}

bool write_at(const uint8_t *tag, uint8_t tag_length, uint32_t offset,
              const uint8_t *data, uint32_t length) {
  const uint8_t *existing = nullptr;
  uint32_t existing_length = 0;
  if (!find_file(tag, tag_length, &existing, &existing_length)) {
    report_fs_write_rejection("missing-record", tag, tag_length, offset,
                              length, 0);
    return false;
  }
  if (offset > existing_length || length > existing_length - offset) {
    report_fs_write_rejection("out-of-range", tag, tag_length, offset,
                              length, existing_length);
    return false;
  }
  auto *file = find_overlay(tag, tag_length);
  if (file != nullptr) {
    if (file->size != existing_length) {
      report_fs_write_failure("update-size", tag, tag_length, existing_length);
      return false;
    }
    std::memcpy(overlay_data(*file) + offset, data, length);
    overlay_dirty = true;
    return true;
  }

  const size_t record_key_length = encoded_tag_length(tag, tag_length);
  if (!filesystem_payload_fits(
          overlay_file_count + (file == nullptr ? 1u : 0u),
          overlay_bytes_used + 1 + record_key_length + existing_length)) {
    report_fs_write_failure("update", tag, tag_length, existing_length);
    return false;
  }
  file = allocate_overlay(tag, tag_length);
  if (file == nullptr) {
    report_fs_write_rejection("index-full", tag, tag_length, offset, length,
                              existing_length);
    return false;
  }
  set_overlay_offset(*file, static_cast<uint32_t>(overlay_bytes_used));
  if (!append_overlay(*file, tag, existing, existing_length)) {
    report_fs_write_rejection("overlay-capacity", tag, tag_length, offset,
                              length, existing_length);
    return false;
  }
  std::memcpy(overlay_data(*file) + offset, data, length);
  overlay_dirty = true;
  return true;
}

void set_path(uint8_t (*tag)[128], uint8_t *length,
              std::initializer_list<uint8_t> bytes) {
  *length = static_cast<uint8_t>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), (*tag));
}

} // namespace

extern "C" uint32_t choupi_available_persistent_memory_bytes(void) {
  if (!filesystem_layout_available || overlay_file_count >= kMaxOverlayFiles ||
      overlay_bytes_used >= kMaxOverlayBytes) {
    return 0;
  }
  const size_t limit = overlay_byte_limit(overlay_file_count + 1);
  return overlay_bytes_used >= limit
             ? 0
             : static_cast<uint32_t>(limit - overlay_bytes_used);
}

extern "C" uint8_t fs_init(void) {
  for (auto &file : overlay) {
    file = OverlayFile{};
  }
  overlay_file_count = 0;
  overlay_bytes_used = 0;
  overlay_dirty = false;
  filesystem_layout_available = false;
  filesystem_active_slot = 0xff;
  filesystem_generation = 0;
#ifdef RP2350_FS_OVERLAY_TEST
  filesystem_layout_available = true;
#else
  const uintptr_t image_end = reinterpret_cast<uintptr_t>(__flash_binary_end);
  filesystem_layout_available =
      image_end < kFilesystemXipBase + flash_layout::firmware_end_limit;
#endif
  if (!filesystem_layout_available || !cap_store_initialize()) return 1;
  const bool restored_current = restore_overlay_snapshot(
      kFilesystemSlot0, kFilesystemSlot1, kFilesystemSlotSize, false, true);
  const bool restored_previous =
      !restored_current && restore_overlay_snapshot(
                               kFilesystemSlot0, kFilesystemSlot1,
                               kFilesystemSlotSize, false, false);
  const bool restored_legacy =
      !restored_current && !restored_previous &&
      restore_overlay_snapshot(flash_layout::legacy_filesystem_slot(0),
                               flash_layout::legacy_filesystem_slot(1),
                               flash_layout::legacy_filesystem_slot_size, true,
                               false);
  if (restored_previous || restored_legacy) {
    // Version migration writes the restored image to the opposite COW slot,
    // preserving the selected source until the compact header validates.
    // Older pre-CAP3 snapshots occupy separate locations and remain untouched.
    overlay_dirty = true;
    if (!persist_overlay()) return 1;
    std::printf("CHOUPI:FS_MIGRATE generation=%lu\r\n",
                static_cast<unsigned long>(filesystem_generation));
    std::fflush(stdout);
  }
  if (!restored_current && !restored_previous && !restored_legacy) {
    bool stored_cap_present = false;
    for (uint8_t package_id = 56; package_id < 64; ++package_id) {
      uint32_t length = 0;
      stored_cap_present |=
          cap_store_data_for_package(package_id, &length) != nullptr;
    }
    const bool snapshot_header_present =
        filesystem_slot_header(kFilesystemSlot0)->magic == kFilesystemMagic ||
        filesystem_slot_header(kFilesystemSlot1)->magic == kFilesystemMagic ||
        filesystem_slot_header(flash_layout::legacy_filesystem_slot(0))->magic ==
            kFilesystemMagic ||
        filesystem_slot_header(flash_layout::legacy_filesystem_slot(1))->magic ==
            kFilesystemMagic;
    if (stored_cap_present || snapshot_header_present) {
      std::printf("FAULT FS snapshot unavailable; preserve flash for recovery\r\n");
      std::fflush(stdout);
      return 1;
    }
  }
  return 0;
}

extern "C" uint8_t fs_sync(void) {
  if (choupi_persistent_write_transaction_active() != 0) return 0;
  return persist_overlay() ? 0 : 1;
}

extern "C" void fs_drop(void) { (void)fs_sync(); }

extern "C" uint8_t fs_exists(const uint8_t *tag, uint8_t tag_length) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  return find_file(tag, tag_length, &data, &length) ? 1 : 0;
}

// Enumerate the mutable persistent overlay. The cursor is a slot index in the
// Pico overlay; storage mutation invalidates it. ROM-mask defaults are not
// included because they are immutable and cannot consume persistent space.
extern "C" uint8_t fs_next_tag(const uint8_t *prefix, uint8_t prefix_length,
                               fs_tag_next_request_t *request) {
  static_assert(sizeof(fs_tag_next_request_t) == 134,
                "filesystem cursor C ABI must match the OS backend");
  if (prefix == nullptr || request == nullptr ||
      prefix_length > kMaxOverlayTagLength || request->offset != 0 ||
      request->bucket > kMaxOverlayFiles) {
    return 2;
  }

  for (uint16_t slot = request->bucket; slot < kMaxOverlayFiles; ++slot) {
    request->bucket = static_cast<uint16_t>(slot + 1);
    const OverlayFile &file = overlay[slot];
    if (!overlay_is_used(file) || !overlay_has_record(file)) continue;

    uint8_t tag[kMaxOverlayTagLength]{};
    uint8_t tag_length = 0;
    if (!reconstruct_overlay_tag(slot, tag, &tag_length) || tag_length == 0 ||
        tag_length > sizeof(request->tag) - 1) {
      return 2;
    }
    if (prefix_length > tag_length ||
        std::memcmp(tag, prefix, prefix_length) != 0) {
      continue;
    }

    request->tag[0] = tag_length;
    std::memcpy(request->tag + 1, tag, tag_length);
    return 0;
  }

  request->bucket = static_cast<uint16_t>(kMaxOverlayFiles);
  return 1;
}

extern "C" uint8_t fs_length(const uint8_t *tag, uint8_t tag_length,
                              uint32_t *length) {
  const uint8_t *data = nullptr;
  return find_file(tag, tag_length, &data, length) ? 0 : 1;
}

extern "C" uint8_t fs_read_inplace(const uint8_t *tag, uint8_t tag_length,
                                   const uint8_t **data, uint32_t *length) {
  return find_file(tag, tag_length, data, length) ? 0 : 1;
}

extern "C" uint8_t fs_read(const uint8_t *tag, uint8_t tag_length,
                           uint8_t *destination, uint32_t length) {
  const uint8_t *source = nullptr;
  uint32_t source_length = 0;
  if (!find_file(tag, tag_length, &source, &source_length) ||
      length > source_length) {
    return 1;
  }
  std::memcpy(destination, source, length);
  return 0;
}

extern "C" uint8_t fs_write(const uint8_t *tag, uint8_t tag_length,
                            const uint8_t *data, uint32_t length) {
  auto *file = find_overlay(tag, tag_length);
  const size_t old_record_size =
      file == nullptr ? 0 : 1 + stored_tag_length(*file) + file->size;
  const size_t record_key_length =
      file == nullptr ? encoded_tag_length(tag, tag_length)
                      : overlay_key_length(*file);
  const size_t new_file_count =
      overlay_file_count + (file == nullptr ? 1u : 0u);
  const size_t new_bytes_used =
      overlay_bytes_used - old_record_size + 1 + record_key_length + length;
  if (!filesystem_payload_fits(new_file_count, new_bytes_used) ||
      (file == nullptr && overlay_file_count == kMaxOverlayFiles)) {
    report_fs_write_failure("new", tag, tag_length, length);
    return 1;
  }
  if (file == nullptr) {
    file = allocate_overlay(tag, tag_length);
  }
  if (file == nullptr || !append_overlay(*file, tag, data, length)) {
    report_fs_write_failure("new", tag, tag_length, length);
    return 1;
  }
  overlay_dirty = true;
  return 0;
}

#ifdef RP2350_FS_OVERLAY_TEST
extern "C" uint8_t rp2350_fs_test_write_parent_encoded(
    const uint8_t *tag, uint8_t tag_length, const uint8_t *data,
    uint32_t length) {
  if (tag == nullptr || tag_length < 2) return 1;
  auto *file = allocate_overlay(tag, tag_length);
  if (file == nullptr) return 1;
  file->flags &= static_cast<uint8_t>(~kOverlayCompressedTag);
  file->flags |= kOverlayParentTag;
  if (!append_overlay(*file, tag, data, length)) return 1;
  overlay_dirty = true;
  return 0;
}
#endif

namespace {
uint8_t erase_overlay_slot(uint16_t slot) {
  if (slot >= kMaxOverlayFiles) return 1;
  auto &file = overlay[slot];
  if (!overlay_is_used(file) || !overlay_has_record(file) ||
      !overlay_has_storage(file) ||
      overlay_offset(file) >= overlay_bytes_used) {
    return 1;
  }

  // Re-encode the existing key bytes verbatim. In particular, a compressed
  // child can outlive a parent that was deleted by an older firmware; its
  // canonical tag can be reconstructed, while find_overlay() cannot resolve
  // it through the now-deleted parent. Preserving the encoding lets us write
  // a durable tombstone without resolving that parent again.
  const uint32_t file_offset = overlay_offset(file);
  const uint8_t key_header = overlay_bytes[file_offset];
  const size_t key_length = key_header & 0x7f;
  const size_t old_record_size = 1 + key_length + file.size;
  if (old_record_size > overlay_bytes_used - file_offset) return 1;
  uint8_t encoded_key[128]{};
  if (key_length + 1 > sizeof(encoded_key)) return 1;
  std::memcpy(encoded_key, overlay_bytes + file_offset, key_length + 1);

  const size_t old_offset = file_offset;
  const size_t remaining = overlay_bytes_used - old_offset - old_record_size;
  std::memmove(overlay_bytes + old_offset,
               overlay_bytes + old_offset + old_record_size, remaining);
  for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
    if (&overlay[index] != &file && overlay_has_storage(overlay[index]) &&
        overlay_offset(overlay[index]) > old_offset) {
      set_overlay_offset(
          overlay[index],
          overlay_offset(overlay[index]) -
              static_cast<uint32_t>(old_record_size));
    }
  }

  const size_t new_offset = overlay_bytes_used - old_record_size;
  std::memcpy(overlay_bytes + new_offset, encoded_key, key_length + 1);
  overlay_bytes_used = new_offset + key_length + 1;
  set_overlay_offset(file, static_cast<uint32_t>(new_offset));
  file.size = 0;
  file.flags |= kOverlayHasRecord | kOverlayDeleted | kOverlaySerialized;
  overlay_dirty = true;
  return 0;
}
} // namespace

extern "C" uint8_t fs_erase(const uint8_t *tag, uint8_t tag_length) {
  auto *file = find_overlay(tag, tag_length);
  if (file == nullptr) {
    const uint8_t *ignored_data = nullptr;
    uint32_t ignored_length = 0;
    if (!find_file(tag, tag_length, &ignored_data, &ignored_length)) {
      return 1;
    }
    file = allocate_overlay(tag, tag_length);
    if (file == nullptr) {
      return 1;
    }
    if (!append_overlay(*file, tag, nullptr, 0)) return 1;
    file->flags |= kOverlayHasRecord | kOverlayDeleted;
    overlay_dirty = true;
    return 0;
  } else if (!overlay_has_record(*file)) {
    return 1;
  }

  return erase_overlay_slot(static_cast<uint16_t>(file - overlay));
}

namespace {
template <typename Match>
uint8_t erase_overlay_records(Match matches) {
  std::fill(&erase_bucket_head[0][0],
            &erase_bucket_head[0][0] +
                (kMaxOverlayTagLength + 1) * 17,
            UINT16_MAX);
  for (uint16_t slot = 0; slot < kMaxOverlayFiles; ++slot) {
    const OverlayFile &file = overlay[slot];
    if (!overlay_is_used(file) || !overlay_has_record(file)) continue;
    uint8_t tag[kMaxOverlayTagLength]{};
    uint8_t tag_length = 0;
    if (!reconstruct_overlay_tag(slot, tag, &tag_length) ||
        !matches(tag, tag_length)) {
      continue;
    }
    const uint8_t payload_bucket = static_cast<uint8_t>(
        std::min<size_t>(file.size / 256, 16));
    erase_bucket_next[slot] = erase_bucket_head[tag_length][payload_bucket];
    erase_bucket_head[tag_length][payload_bucket] = slot;
  }

  // A child tag is encoded relative to its parent slot. Erase longer tags
  // first so fs_erase can still resolve each parent while its descendants are
  // removed. Within each depth, reclaim payload bytes first so a full overlay
  // can make room for tombstones of smaller records. The fixed buckets avoid
  // heap allocation in the card manager.
  for (int length = kMaxOverlayTagLength; length > 0; --length) {
    for (int payload_bucket = 16; payload_bucket >= 0; --payload_bucket) {
      for (uint16_t slot = erase_bucket_head[length][payload_bucket];
           slot != UINT16_MAX; slot = erase_bucket_next[slot]) {
        uint8_t tag[kMaxOverlayTagLength]{};
        uint8_t tag_length = 0;
        if (!reconstruct_overlay_tag(slot, tag, &tag_length) ||
            tag_length != length) {
#ifndef RP2350_FS_OVERLAY_TEST
          std::printf("CHOUPI:FS_ERASE_FAIL reason=tag-reconstruct slot=%u depth=%u\r\n",
                      static_cast<unsigned>(slot),
                      static_cast<unsigned>(length));
          std::fflush(stdout);
#endif
          return 1;
        }
        if (erase_overlay_slot(slot) != 0) {
#ifndef RP2350_FS_OVERLAY_TEST
          std::printf("CHOUPI:FS_ERASE_FAIL reason=slot-delete slot=%u tag_len=%u size=%lu bytes=%lu/%lu tag=",
                      static_cast<unsigned>(slot),
                      static_cast<unsigned>(tag_length),
                      static_cast<unsigned long>(overlay[slot].size),
                      static_cast<unsigned long>(overlay_bytes_used),
                      static_cast<unsigned long>(kMaxOverlayBytes));
          for (uint8_t index = 0; index < tag_length && index < 8; ++index) {
            std::printf("%02X", tag[index]);
          }
          std::printf("\r\n");
          std::fflush(stdout);
#endif
          return 1;
        }
      }
    }
  }
  return 0;
}
} // namespace

extern "C" uint8_t fs_erase_prefix(const uint8_t *prefix,
                                    uint8_t prefix_length) {
  if (prefix == nullptr || prefix_length == 0 ||
      prefix_length > kMaxOverlayTagLength) {
    return 1;
  }
  return erase_overlay_records([prefix, prefix_length](const uint8_t *tag,
                                                        uint8_t length) {
    return length >= prefix_length &&
           std::memcmp(tag, prefix, prefix_length) == 0;
  });
}

extern "C" uint8_t fs_erase_package_state(uint8_t package_id) {
  return erase_overlay_records([package_id](const uint8_t *tag,
                                              uint8_t length) {
    const bool static_field = length >= 2 && tag[0] == 0x02 &&
                              tag[1] == package_id;
    const bool applet_field = length >= 3 && tag[0] == 0x03 &&
                              tag[2] == package_id;
    const bool lifecycle = length >= 3 && tag[0] == 0x02 && tag[1] == 0xff &&
                           tag[2] == package_id;
    return static_field || applet_field || lifecycle;
  });
}

extern "C" uint8_t choupi_store_cap_package(uint8_t package_id,
                                            const uint8_t *package_aid,
                                            uint8_t package_aid_length,
                                            const uint8_t *data,
                                            uint32_t length) {
  if (!jcvm::FlashMemory_Handler::isDynamicPackageID(package_id) ||
      package_aid == nullptr || package_aid_length < 5 ||
      package_aid_length > 16 || data == nullptr || length == 0 ||
      length > 64 * 1024) {
    return 1;
  }
  return cap_store_write_package(package_id, package_aid, package_aid_length,
                                 data, length) ? 0 : 1;
}

extern "C" uint8_t choupi_store_cap_instance(uint8_t package_id,
                                              const uint8_t *applet_aid,
                                              uint8_t applet_aid_length,
                                              const uint8_t *aid,
                                              uint8_t aid_length,
                                              uint16_t applet_class_index,
                                              uint8_t owner_id) {
  return cap_store_mark_package_installed(package_id, applet_aid,
                                          applet_aid_length, aid, aid_length,
                                          applet_class_index, owner_id) ? 0 : 1;
}

extern "C" const uint8_t *choupi_cap_store_data(uint32_t *length) {
  return cap_store_data(length);
}

extern "C" const uint8_t *choupi_cap_store_data_for_package(
    uint8_t package_id, uint32_t *length) {
  return cap_store_data_for_package(package_id, length);
}

extern "C" uint8_t fs_read_1b_at(const uint8_t *tag, uint8_t tag_length,
                                 uint32_t offset, uint8_t *value) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  if (!find_file(tag, tag_length, &data, &length) || offset >= length) {
    return 1;
  }
  *value = data[offset];
  return 0;
}

extern "C" uint8_t fs_read_2b_at(const uint8_t *tag, uint8_t tag_length,
                                 uint32_t offset, uint16_t *value) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  offset *= 2;
  if (!find_file(tag, tag_length, &data, &length) || offset > length ||
      length - offset < 2) {
    return 1;
  }
  *value = static_cast<uint16_t>(data[offset]) |
           static_cast<uint16_t>(data[offset + 1] << 8);
  return 0;
}

extern "C" uint8_t fs_read_4b_at(const uint8_t *tag, uint8_t tag_length,
                                 uint32_t offset, uint32_t *value) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  offset *= 4;
  if (!find_file(tag, tag_length, &data, &length) || offset > length ||
      length - offset < 4) {
    return 1;
  }
  *value = static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
  return 0;
}

extern "C" uint8_t fs_write_1b_at(const uint8_t *tag, uint8_t tag_length,
                                  uint32_t offset, uint8_t value) {
  return write_at(tag, tag_length, offset, &value, sizeof(value)) ? 0 : 1;
}

extern "C" uint8_t fs_write_2b_at(const uint8_t *tag, uint8_t tag_length,
                                  uint32_t offset, uint16_t value) {
  offset *= 2;
  const uint8_t bytes[] = {static_cast<uint8_t>(value),
                           static_cast<uint8_t>(value >> 8)};
  return write_at(tag, tag_length, offset, bytes, sizeof(bytes)) ? 0 : 1;
}

extern "C" uint8_t fs_write_4b_at(const uint8_t *tag, uint8_t tag_length,
                                  uint32_t offset, uint32_t value) {
  offset *= 4;
  const uint8_t bytes[] = {static_cast<uint8_t>(value),
                           static_cast<uint8_t>(value >> 8),
                           static_cast<uint8_t>(value >> 16),
                           static_cast<uint8_t>(value >> 24)};
  return write_at(tag, tag_length, offset, bytes, sizeof(bytes)) ? 0 : 1;
}

extern "C" void path_package_list(uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length, {0x00});
}

extern "C" void path_cap(uint8_t package, uint8_t (*tag)[128],
                         uint8_t *length) {
  set_path(tag, length, {0x01, package});
}

extern "C" void path_static(uint8_t package, uint8_t field,
                            uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length, {0x02, package, field});
}

extern "C" void path_static16(uint8_t package, uint16_t field,
                               uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length,
           {0x02, package, static_cast<uint8_t>(field >> 8),
            static_cast<uint8_t>(field)});
}

extern "C" void path_applet_field(uint8_t applet, uint8_t package,
                                  uint8_t class_id, uint8_t field,
                                  uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length, {0x03, applet, package, class_id, field});
}

extern "C" void path_applet_field16(uint8_t applet, uint8_t package,
                                    uint16_t class_id, uint8_t field,
                                    uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length,
           {0x03, applet, package,
            static_cast<uint8_t>(class_id >> 8),
            static_cast<uint8_t>(class_id), field});
}
