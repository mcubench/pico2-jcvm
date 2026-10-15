#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ffi.h"
#include "jni.hpp"
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"
#include "hardware/flash.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "pico/platform.h"
#include "tusb.h"
#include "cap_store.hpp"

extern "C" const uint8_t choupi_rommask_start[];
extern "C" const uint8_t choupi_rommask_end[];
extern "C" const uint8_t __flash_binary_end[];
#ifdef RP2350_FS_OVERLAY_TEST
extern "C" const uint8_t *rp2350_fs_test_flash_base(void);
#endif
extern "C" uint16_t runtime(uint8_t package_id, uint8_t class_id,
                             uint8_t method_id);
extern "C" void choupi_ccid_task(void);
extern "C" void choupi_ccid_debug_state(uint8_t *stage, uint8_t *endpoint,
                                         uint16_t *rx_events,
                                         uint16_t *task_count);

extern "C" void choupi_watchdog_update(void) { watchdog_update(); }
extern "C" uint32_t choupi_time_us_32(void) { return time_us_32(); }
extern "C" void choupi_install_trace(uint32_t pc, uint8_t opcode,
                                      uint32_t instruction_count) {
  watchdog_hw->scratch[0] = 0x494e5354; // "INST"
  watchdog_hw->scratch[1] = pc;
  watchdog_hw->scratch[2] = (instruction_count & 0x00ffffffu) |
                            (static_cast<uint32_t>(opcode) << 24);
  watchdog_hw->scratch[3] = 0;
  watchdog_hw->scratch[5] = 0;
}
extern "C" void choupi_install_stage(uint8_t stage) {
  watchdog_hw->scratch[3] = stage;
}
extern "C" void choupi_install_context(uint32_t context) {
  watchdog_hw->scratch[4] = context;
}
extern "C" uint8_t choupi_install_debug_stage(void) {
  return static_cast<uint8_t>(watchdog_hw->scratch[3]);
}
extern "C" uint32_t choupi_install_debug_context(void) {
  return watchdog_hw->scratch[4];
}
extern "C" void choupi_install_origin(uint32_t origin) {
  watchdog_hw->scratch[5] = origin;
}
namespace {

bool watchdog_recovery_pending = false;
bool watchdog_recovery_by_watchdog = false;
uint32_t watchdog_recovery_pc = 0;
uint32_t watchdog_recovery_info = 0;
uint32_t watchdog_recovery_stage = 0;
uint32_t watchdog_recovery_context = 0;
uint32_t watchdog_recovery_origin = 0;

constexpr size_t kRomFsOffset = 0x20000;
constexpr size_t kMaxOverlayFiles = 8192;
constexpr size_t kMaxOverlayBytes = 64 * 1024;
constexpr uint32_t kFilesystemSlot0 = 0x100000;
constexpr uint32_t kFilesystemSlotSize = 0x40000;
constexpr uint32_t kFilesystemSlot1 = kFilesystemSlot0 + kFilesystemSlotSize;
constexpr uint32_t kFilesystemHeaderSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kFilesystemPayloadOffset = kFilesystemHeaderSize;
constexpr uint32_t kFilesystemXipBase = 0x10000000;
constexpr uint32_t kFilesystemMagic = 0x3253464a; // "JFS2"
constexpr uint16_t kFilesystemVersion = 2;

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
              "filesystem snapshot index record must be 10 bytes");
constexpr size_t kFilesystemMaxPayload =
    kMaxOverlayFiles * sizeof(FilesystemRecord) + kMaxOverlayBytes;
static_assert(kFilesystemPayloadOffset + kFilesystemMaxPayload <=
                  kFilesystemSlotSize,
              "filesystem snapshot maximum exceeds its slot");
static_assert(kFilesystemSlot1 + kFilesystemSlotSize <=
                  PICO_FLASH_SIZE_BYTES,
              "filesystem slots exceed RP2350 flash");
static_assert(kFilesystemSlot1 + kFilesystemSlotSize <= 0x1c0000,
              "filesystem slots overlap the CAP store");

struct __attribute__((packed)) FilesystemHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t generation;
  uint32_t file_count;
  uint32_t bytes_used;
  uint32_t payload_crc32;
  uint32_t header_crc32;
};

static_assert(sizeof(FilesystemHeader) <= FLASH_PAGE_SIZE,
  uint32_t payload_length;
              "filesystem header must fit in one flash page");
constexpr uint8_t kOverlaySerialized = 1u << 4;
constexpr uint8_t kOverlayDeleted = 1u << 5;

alignas(FLASH_PAGE_SIZE) OverlayFile overlay[kMaxOverlayFiles];
size_t overlay_file_count = 0;

constexpr uint8_t kOverlayUsed = 1u << 0;
constexpr uint8_t kOverlayHasRecord = 1u << 1;
constexpr uint8_t kOverlayCompressedTag = 1u << 2;
constexpr uint8_t kOverlayParentTag = 1u << 3;
alignas(FLASH_PAGE_SIZE) uint8_t overlay_bytes[kMaxOverlayBytes];
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
  return ~crc;
}

const uint8_t *filesystem_slot_data(uint8_t slot) {
  const uint32_t offset = slot == 0 ? kFilesystemSlot0 : kFilesystemSlot1;
}

uint32_t filesystem_payload_crc32(uint32_t bytes_used) {
  uint32_t crc = 0xffffffffu;
  for (uint16_t slot = 0; slot < kMaxOverlayFiles; ++slot) {
    if ((overlay[slot].flags & kOverlayUsed) == 0) continue;
    const uint8_t slot_bytes[] = {static_cast<uint8_t>(slot),
                                  static_cast<uint8_t>(slot >> 8)};
    filesystem_crc32_append(crc, slot_bytes, sizeof(slot_bytes));
    filesystem_crc32_append(
        crc, reinterpret_cast<const uint8_t *>(&overlay[slot]),
        sizeof(OverlayFile));
  }
  filesystem_crc32_append(crc, overlay_bytes, bytes_used);
#ifdef RP2350_FS_OVERLAY_TEST
  return rp2350_fs_test_flash_base() + offset;
#else
  return reinterpret_cast<const uint8_t *>(kFilesystemXipBase + offset);
#endif
}

const FilesystemHeader *filesystem_slot_header(uint8_t slot) {
  return reinterpret_cast<const FilesystemHeader *>(filesystem_slot_data(slot));
}

bool filesystem_header_valid(const FilesystemHeader &header) {
  return header.magic == kFilesystemMagic &&
         header.version == kFilesystemVersion &&
         header.file_count <= kMaxOverlayFiles &&
         header.bytes_used <= kMaxOverlayBytes &&
         header.header_crc32 == filesystem_crc32(
             reinterpret_cast<const uint8_t *>(&header),
             offsetof(FilesystemHeader, header_crc32));
}

         header.payload_length ==
             header.file_count * sizeof(FilesystemRecord) + header.bytes_used &&
         header.payload_length <= kFilesystemMaxPayload &&
bool filesystem_slot_valid(uint8_t slot) {
  const auto *header = filesystem_slot_header(slot);
  if (!filesystem_header_valid(*header)) return false;
  const uint8_t *payload = filesystem_slot_data(slot) + kFilesystemPayloadOffset;
  return filesystem_crc32(payload, header->payload_length) ==
         header->payload_crc32;
}

struct FilesystemWriteContext {
  uint32_t generation;
  uint32_t slot_offset;
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

  uint32_t file_count;
  uint32_t bytes_used;
  uint32_t payload_crc32;
  bool success = false;
};

void __not_in_flash_func(write_filesystem_snapshot)(void *opaque) {
  uint32_t payload_length;
  auto *context = static_cast<FilesystemWriteContext *>(opaque);
  uint32_t erase_size;
  flash_range_erase(context->slot_offset, context->erase_size);
  FilesystemStream stream{context->slot_offset + kFilesystemPayloadOffset, 0,
                          {}};
  for (size_t index = 0; index < FLASH_PAGE_SIZE; ++index) {
    stream.page[index] = 0xff;
  }
  for (uint16_t slot = 0; slot < kMaxOverlayFiles; ++slot) {
    if ((overlay[slot].flags & kOverlayUsed) == 0) continue;
    emit_filesystem_byte(&stream, static_cast<uint8_t>(slot));
    emit_filesystem_byte(&stream, static_cast<uint8_t>(slot >> 8));
    const auto *record = reinterpret_cast<const uint8_t *>(&overlay[slot]);
    for (size_t index = 0; index < sizeof(OverlayFile); ++index) {
      emit_filesystem_byte(&stream, record[index]);
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
  header.payload_crc32 = context->payload_crc32;
  header.header_crc32 = filesystem_crc32(
      reinterpret_cast<const uint8_t *>(&header),
      offsetof(FilesystemHeader, header_crc32));
  alignas(FLASH_PAGE_SIZE) uint8_t page[FLASH_PAGE_SIZE];
  header.payload_length = context->payload_length;
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
  const uint8_t next_slot = filesystem_active_slot == 0 ? 1 : 0;
  const uint32_t slot_offset =
      next_slot == 0 ? kFilesystemSlot0 : kFilesystemSlot1;
  const uint32_t next_generation = filesystem_generation + 1;
  const uint32_t bytes_used = static_cast<uint32_t>(overlay_bytes_used);
  const uint32_t file_count = static_cast<uint32_t>(overlay_file_count);
  const uint32_t payload_length =
      file_count * sizeof(FilesystemRecord) + bytes_used;
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
      !filesystem_slot_valid(next_slot)) {
    std::printf("CHOUPI:FS_SYNC_FAIL status=%d slot=%u\r\n", status,
                static_cast<unsigned>(next_slot));
    std::fflush(stdout);
    return false;
  }
  filesystem_active_slot = next_slot;
  filesystem_generation = next_generation;
  overlay_dirty = false;
  return true;
}

void restore_overlay_snapshot() {
  const bool valid0 = filesystem_slot_valid(0);
  const bool valid1 = filesystem_slot_valid(1);
  if (!valid0 && !valid1) return;
  uint8_t slot = valid0 ? 0 : 1;
  if (valid0 && valid1) {
    const uint32_t generation0 = filesystem_slot_header(0)->generation;
    const uint32_t generation1 = filesystem_slot_header(1)->generation;
    if (static_cast<int32_t>(generation1 - generation0) > 0) slot = 1;
  }
  const auto *header = filesystem_slot_header(slot);
  const uint8_t *payload = filesystem_slot_data(slot) + kFilesystemPayloadOffset;
  bool valid = true;
  for (uint32_t index = 0; index < header->file_count; ++index) {
    const uint16_t slot_index = static_cast<uint16_t>(payload[0]) |
                                static_cast<uint16_t>(payload[1] << 8);
    if (slot_index >= kMaxOverlayFiles ||
        (overlay[slot_index].flags & kOverlayUsed) != 0) {
      valid = false;
      break;
    }
    std::memcpy(&overlay[slot_index], payload + 2, sizeof(OverlayFile));
    if ((overlay[slot_index].flags & kOverlayUsed) == 0) {
      valid = false;
      break;
    }
    payload += sizeof(FilesystemRecord);
  }
  if (valid) {
    std::memcpy(overlay_bytes, payload, header->bytes_used);
    for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
      const auto &file = overlay[index];
      if ((file.flags & kOverlayUsed) != 0 &&
          (file.offset >= header->bytes_used ||
           (overlay_bytes[file.offset] & 0x7f) >
               header->bytes_used - file.offset ||
           file.size > header->bytes_used - file.offset - 1 -
                           (overlay_bytes[file.offset] & 0x7f))) {
        valid = false;
        break;
      }
    }
  }
  if (!valid) {
    for (auto &entry : overlay) entry = OverlayFile{};
    overlay_file_count = 0;
    overlay_bytes_used = 0;
    return;
  }
  overlay_file_count = header->file_count;
  overlay_bytes_used = header->bytes_used;
  overlay_dirty = false;
  filesystem_active_slot = slot;
  filesystem_generation = header->generation;
}
              static_cast<unsigned long>(length),
              static_cast<unsigned>(overlay_file_count),
              static_cast<unsigned long>(overlay_bytes_used),
              static_cast<unsigned long>(kMaxOverlayBytes));
  for (uint8_t index = 0; index < tag_length && index < 8; ++index) {

void report_fs_write_failure(const char *operation, const uint8_t *tag,
                             uint8_t tag_length, uint32_t length) {
  std::printf("CHOUPI:FS_WRITE_FAIL op=%s tag_len=%u size=%lu files=%u bytes=%lu/%lu tag=",
              operation, static_cast<unsigned>(tag_length),
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
  return overlay_bytes[file.offset] & 0x7F;
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
  *parent_slot = static_cast<uint16_t>(slot);
  return true;
}

uint8_t encoded_tag_length(const uint8_t *tag, uint8_t tag_length) {
  uint16_t parent_slot = 0;
  if (find_parent_slot(tag, tag_length, &parent_slot)) {
    return 3;
  }
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
        overlay_bytes + file.offset + 1, file.tag_length);
    return static_cast<uint8_t>(root_length + (file.tag_length - root_length) / 2);
  }
  return file.tag_length;
}

bool stored_tag_matches(const OverlayFile &file, const uint8_t *tag,
                        uint8_t tag_length) {
  const uint8_t header = overlay_bytes[file.offset];
  const uint8_t *stored = overlay_bytes + file.offset + 1;
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
  return overlay_bytes + file.offset + 1 + stored_tag_length(file);
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
  for (size_t probes = 0; probes < kMaxOverlayFiles; ++probes) {
    auto &file = overlay[slot];
    if (!overlay_is_used(file)) {
      return nullptr;
    }
    if (file.hash == hash && file.tag_length == tag_length &&
        overlay_has_storage(file) &&
        stored_tag_matches(file, tag, tag_length)) {
      if (found_slot != nullptr) {
        *found_slot = slot;
      }
      return &file;
    }
    slot = (slot + 1) % kMaxOverlayFiles;
  }
  return nullptr;
}

OverlayFile *allocate_overlay(const uint8_t *tag, uint8_t tag_length) {
  if (tag_length > 128 ||
      (tag_length > 127 && !is_compressible_tag(tag, tag_length))) {
    return nullptr;
  }
  if (auto *file = find_overlay(tag, tag_length)) {
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
      uint16_t parent_slot = 0;
      if (find_parent_slot(tag, tag_length, &parent_slot)) {
        file.flags |= kOverlayParentTag;
      } else if (is_compressible_tag(tag, tag_length)) {
        file.flags |= kOverlayCompressedTag;
      }
      ++overlay_file_count;
      return &file;
    }
    slot = (slot + 1) % kMaxOverlayFiles;
  }
  return nullptr;
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
  if (new_used > kMaxOverlayBytes) {
    return false;
  }
  const size_t old_offset = file.offset;
  if (old_record_size != 0) {
    std::memmove(overlay_bytes + old_offset,
                 overlay_bytes + old_offset + old_record_size,
                 overlay_bytes_used - old_offset - old_record_size);
    for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
      if (&overlay[index] != &file && overlay_has_storage(overlay[index]) &&
          overlay[index].offset > old_offset) {
        overlay[index].offset -= static_cast<uint32_t>(old_record_size);
      }
    }
  }
  file.offset = static_cast<uint32_t>(overlay_bytes_used - old_record_size);
  overlay_bytes[file.offset] = static_cast<uint8_t>(
      key_length | (parent_tag ? 0x80 : 0));
  if (parent_tag) {
    uint16_t parent_slot = 0;
    if (!find_parent_slot(tag, file.tag_length, &parent_slot)) {
      return false;
    }
    overlay_bytes[file.offset + 1] = static_cast<uint8_t>(parent_slot);
    overlay_bytes[file.offset + 2] = static_cast<uint8_t>(parent_slot >> 8);
    overlay_bytes[file.offset + 3] = tag[file.tag_length - 1];
  } else if (overlay_compresses_tag(file)) {
    const uint8_t root_length = tag_root_length(tag, file.tag_length);
    std::memcpy(overlay_bytes + file.offset + 1, tag, root_length);
    uint8_t stored_index = root_length;
    for (uint8_t index = root_length; index < file.tag_length; index += 2) {
      overlay_bytes[file.offset + 1 + stored_index++] = tag[index];
    }
  } else if (file.tag_length != 0) {
    std::memcpy(overlay_bytes + file.offset + 1, tag, file.tag_length);
  }
  if (length != 0) {
    std::memcpy(overlay_bytes + file.offset + 1 + key_length, data,
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
    return true;
  }

  const size_t record_key_length = encoded_tag_length(tag, tag_length);
  if (overlay_bytes_used + 1 + record_key_length + existing_length >
    overlay_dirty = true;
      kMaxOverlayBytes) {
    report_fs_write_failure("update", tag, tag_length, existing_length);
    return false;
  }
  file = allocate_overlay(tag, tag_length);
  if (file == nullptr) {
    report_fs_write_rejection("index-full", tag, tag_length, offset, length,
                              existing_length);
    return false;
  }
  file->offset = static_cast<uint16_t>(overlay_bytes_used);
  if (!append_overlay(*file, tag, existing, existing_length)) {
    report_fs_write_rejection("overlay-capacity", tag, tag_length, offset,
                              length, existing_length);
    return false;
  }
  std::memcpy(overlay_data(*file) + offset, data, length);
  return true;
}

void set_path(uint8_t (*tag)[128], uint8_t *length,
              std::initializer_list<uint8_t> bytes) {
  overlay_dirty = true;
  *length = static_cast<uint8_t>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), (*tag));
}

} // namespace

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
  restore_overlay_snapshot();
#else
  const uintptr_t image_end = reinterpret_cast<uintptr_t>(__flash_binary_end);
  filesystem_layout_available =
      image_end < kFilesystemXipBase + kFilesystemSlot0;
  if (filesystem_layout_available) {
    restore_overlay_snapshot();
  }
#endif
  (void)cap_store_initialize();
  return 0;
}
extern "C" uint8_t fs_sync(void) {
  return persist_overlay() ? 0 : 1;
}

extern "C" void fs_drop(void) { (void)fs_sync(); }



extern "C" uint8_t fs_length(const uint8_t *tag, uint8_t tag_length,
                              uint32_t *length) {
  const uint8_t *data = nullptr;
  return find_file(tag, tag_length, &data, length) ? 0 : 1;
extern "C" uint8_t fs_exists(const uint8_t *tag, uint8_t tag_length) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  return find_file(tag, tag_length, &data, &length) ? 1 : 0;
}
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
  if (overlay_bytes_used - old_record_size + 1 + record_key_length + length >
      kMaxOverlayBytes || (file == nullptr &&
                           overlay_file_count == kMaxOverlayFiles)) {
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
  return 0;
}

extern "C" uint8_t fs_erase(const uint8_t *tag, uint8_t tag_length) {
  auto *file = find_overlay(tag, tag_length);
  overlay_dirty = true;
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
  } else if (!overlay_has_record(*file)) {
    return 1;
  }

  if (!append_overlay(*file, tag, nullptr, 0)) {
    return 1;
  }
  file->flags &= static_cast<uint8_t>(~kOverlayHasRecord);
  file->flags |= kOverlayDeleted;
  return 0;
}

extern "C" uint8_t choupi_store_cap_package(uint8_t package_id,
                                            const uint8_t *data,
  overlay_dirty = true;
                                            uint32_t length) {
  if (package_id != 20 || data == nullptr || length == 0 ||
      length > 28 * 1024) {
    return 1;
  }
  return cap_store_write(data, length) ? 0 : 1;
}

extern "C" uint8_t choupi_store_cap_instance(const uint8_t *aid,
                                              uint8_t aid_length,
                                              uint16_t applet_class_index) {
  return cap_store_mark_installed(aid, aid_length, applet_class_index) ? 0 : 1;
}

extern "C" const uint8_t *choupi_cap_store_data(uint32_t *length) {
  return cap_store_data(length);
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

extern "C" void path_applet_field(uint8_t applet, uint8_t package,
                                  uint8_t class_id, uint8_t field,
                                  uint8_t (*tag)[128], uint8_t *length) {
  set_path(tag, length, {0x03, applet, package, class_id, field});
}

int main_arm(void) {
  // A stalled VM can stop servicing USB requests. Preserve and report its last
  // INSTALL opcode after a reset, even if BOOTROM/USB recovery replaced the
  // original watchdog reset-cause flag before this image started.
  watchdog_recovery_by_watchdog = watchdog_enable_caused_reboot();
  if (watchdog_hw->scratch[0] == 0x494e5354) {
    watchdog_recovery_pending = true;
    watchdog_recovery_pc = watchdog_hw->scratch[1];
    watchdog_recovery_info = watchdog_hw->scratch[2];
    watchdog_recovery_stage = watchdog_hw->scratch[3];
    watchdog_recovery_context = watchdog_hw->scratch[4];
    watchdog_recovery_origin = watchdog_hw->scratch[5];
  }

  stdio_init_all();
  for (unsigned int attempt = 0; attempt < 120; ++attempt) {
    // IRQ background servicing is disabled so TinyUSB and CCID stay on one
    // context. Drive enumeration here too; stdio output alone does not poll
    // TinyUSB before the CDC host has connected.
    tud_task();
    if (stdio_usb_connected()) break;
    printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
    fflush(stdout);
    sleep_ms(250);
  }
  sleep_ms(500);
  printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
  printf("CHOUPI:STAGE fs_init:start\r\n");
  fflush(stdout);
  if (fs_init() != 0) {
    printf("TEST:FAIL choupi_fs_init\r\n");
    for (;;) {
      tight_loop_contents();
    }
  }
  printf("CHOUPI:STAGE fs_init:done\r\n");
  fflush(stdout);

  // Refresh once per VM instruction. If a native operation or handler wedges,
  // the next boot enters BOOTSEL instead of returning to an unresponsive app.
  watchdog_enable(15000, true);
  if (!watchdog_recovery_pending) {
    watchdog_hw->scratch[0] = 0;
    watchdog_hw->scratch[1] = 0;
    watchdog_hw->scratch[2] = 0;
    watchdog_hw->scratch[3] = 0;
    watchdog_hw->scratch[4] = 0;
    watchdog_hw->scratch[5] = 0;
  }

  uint16_t status = 0x6F00;
  try {
    printf("CHOUPI:STAGE runtime:start\r\n");
    fflush(stdout);
    status = runtime(STARTING_JAVACARD_PACKAGE, STARTING_JAVACARD_CLASS,
                     STARTING_JAVACARD_METHOD);
    printf("CHOUPI:STAGE runtime:done\r\n");
    fflush(stdout);
  } catch (...) {
    printf("TEST:FAIL choupi_uncaught_exception\r\n");
  }
  printf("CHOUPI:STATUS sw=%04X\r\n", status);
  const bool passed = status == 0x9000;
  absolute_time_t next_report = make_timeout_time_ms(1000);
  for (;;) {
    // Keep TinyUSB and the custom CCID callbacks on one thread. In particular,
    // the main-loop CCID handler must queue responses from the same context
    // that owns usbd endpoint state.
    tud_task();
    choupi_ccid_task();
    watchdog_update();
    if (time_reached(next_report)) {
      uint8_t ccid_stage = 0;
      uint8_t ccid_endpoint = 0;
      uint16_t ccid_rx_events = 0;
      uint16_t ccid_task_count = 0;
      choupi_ccid_debug_state(&ccid_stage, &ccid_endpoint, &ccid_rx_events,
                              &ccid_task_count);
      printf("CHOUPI:CCID stage=%u endpoint=%02X rx_events=%u tasks=%u\r\n",
             static_cast<unsigned>(ccid_stage),
             static_cast<unsigned>(ccid_endpoint),
             static_cast<unsigned>(ccid_rx_events),
             static_cast<unsigned>(ccid_task_count));
      printf("CHOUPI:STATUS sw=%04X\r\n", status);
      printf("TEST:%s choupi_runtime\r\n", passed ? "PASS" : "FAIL");
      printf("TEST:SUMMARY pass=%u fail=%u\r\n", passed ? 1u : 0u,
             passed ? 0u : 1u);
      if (watchdog_recovery_pending) {
        printf("CHOUPI:WATCHDOG_RECOVERY marker=494E5354 cause=%s pc=%08lX opcode=%02lX count=%lu stage=%lu context=%08lX origin=%08lX\r\n",
               watchdog_recovery_by_watchdog ? "watchdog" : "other",
               static_cast<unsigned long>(watchdog_recovery_pc),
               static_cast<unsigned long>(watchdog_recovery_info >> 24),
               static_cast<unsigned long>(watchdog_recovery_info & 0x00ffffffu),
               static_cast<unsigned long>(watchdog_recovery_stage),
               static_cast<unsigned long>(watchdog_recovery_context),
               static_cast<unsigned long>(watchdog_recovery_origin));
      }
      fflush(stdout);
      next_report = delayed_by_ms(next_report, 1000);
    }
    tight_loop_contents();
  }
}
