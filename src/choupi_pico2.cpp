#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <malloc.h>
#include <unistd.h>

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
#include "flash_layout.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "reader_led.hpp"

extern "C" char __StackLimit;

extern "C" const uint8_t choupi_rommask_start[];
extern "C" const uint8_t choupi_rommask_end[];
extern "C" const uint8_t __flash_binary_end[];
#ifdef RP2350_FS_OVERLAY_TEST
extern "C" const uint8_t *rp2350_fs_test_flash_base(void);
#endif
extern "C" uint16_t runtime(uint8_t package_id, uint8_t class_id,
                             uint8_t method_id);
extern "C" void choupi_vm_runtime_debug_state(
    uint16_t *status, uint16_t *exception_id, uint8_t *opcode, uint32_t *pc,
    uint16_t *stack_depth, uint32_t *opcode_history,
    uint32_t *depth_history, uint8_t *field_bounds_failure,
    uint16_t *field_index, uint16_t *field_count, uint16_t *field_package,
    uint16_t *field_class, uint16_t *field_value_ref);
extern "C" void choupi_vm_field_cp_state(
    uint8_t *recorded, uint8_t *succeeded, uint8_t *package_id,
    uint16_t *cp_index, uint8_t *token, uint8_t *exception_id);
extern "C" void choupi_vm_field_heap_ref_state(
    uint8_t *recorded, uint16_t *objectref, uint16_t *offset,
    uint8_t *is_array);
extern "C" void choupi_vm_persistent_restore_failure_state(
    uint8_t *recorded, uint16_t *package_id, uint16_t *class_index,
    uint16_t *field_index, uint8_t *field_type, uint8_t *stage,
    uint8_t *exception_id);
extern "C" void choupi_vm_putfield_stage_state(
    uint8_t *stage, uint16_t *objectref, uint16_t *value,
    uint8_t *exception_id);
extern "C" void choupi_vm_field_write_state(
    uint8_t *stage, uint16_t *package_id, uint16_t *class_index,
    uint16_t *field_index, uint16_t *value, uint16_t *field_count,
    uint8_t *flags, uint8_t *exception_id);
extern "C" void choupi_vm_array_write_failure_state(
    uint8_t *recorded, uint8_t *stage, uint16_t *index, uint16_t *size,
    uint16_t *ref, uint8_t *type, uint8_t *exception_id);
extern "C" uint8_t choupi_persistent_write_transaction_active(void);
extern "C" void choupi_ccid_task(void);
extern "C" void choupi_ccid_native_crypto_progress(void);
extern "C" void choupi_vm_native_progress(void);
extern "C" void choupi_ccid_debug_state(uint8_t *stage, uint8_t *endpoint,
                                         uint16_t *rx_events,
                                         uint16_t *task_count,
                                         uint8_t *last_apdu_length,
                                         uint8_t *last_apdu_header,
                                         uint8_t *last_message_type,
                                         uint8_t *last_payload_length,
                                         uint16_t *xfr_block_count,
                                         uint16_t *out_xfer_errors,
                                         uint16_t *in_xfer_errors,
                                         uint16_t *out_rearm_errors,
                                         uint16_t *in_submit_errors,
                                         uint16_t *invalid_messages,
                                         uint8_t *last_xfer_result);
extern "C" void choupi_ccid_debug_response(uint8_t *apdu_header,
                                             uint16_t *response_length,
                                             uint16_t *status_word);
extern "C" bool choupi_ccid_debug_take_response_data(uint8_t *data,
                                                        uint8_t capacity,
                                                        uint8_t *length);
extern "C" void choupi_ccid_debug_unknown(uint8_t *last_message,
                                            uint16_t *message_count);
extern "C" void choupi_ccid_debug_recent(uint8_t *commands,
                                           uint8_t command_capacity,
                                           uint16_t *command_count);

extern "C" void choupi_watchdog_update(void) { watchdog_update(); }
extern "C" void choupi_native_crypto_progress(void) {
  choupi_vm_native_progress();
  watchdog_update();
  choupi_ccid_native_crypto_progress();
}
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
constexpr uint32_t kFilesystemSlot0 = flash_layout::filesystem_slot(0);
constexpr uint32_t kFilesystemSlotSize = flash_layout::filesystem_slot_size;
constexpr uint32_t kFilesystemSlot1 = flash_layout::filesystem_slot(1);
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
// OverlayFile and FilesystemRecord store byte offsets in uint16_t fields.
// Keep the arena within that representation until the on-flash format is
// deliberately upgraded.
constexpr size_t kMaxOverlayBytes = UINT16_MAX;
static_assert(kMaxOverlayBytes <= UINT16_MAX,
              "overlay offsets must fit their 16-bit representation");
constexpr size_t kFilesystemMaxPayload =
    kMaxOverlayFiles * sizeof(FilesystemRecord) + kMaxOverlayBytes;
static_assert(kFilesystemPayloadOffset + kFilesystemMaxPayload <=
                  kFilesystemSlotSize,
              "filesystem snapshot maximum exceeds its slot");
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
constexpr uint8_t kMaxOverlayTagLength = 128;

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
  return header.magic == kFilesystemMagic &&
         header.version == kFilesystemVersion &&
         header.file_count <= kMaxOverlayFiles &&
         header.bytes_used <= kMaxOverlayBytes &&
         header.payload_length ==
             header.file_count * sizeof(FilesystemRecord) + header.bytes_used &&
         header.payload_length <= kFilesystemMaxPayload &&
         header.header_crc32 == filesystem_crc32(
             reinterpret_cast<const uint8_t *>(&header),
             offsetof(FilesystemHeader, header_crc32));
}

bool filesystem_slot_valid(uint32_t offset) {
  const auto *header = filesystem_slot_header(offset);
  if (!filesystem_header_valid(*header)) return false;
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
      !filesystem_slot_valid(slot_offset)) {
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

bool restore_overlay_slot(uint32_t offset, uint8_t slot, bool legacy) {
  for (auto &entry : overlay) entry = OverlayFile{};
  overlay_file_count = 0;
  overlay_bytes_used = 0;
  const auto *header = filesystem_slot_header(offset);
  const uint8_t *payload = filesystem_slot_data(offset) + kFilesystemPayloadOffset;
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
    return false;
  }
  overlay_file_count = header->file_count;
  overlay_bytes_used = header->bytes_used;
  overlay_dirty = false;
  filesystem_active_slot = legacy ? 0xff : slot;
  filesystem_generation = header->generation;
  return true;
}

bool restore_overlay_snapshot(bool legacy) {
  const uint32_t offset0 = legacy ? flash_layout::legacy_filesystem_slot(0)
                                  : kFilesystemSlot0;
  const uint32_t offset1 = legacy ? flash_layout::legacy_filesystem_slot(1)
                                  : kFilesystemSlot1;
  const bool valid0 = filesystem_slot_valid(offset0);
  const bool valid1 = filesystem_slot_valid(offset1);
  if (!valid0 && !valid1) return false;
  uint8_t first = valid0 ? 0 : 1;
  if (valid0 && valid1 &&
      static_cast<int32_t>(filesystem_slot_header(offset1)->generation -
                           filesystem_slot_header(offset0)->generation) > 0) {
    first = 1;
  }
  const uint32_t first_offset = first == 0 ? offset0 : offset1;
  if (restore_overlay_slot(first_offset, first, legacy)) return true;
  const uint8_t other = first == 0 ? 1 : 0;
  if (!(other == 0 ? valid0 : valid1)) return false;
  return restore_overlay_slot(other == 0 ? offset0 : offset1, other, legacy);
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
          if (newest_live == nullptr || file.offset > newest_live->offset) {
            newest_live = &file;
            newest_live_slot = slot;
          }
        } else if (newest_tombstone == nullptr ||
                   file.offset > newest_tombstone->offset) {
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
      file.offset >= overlay_bytes_used) {
    return false;
  }

  const uint8_t header = overlay_bytes[file.offset];
  const uint8_t key_length = header & 0x7f;
  const uint8_t *stored = overlay_bytes + file.offset + 1;
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
    overlay_dirty = true;
    return true;
  }

  const size_t record_key_length = encoded_tag_length(tag, tag_length);
  if (overlay_bytes_used + 1 + record_key_length + existing_length >
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
  overlay_dirty = true;
  return true;
}

void set_path(uint8_t (*tag)[128], uint8_t *length,
              std::initializer_list<uint8_t> bytes) {
  *length = static_cast<uint8_t>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), (*tag));
}

} // namespace

extern "C" uint32_t choupi_available_transient_memory_bytes(void) {
#if defined(__GLIBC__) && \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33))
  const struct mallinfo2 allocator = mallinfo2();
#else
  const struct mallinfo allocator = mallinfo();
#endif
  const uintptr_t heap_end = reinterpret_cast<uintptr_t>(sbrk(0));
  const uintptr_t heap_limit = reinterpret_cast<uintptr_t>(&__StackLimit);
  const size_t uncommitted = heap_end < heap_limit ? heap_limit - heap_end : 0;
  const uint64_t available =
      static_cast<uint64_t>(allocator.fordblks) + uncommitted;
  return available > UINT32_MAX ? UINT32_MAX
                                : static_cast<uint32_t>(available);
}

extern "C" uint32_t choupi_available_persistent_memory_bytes(void) {
  if (!filesystem_layout_available || overlay_file_count >= kMaxOverlayFiles ||
      overlay_bytes_used >= kMaxOverlayBytes) {
    return 0;
  }
  return static_cast<uint32_t>(kMaxOverlayBytes - overlay_bytes_used);
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
  const bool restored_current = restore_overlay_snapshot(false);
  const bool restored_legacy =
      !restored_current && restore_overlay_snapshot(true);
  if (restored_legacy) {
    // The old snapshots overlap CAP3, but their CRCs prove that the selected
    // snapshot survived. Commit the RAM copy outside both old snapshot slots.
    // If power fails, the old snapshot is still available on the next boot.
    overlay_dirty = true;
    if (!persist_overlay()) return 1;
    std::printf("CHOUPI:FS_MIGRATE generation=%lu\r\n",
                static_cast<unsigned long>(filesystem_generation));
    std::fflush(stdout);
  }
  if (!restored_current && !restored_legacy) {
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
      !overlay_has_storage(file) || file.offset >= overlay_bytes_used) {
    return 1;
  }

  // Re-encode the existing key bytes verbatim. In particular, a compressed
  // child can outlive a parent that was deleted by an older firmware; its
  // canonical tag can be reconstructed, while find_overlay() cannot resolve
  // it through the now-deleted parent. Preserving the encoding lets us write
  // a durable tombstone without resolving that parent again.
  const uint8_t key_header = overlay_bytes[file.offset];
  const size_t key_length = key_header & 0x7f;
  const size_t old_record_size = 1 + key_length + file.size;
  if (old_record_size > overlay_bytes_used - file.offset) return 1;
  uint8_t encoded_key[128]{};
  if (key_length + 1 > sizeof(encoded_key)) return 1;
  std::memcpy(encoded_key, overlay_bytes + file.offset, key_length + 1);

  const size_t old_offset = file.offset;
  const size_t remaining = overlay_bytes_used - old_offset - old_record_size;
  std::memmove(overlay_bytes + old_offset,
               overlay_bytes + old_offset + old_record_size, remaining);
  for (size_t index = 0; index < kMaxOverlayFiles; ++index) {
    if (&overlay[index] != &file && overlay_has_storage(overlay[index]) &&
        overlay[index].offset > old_offset) {
      overlay[index].offset -= static_cast<uint32_t>(old_record_size);
    }
  }

  const size_t new_offset = overlay_bytes_used - old_record_size;
  std::memcpy(overlay_bytes + new_offset, encoded_key, key_length + 1);
  overlay_bytes_used = new_offset + key_length + 1;
  file.offset = static_cast<uint32_t>(new_offset);
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

int main_arm(void) {
  choupi_reader_led_init();
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
    // Consume the retained marker after copying it to RAM. A later reset
    // should report a new VM trace, not repeat this diagnostic indefinitely.
    watchdog_hw->scratch[0] = 0;
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
    choupi_reader_led_task();
    watchdog_update();
    if (time_reached(next_report)) {
      uint8_t ccid_stage = 0;
      uint8_t ccid_endpoint = 0;
      uint16_t ccid_rx_events = 0;
      uint16_t ccid_task_count = 0;
      uint8_t last_apdu_length = 0;
      uint8_t last_apdu_header[5] = {};
      uint8_t last_message_type = 0;
      uint8_t last_payload_length = 0;
      uint16_t xfr_block_count = 0;
      uint16_t out_xfer_errors = 0;
      uint16_t in_xfer_errors = 0;
      uint16_t out_rearm_errors = 0;
      uint16_t in_submit_errors = 0;
      uint16_t invalid_messages = 0;
      uint8_t last_xfer_result = 0;
      uint8_t last_unknown_message = 0;
      uint16_t unknown_message_count = 0;
      uint8_t recent_commands[12] = {};
      uint16_t command_count = 0;
      uint8_t response_apdu_header[5] = {};
      uint16_t response_length = 0;
      uint16_t response_status = 0;
      choupi_ccid_debug_state(&ccid_stage, &ccid_endpoint, &ccid_rx_events,
                              &ccid_task_count, &last_apdu_length,
                              last_apdu_header, &last_message_type,
                              &last_payload_length, &xfr_block_count,
                              &out_xfer_errors, &in_xfer_errors,
                              &out_rearm_errors, &in_submit_errors,
                              &invalid_messages, &last_xfer_result);
      choupi_ccid_debug_response(response_apdu_header, &response_length,
                                 &response_status);
      choupi_ccid_debug_unknown(&last_unknown_message,
                                &unknown_message_count);
      choupi_ccid_debug_recent(recent_commands, sizeof(recent_commands),
                               &command_count);
      printf("CHOUPI:CCID stage=%u endpoint=%02X rx_events=%u tasks=%u xfr=%u msg=%02X len=%u apdu=%u:%02X%02X%02X%02X%02X\r\n",
             static_cast<unsigned>(ccid_stage),
             static_cast<unsigned>(ccid_endpoint),
             static_cast<unsigned>(ccid_rx_events),
             static_cast<unsigned>(ccid_task_count),
             static_cast<unsigned>(xfr_block_count),
             static_cast<unsigned>(last_message_type),
             static_cast<unsigned>(last_payload_length),
             static_cast<unsigned>(last_apdu_length), last_apdu_header[0],
             last_apdu_header[1], last_apdu_header[2], last_apdu_header[3],
             last_apdu_header[4]);
      if (out_xfer_errors != 0 || in_xfer_errors != 0 ||
          out_rearm_errors != 0 || in_submit_errors != 0 ||
          invalid_messages != 0) {
        printf("CHOUPI:CCID_ERRORS out_xfer=%u in_xfer=%u out_rearm=%u "
               "in_submit=%u invalid=%u last_result=%u\r\n",
               static_cast<unsigned>(out_xfer_errors),
               static_cast<unsigned>(in_xfer_errors),
               static_cast<unsigned>(out_rearm_errors),
               static_cast<unsigned>(in_submit_errors),
               static_cast<unsigned>(invalid_messages),
               static_cast<unsigned>(last_xfer_result));
      }
      printf("CHOUPI:CCID_RESPONSE apdu=%02X%02X%02X%02X%02X len=%u sw=%04X\r\n",
             response_apdu_header[0], response_apdu_header[1],
             response_apdu_header[2], response_apdu_header[3],
             response_apdu_header[4], static_cast<unsigned>(response_length),
             static_cast<unsigned>(response_status));
      uint8_t response_data[64] = {};
      uint8_t response_data_length = 0;
      if (choupi_ccid_debug_take_response_data(
              response_data, sizeof(response_data), &response_data_length)) {
        printf("CHOUPI:CCID_RESPONSE_DATA len=%u hex=",
               static_cast<unsigned>(response_data_length));
        for (uint8_t index = 0; index < response_data_length; ++index) {
          printf("%02X", response_data[index]);
        }
        printf("\r\n");
      }
      if (unknown_message_count != 0) {
        printf("CHOUPI:CCID_UNKNOWN last=%02X count=%u\r\n",
               static_cast<unsigned>(last_unknown_message),
               static_cast<unsigned>(unknown_message_count));
      }
      if (command_count != 0) {
        printf("CHOUPI:CCID_COMMANDS count=%u recent=",
               static_cast<unsigned>(command_count));
        for (uint8_t index = 0; index < sizeof(recent_commands); ++index) {
          printf("%02X", static_cast<unsigned>(recent_commands[index]));
        }
        printf("\r\n");
      }
      printf("CHOUPI:STATUS sw=%04X\r\n", status);
      printf("TEST:%s choupi_runtime\r\n", passed ? "PASS" : "FAIL");
      printf("TEST:SUMMARY pass=%u fail=%u\r\n", passed ? 1u : 0u,
             passed ? 0u : 1u);
      if (!passed) {
        uint16_t runtime_status = 0;
        uint16_t exception_id = 0;
        uint8_t opcode = 0;
        uint32_t pc = 0;
        uint16_t stack_depth = 0;
        uint32_t opcode_history = 0;
        uint32_t depth_history = 0;
        uint8_t field_bounds_failure = 0;
        uint16_t field_index = 0;
        uint16_t field_count = 0;
        uint16_t field_package = 0;
        uint16_t field_class = 0;
        uint16_t field_value_ref = 0;
        choupi_vm_runtime_debug_state(&runtime_status, &exception_id,
                                     &opcode, &pc, &stack_depth,
                                     &opcode_history, &depth_history,
                                     &field_bounds_failure, &field_index,
                                     &field_count, &field_package,
                                     &field_class, &field_value_ref);
        printf("CHOUPI:VM_RUNTIME_FAILURE sw=%04X exception=%u opcode=%02X "
               "pc=%08lX stack=%u op_history=%08lX depth_history=%08lX\r\n",
               static_cast<unsigned>(runtime_status),
               static_cast<unsigned>(exception_id),
               static_cast<unsigned>(opcode), static_cast<unsigned long>(pc),
               static_cast<unsigned>(stack_depth),
               static_cast<unsigned long>(opcode_history),
               static_cast<unsigned long>(depth_history));
        if (field_bounds_failure != 0) {
          printf("CHOUPI:FIELD_BOUNDS index=%u count=%u package=%u class=%u "
                 "value_ref=%04X\r\n",
                 static_cast<unsigned>(field_index),
                 static_cast<unsigned>(field_count),
                 static_cast<unsigned>(field_package),
                 static_cast<unsigned>(field_class),
                 static_cast<unsigned>(field_value_ref));
        }
        uint8_t cp_recorded = 0;
        uint8_t cp_succeeded = 0;
        uint8_t cp_package = 0;
        uint16_t cp_index = 0;
        uint8_t cp_token = 0;
        uint8_t cp_exception = 0;
        choupi_vm_field_cp_state(&cp_recorded, &cp_succeeded, &cp_package,
                                 &cp_index, &cp_token, &cp_exception);
        if (cp_recorded != 0) {
          printf("CHOUPI:FIELD_CP_LOOKUP package=%u cp=%u succeeded=%u "
                 "token=%u exception=%u\r\n",
                 static_cast<unsigned>(cp_package),
                 static_cast<unsigned>(cp_index),
                 static_cast<unsigned>(cp_succeeded),
                 static_cast<unsigned>(cp_token),
                 static_cast<unsigned>(cp_exception));
        }
        uint8_t heap_ref_recorded = 0;
        uint16_t heap_ref = 0;
        uint16_t heap_offset = 0;
        uint8_t heap_is_array = 0;
        choupi_vm_field_heap_ref_state(&heap_ref_recorded, &heap_ref,
                                       &heap_offset, &heap_is_array);
        if (heap_ref_recorded != 0) {
          printf("CHOUPI:PUTFIELD_HEAP_REF object=%04X offset=%u is_array=%u\r\n",
                 static_cast<unsigned>(heap_ref),
                 static_cast<unsigned>(heap_offset),
                 static_cast<unsigned>(heap_is_array));
        }
        uint8_t restore_recorded = 0;
        uint16_t restore_package = 0;
        uint16_t restore_class = 0;
        uint16_t restore_index = 0;
        uint8_t restore_type = 0;
        uint8_t restore_stage = 0;
        uint8_t restore_exception = 0;
        choupi_vm_persistent_restore_failure_state(
            &restore_recorded, &restore_package, &restore_class,
            &restore_index, &restore_type, &restore_stage,
            &restore_exception);
        if (restore_recorded != 0) {
          printf("CHOUPI:PERSISTENT_RESTORE_FAIL package=%u class=%u "
                 "field=%u type=%u stage=%u exception=%u\r\n",
                 static_cast<unsigned>(restore_package),
                 static_cast<unsigned>(restore_class),
                 static_cast<unsigned>(restore_index),
                 static_cast<unsigned>(restore_type),
                 static_cast<unsigned>(restore_stage),
                 static_cast<unsigned>(restore_exception));
        }
        uint8_t putfield_stage = 0;
        uint16_t putfield_object = 0;
        uint16_t putfield_value = 0;
        uint8_t putfield_exception = 0;
        choupi_vm_putfield_stage_state(&putfield_stage, &putfield_object,
                                       &putfield_value,
                                       &putfield_exception);
        if (putfield_stage != 0) {
          printf("CHOUPI:PUTFIELD_STAGE stage=%u object=%04X value=%04X exception=%u\r\n",
                 static_cast<unsigned>(putfield_stage),
                 static_cast<unsigned>(putfield_object),
                 static_cast<unsigned>(putfield_value),
                 static_cast<unsigned>(putfield_exception));
        }
        uint8_t write_stage = 0;
        uint16_t write_package = 0;
        uint16_t write_class = 0;
        uint16_t write_index = 0;
        uint16_t write_value = 0;
        uint16_t write_count = 0;
        uint8_t write_flags = 0;
        uint8_t write_exception = 0;
        choupi_vm_field_write_state(
            &write_stage, &write_package, &write_class, &write_index,
            &write_value, &write_count, &write_flags, &write_exception);
        if (write_stage != 0) {
          printf("CHOUPI:FIELD_WRITE stage=%u package=%u class=%u index=%u "
                 "value=%04X count=%u flags=%02X exception=%u\r\n",
                 static_cast<unsigned>(write_stage),
                 static_cast<unsigned>(write_package),
                 static_cast<unsigned>(write_class),
                 static_cast<unsigned>(write_index),
                 static_cast<unsigned>(write_value),
                 static_cast<unsigned>(write_count),
                 static_cast<unsigned>(write_flags),
                 static_cast<unsigned>(write_exception));
        }
        uint8_t array_failure_recorded = 0;
        uint8_t array_failure_stage = 0;
        uint16_t array_failure_index = 0;
        uint16_t array_failure_size = 0;
        uint16_t array_failure_ref = 0;
        uint8_t array_failure_type = 0;
        uint8_t array_failure_exception = 0;
        choupi_vm_array_write_failure_state(
            &array_failure_recorded, &array_failure_stage,
            &array_failure_index, &array_failure_size, &array_failure_ref,
            &array_failure_type, &array_failure_exception);
        if (array_failure_recorded != 0) {
          printf("CHOUPI:ARRAY_WRITE_FAIL stage=%u index=%u size=%u "
                 "ref=%04X type=%u exception=%u\r\n",
                 static_cast<unsigned>(array_failure_stage),
                 static_cast<unsigned>(array_failure_index),
                 static_cast<unsigned>(array_failure_size),
                 static_cast<unsigned>(array_failure_ref),
                 static_cast<unsigned>(array_failure_type),
                 static_cast<unsigned>(array_failure_exception));
        }
      }
      if (watchdog_recovery_pending) {
        printf("CHOUPI:WATCHDOG_RECOVERY marker=494E5354 cause=%s pc=%08lX opcode=%02lX count=%lu stage=%lu context=%08lX origin=%08lX\r\n",
               watchdog_recovery_by_watchdog ? "watchdog" : "other",
               static_cast<unsigned long>(watchdog_recovery_pc),
               static_cast<unsigned long>(watchdog_recovery_info >> 24),
               static_cast<unsigned long>(watchdog_recovery_info & 0x00ffffffu),
               static_cast<unsigned long>(watchdog_recovery_stage),
               static_cast<unsigned long>(watchdog_recovery_context),
               static_cast<unsigned long>(watchdog_recovery_origin));
        watchdog_recovery_pending = false;
      }
      fflush(stdout);
      next_report = delayed_by_ms(next_report, 1000);
    }
    tight_loop_contents();
  }
}
