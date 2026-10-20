#include "cap_store.hpp"

#include <cstddef>
#include <cstring>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"

extern "C" const uint8_t choupi_rommask_end[];

namespace {
// The first 1 MiB is reserved for the firmware and ROM mask. Dynamic CAPs
// occupy eight 68 KiB slots; each slot has a 4 KiB metadata sector and up to
// 64 KiB of page-programmed CAP data.
constexpr uint32_t kSlotsOffset = 0x100000;
constexpr uint32_t kSlotCount = 8;
constexpr uint32_t kHeaderSectorSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kMaxCodeLength = 64 * 1024;
constexpr uint32_t kSlotStride = kHeaderSectorSize + kMaxCodeLength;
constexpr uint32_t kCodeOffset = kHeaderSectorSize;
constexpr uint8_t kFirstPackageId = 56;
constexpr uint32_t kXipBase = 0x10000000;
constexpr uint32_t kHeaderMagic = 0x33415043; // "CAP3" in little endian
constexpr uint16_t kHeaderVersion = 3;

struct __attribute__((packed)) CapSlotHeader {
  uint32_t magic;
  uint16_t version;
  uint8_t package_id;
  uint8_t package_aid_length;
  uint32_t code_length;
  uint32_t code_crc32;
  uint8_t package_aid[16];
  uint8_t installed;
  uint8_t instance_aid_length;
  uint8_t instance_aid[16];
  uint16_t applet_class_index;
  uint16_t reserved;
  uint32_t header_crc32;
};

static_assert(kSlotCount * kSlotStride == 0x88000,
              "CAP slot region size changed; review flash partition map");
static_assert(sizeof(CapSlotHeader) <= FLASH_PAGE_SIZE,
              "CAP slot header must fit in one flash page");
static_assert((kSlotsOffset % FLASH_SECTOR_SIZE) == 0,
              "CAP slot region must be sector aligned");
static_assert((kSlotStride % FLASH_SECTOR_SIZE) == 0,
              "CAP slot stride must be sector aligned");
static_assert((kCodeOffset % FLASH_PAGE_SIZE) == 0,
              "CAP code must be page aligned");
static_assert(PICO_FLASH_SIZE_BYTES >= kSlotsOffset + kSlotCount * kSlotStride,
              "Pico flash is too small for the CAP slot region");

const CapSlotHeader *cached_headers[kSlotCount]{};
const uint8_t *cached_codes[kSlotCount]{};
uint32_t cached_lengths[kSlotCount]{};

uint32_t crc32(const uint8_t *data, size_t length) noexcept {
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

uint32_t header_crc(const CapSlotHeader &header) noexcept {
  return crc32(reinterpret_cast<const uint8_t *>(&header),
               offsetof(CapSlotHeader, header_crc32));
}

uint32_t slot_offset(uint8_t slot) noexcept {
  return kSlotsOffset + static_cast<uint32_t>(slot) * kSlotStride;
}

const CapSlotHeader *slot_header(uint8_t slot) noexcept {
  return reinterpret_cast<const CapSlotHeader *>(
      kXipBase + slot_offset(slot));
}

const uint8_t *slot_code(uint8_t slot) noexcept {
  return reinterpret_cast<const uint8_t *>(
      kXipBase + slot_offset(slot) + kCodeOffset);
}

bool slot_is_inside_reserved_flash() noexcept {
  const uintptr_t image_end = reinterpret_cast<uintptr_t>(choupi_rommask_end);
  return image_end < static_cast<uintptr_t>(kXipBase + kSlotsOffset) &&
         kSlotsOffset + kSlotCount * kSlotStride <= PICO_FLASH_SIZE_BYTES;
}

bool aid_valid(const uint8_t *aid, uint8_t length) noexcept {
  return aid != nullptr && length >= 5 && length <= 16;
}

bool validate_header(const CapSlotHeader &header, uint8_t slot) noexcept {
  return header.magic == kHeaderMagic && header.version == kHeaderVersion &&
         header.package_id == static_cast<uint8_t>(kFirstPackageId + slot) &&
         header.package_aid_length >= 5 && header.package_aid_length <= 16 &&
         header.code_length != 0 && header.code_length <= kMaxCodeLength &&
         header.installed <= 1 &&
         (header.installed == 0 ||
          (header.instance_aid_length >= 5 &&
           header.instance_aid_length <= 16 &&
           header.applet_class_index != 0xffff)) &&
         header.header_crc32 == header_crc(header);
}

void build_header_page(uint8_t page[FLASH_PAGE_SIZE],
                       const CapSlotHeader &header) noexcept {
  std::memset(page, 0xff, FLASH_PAGE_SIZE);
  std::memcpy(page, &header, sizeof(header));
}

struct WriteContext {
  uint8_t slot;
  const uint8_t *data;
  size_t length;
  CapSlotHeader header;
  bool success = false;
};

void __not_in_flash_func(write_slot_callback)(void *opaque) {
  auto *context = static_cast<WriteContext *>(opaque);
  const uint32_t base = slot_offset(context->slot);
  flash_range_erase(base, kSlotStride);

  uint8_t page[FLASH_PAGE_SIZE];
  size_t offset = 0;
  while (offset < context->length) {
    std::memset(page, 0xff, sizeof(page));
    const size_t remaining = context->length - offset;
    const size_t chunk = remaining < sizeof(page) ? remaining : sizeof(page);
    std::memcpy(page, context->data + offset, chunk);
    flash_range_program(base + kCodeOffset + static_cast<uint32_t>(offset),
                        page, sizeof(page));
    offset += chunk;
  }
  build_header_page(page, context->header);
  flash_range_program(base, page, sizeof(page));
  context->success = true;
}

struct MetadataContext {
  uint8_t slot;
  uint8_t page[FLASH_PAGE_SIZE];
  bool success = false;
};

void __not_in_flash_func(write_metadata_callback)(void *opaque) {
  auto *context = static_cast<MetadataContext *>(opaque);
  flash_range_erase(slot_offset(context->slot), kHeaderSectorSize);
  flash_range_program(slot_offset(context->slot), context->page,
                      FLASH_PAGE_SIZE);
  context->success = true;
}

struct DeleteContext {
  uint8_t slot;
  bool success = false;
};

void __not_in_flash_func(delete_slot_callback)(void *opaque) {
  auto *context = static_cast<DeleteContext *>(opaque);
  flash_range_erase(slot_offset(context->slot), kSlotStride);
  context->success = true;
}

bool write_metadata(uint8_t slot, CapSlotHeader &header) noexcept {
  header.header_crc32 = header_crc(header);
  MetadataContext context{};
  context.slot = slot;
  build_header_page(context.page, header);
  const int status = flash_safe_execute(write_metadata_callback, &context, 5000);
  return status == PICO_OK && context.success && cap_store_initialize();
}

} // namespace

bool cap_store_initialize() noexcept {
  if (!slot_is_inside_reserved_flash()) return false;
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    cached_headers[slot] = nullptr;
    cached_codes[slot] = nullptr;
    cached_lengths[slot] = 0;
    const CapSlotHeader *header = slot_header(slot);
    if (!validate_header(*header, slot)) continue;
    const uint8_t *code = slot_code(slot);
    if (crc32(code, header->code_length) != header->code_crc32) continue;
    cached_headers[slot] = header;
    cached_codes[slot] = code;
    cached_lengths[slot] = header->code_length;
  }
  return true;
}

bool cap_store_find_package(const uint8_t *package_aid,
                            uint8_t package_aid_length,
                            uint8_t &package_id) noexcept {
  if (!aid_valid(package_aid, package_aid_length)) return false;
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    const auto *header = cached_headers[slot];
    if (header != nullptr && header->package_aid_length == package_aid_length &&
        std::memcmp(header->package_aid, package_aid, package_aid_length) == 0) {
      package_id = header->package_id;
      return true;
    }
  }
  return false;
}

bool cap_store_write_package(uint8_t package_id, const uint8_t *package_aid,
                             uint8_t package_aid_length, const uint8_t *data,
                             size_t length) noexcept {
  if (package_id < kFirstPackageId || package_id >= kFirstPackageId + kSlotCount ||
      !aid_valid(package_aid, package_aid_length) || data == nullptr ||
      length == 0 || length > kMaxCodeLength || !slot_is_inside_reserved_flash()) {
    return false;
  }
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  CapSlotHeader header{};
  header.magic = kHeaderMagic;
  header.version = kHeaderVersion;
  header.package_id = package_id;
  header.package_aid_length = package_aid_length;
  header.code_length = static_cast<uint32_t>(length);
  header.code_crc32 = crc32(data, length);
  std::memcpy(header.package_aid, package_aid, package_aid_length);
  std::memset(header.package_aid + package_aid_length, 0xff,
              sizeof(header.package_aid) - package_aid_length);
  header.installed = 0;
  header.instance_aid_length = 0;
  std::memset(header.instance_aid, 0xff, sizeof(header.instance_aid));
  header.applet_class_index = 0xffff;
  header.reserved = 0xffff;
  header.header_crc32 = header_crc(header);

  WriteContext context{slot, data, length, header, false};
  const int status = flash_safe_execute(write_slot_callback, &context, 5000);
  if (status != PICO_OK || !context.success || !cap_store_initialize() ||
      cached_headers[slot] == nullptr || cached_lengths[slot] != length ||
      std::memcmp(cached_codes[slot], data, length) != 0) {
    return false;
  }
  return true;
}

bool cap_store_delete_package(uint8_t package_id) noexcept {
  if (package_id < kFirstPackageId || package_id >= kFirstPackageId + kSlotCount ||
      !slot_is_inside_reserved_flash()) return false;
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  DeleteContext context{slot, false};
  const int status = flash_safe_execute(delete_slot_callback, &context, 5000);
  if (status != PICO_OK || !context.success) return false;
  return cap_store_initialize();
}

const uint8_t *cap_store_data_for_package(uint8_t package_id,
                                          uint32_t *length) noexcept {
  if (length == nullptr || package_id < kFirstPackageId ||
      package_id >= kFirstPackageId + kSlotCount) return nullptr;
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  if (cached_codes[slot] == nullptr) return nullptr;
  *length = cached_lengths[slot];
  return cached_codes[slot];
}

bool cap_store_package_aid(uint8_t package_id, uint8_t *aid,
                           uint8_t &aid_length) noexcept {
  aid_length = 0;
  if (aid == nullptr || package_id < kFirstPackageId ||
      package_id >= kFirstPackageId + kSlotCount) return false;
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  if (cached_headers[slot] == nullptr) return false;
  aid_length = cached_headers[slot]->package_aid_length;
  std::memcpy(aid, cached_headers[slot]->package_aid, aid_length);
  return true;
}

bool cap_store_mark_package_installed(uint8_t package_id,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index) noexcept {
  if (package_id < kFirstPackageId || package_id >= kFirstPackageId + kSlotCount ||
      !aid_valid(instance_aid, instance_aid_length) ||
      applet_class_index == 0xffff) return false;
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  if (cached_headers[slot] == nullptr) return false;
  CapSlotHeader header = *cached_headers[slot];
  header.installed = 1;
  header.instance_aid_length = instance_aid_length;
  std::memset(header.instance_aid, 0xff, sizeof(header.instance_aid));
  std::memcpy(header.instance_aid, instance_aid, instance_aid_length);
  header.applet_class_index = applet_class_index;
  return write_metadata(slot, header);
}

bool cap_store_package_instance(uint8_t package_id, uint8_t *aid,
                                uint8_t &aid_length) noexcept {
  aid_length = 0;
  if (aid == nullptr || package_id < kFirstPackageId ||
      package_id >= kFirstPackageId + kSlotCount) return false;
  const uint8_t slot = static_cast<uint8_t>(package_id - kFirstPackageId);
  if (cached_headers[slot] == nullptr || cached_headers[slot]->installed != 1 ||
      cached_headers[slot]->instance_aid_length < 5 ||
      cached_headers[slot]->instance_aid_length > 16) return false;
  aid_length = cached_headers[slot]->instance_aid_length;
  std::memcpy(aid, cached_headers[slot]->instance_aid, aid_length);
  return true;
}

bool cap_store_package_class_index(uint8_t package_id,
                                   uint16_t &class_index) noexcept {
  if (package_id < kFirstPackageId || package_id >= kFirstPackageId + kSlotCount)
    return false;
  const auto *header = cached_headers[package_id - kFirstPackageId];
  if (header == nullptr || header->installed != 1 ||
      header->applet_class_index == 0xffff) return false;
  class_index = header->applet_class_index;
  return true;
}

// Temporary compatibility entry points used by older single-package code.
// The card manager migrates to package-indexed calls in the next step.
bool cap_store_write(const uint8_t *data, size_t length) noexcept {
  constexpr uint8_t legacy_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                   0x54, 0x65, 0x73, 0x74};
  return cap_store_write_package(63, legacy_aid, sizeof(legacy_aid), data, length);
}

bool cap_store_delete() noexcept { return cap_store_delete_package(63); }

bool cap_store_mark_installed(const uint8_t *aid, uint8_t aid_length,
                              uint16_t applet_class_index) noexcept {
  return cap_store_mark_package_installed(63, aid, aid_length, applet_class_index);
}

const uint8_t *cap_store_data(uint32_t *length) noexcept {
  return cap_store_data_for_package(63, length);
}

bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept {
  return cap_store_package_instance(63, aid, aid_length);
}

bool cap_store_applet_class_index(uint16_t &class_index) noexcept {
  return cap_store_package_class_index(63, class_index);
}
