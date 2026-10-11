#include "cap_store.hpp"

#include <cstddef>
#include <cstring>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"

extern "C" const uint8_t choupi_rommask_end[];

namespace {
constexpr uint32_t kSlotOffset = 0x1c0000;
constexpr uint32_t kSlotSize = 0x40000;
constexpr uint32_t kHeaderSectorSize = FLASH_SECTOR_SIZE;
constexpr uint32_t kCodeOffset = kSlotOffset + kHeaderSectorSize;
constexpr size_t kMaxCodeLength = 28 * 1024;
constexpr uint32_t kHeaderMagic = 0x32415043; // "CAP2" in little endian
constexpr uint16_t kHeaderVersion = 2;
constexpr uint16_t kLegacyHeaderVersion = 1;
constexpr uint8_t kPackageId = 20;
constexpr uint32_t kXipBase = 0x10000000;

struct __attribute__((packed)) CapSlotHeader {
  uint32_t magic;
  uint16_t version;
  uint8_t package_id;
  uint8_t installed;
  uint32_t code_length;
  uint32_t code_crc32;
  uint8_t instance_aid_length;
  uint8_t instance_aid[16];
  uint32_t header_crc32;
  // Appended after the original CRC so version-1 headers remain readable.
  uint16_t applet_class_index;
  uint32_t applet_class_crc32;
};

static_assert(sizeof(CapSlotHeader) <= FLASH_PAGE_SIZE,
              "CAP slot header must fit in one flash page");
static_assert((kSlotOffset % FLASH_SECTOR_SIZE) == 0,
              "CAP slot must be sector aligned");
static_assert((kSlotSize % FLASH_SECTOR_SIZE) == 0,
              "CAP slot size must be sector aligned");
static_assert(kCodeOffset % FLASH_PAGE_SIZE == 0,
              "CAP code must be page aligned");
static_assert(kCodeOffset + kMaxCodeLength <= kSlotOffset + kSlotSize,
              "CAP code exceeds the reserved slot");
static_assert(PICO_FLASH_SIZE_BYTES >= kSlotOffset + kSlotSize,
              "Pico flash is too small for the CAP slot");

const CapSlotHeader *cached_header = nullptr;
const uint8_t *cached_code = nullptr;
uint32_t cached_code_length = 0;

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

uint32_t applet_class_crc(const CapSlotHeader &header) noexcept {
  uint32_t crc = crc32(
      reinterpret_cast<const uint8_t *>(&header.applet_class_index),
      sizeof(header.applet_class_index));
  crc ^= crc32(header.instance_aid, header.instance_aid_length);
  return crc;
}

const CapSlotHeader *slot_header() noexcept {
  return reinterpret_cast<const CapSlotHeader *>(kXipBase + kSlotOffset);
}

bool slot_is_inside_reserved_flash() noexcept {
  const uintptr_t image_end = reinterpret_cast<uintptr_t>(choupi_rommask_end);
  return image_end < static_cast<uintptr_t>(kXipBase + kSlotOffset) &&
         kSlotOffset + kSlotSize <= PICO_FLASH_SIZE_BYTES;
}

bool validate_header(const CapSlotHeader &header) noexcept {
  if (header.magic != kHeaderMagic ||
      (header.version != kLegacyHeaderVersion &&
       header.version != kHeaderVersion) ||
      header.package_id != kPackageId || header.code_length == 0 ||
      header.code_length > kMaxCodeLength ||
      header.instance_aid_length > sizeof(header.instance_aid) ||
      header.header_crc32 != header_crc(header)) {
    return false;
  }
  return header.version == kLegacyHeaderVersion ||
         header.applet_class_crc32 == applet_class_crc(header);
}

void build_header_page(uint8_t page[FLASH_PAGE_SIZE],
                       const CapSlotHeader &header) noexcept {
  for (size_t index = 0; index < FLASH_PAGE_SIZE; ++index) page[index] = 0xff;
  const auto *source = reinterpret_cast<const uint8_t *>(&header);
  for (size_t index = 0; index < sizeof(header); ++index) page[index] = source[index];
}

struct WriteContext {
  const uint8_t *data;
  size_t length;
  uint32_t data_crc;
  bool success = false;
};

void __not_in_flash_func(write_slot_callback)(void *opaque) {
  auto *context = static_cast<WriteContext *>(opaque);
  flash_range_erase(kSlotOffset, kSlotSize);

  uint8_t page[FLASH_PAGE_SIZE];
  size_t offset = 0;
  while (offset < context->length) {
    for (size_t index = 0; index < FLASH_PAGE_SIZE; ++index) page[index] = 0xff;
    const size_t remaining = context->length - offset;
    const size_t chunk = remaining < FLASH_PAGE_SIZE ? remaining : FLASH_PAGE_SIZE;
    for (size_t index = 0; index < chunk; ++index) {
      page[index] = context->data[offset + index];
    }
    flash_range_program(kCodeOffset + static_cast<uint32_t>(offset), page,
                        FLASH_PAGE_SIZE);
    offset += chunk;
  }

  CapSlotHeader header{};
  header.magic = kHeaderMagic;
  header.version = kHeaderVersion;
  header.package_id = kPackageId;
  header.installed = 0;
  header.code_length = static_cast<uint32_t>(context->length);
  header.code_crc32 = context->data_crc;
  header.instance_aid_length = 0;
  for (auto &byte : header.instance_aid) byte = 0xff;
  header.applet_class_index = 0xffff;
  header.applet_class_crc32 = applet_class_crc(header);
  header.header_crc32 = header_crc(header);
  build_header_page(page, header);
  flash_range_program(kSlotOffset, page, FLASH_PAGE_SIZE);
  context->success = true;
}

struct MetadataContext {
  uint8_t page[FLASH_PAGE_SIZE];
  bool success = false;
};

void __not_in_flash_func(write_metadata_callback)(void *opaque) {
  auto *context = static_cast<MetadataContext *>(opaque);
  flash_range_erase(kSlotOffset, kHeaderSectorSize);
  flash_range_program(kSlotOffset, context->page, FLASH_PAGE_SIZE);
  context->success = true;
}

} // namespace

bool cap_store_initialize() noexcept {
  cached_header = nullptr;
  cached_code = nullptr;
  cached_code_length = 0;
  if (!slot_is_inside_reserved_flash()) return false;

  const CapSlotHeader *header = slot_header();
  if (!validate_header(*header)) return false;
  const auto *code = reinterpret_cast<const uint8_t *>(kXipBase + kCodeOffset);
  if (crc32(code, header->code_length) != header->code_crc32) return false;

  cached_header = header;
  cached_code = code;
  cached_code_length = header->code_length;
  return true;
}

bool cap_store_write(const uint8_t *data, size_t length) noexcept {
  if (data == nullptr || length == 0 || length > kMaxCodeLength ||
      !slot_is_inside_reserved_flash()) {
    return false;
  }
  WriteContext context{data, length, crc32(data, length), false};
  const int status = flash_safe_execute(write_slot_callback, &context, 5000);
  if (status != PICO_OK || !context.success || !cap_store_initialize() ||
      cached_code_length != length || cached_header->code_crc32 != context.data_crc) {
    return false;
  }
  return std::memcmp(cached_code, data, length) == 0;
}

bool cap_store_mark_installed(const uint8_t *instance_aid,
                              uint8_t instance_aid_length,
                              uint16_t applet_class_index) noexcept {
  if (!cap_store_initialize() || instance_aid == nullptr ||
      instance_aid_length < 5 || instance_aid_length > 16 ||
      applet_class_index == 0xffff) {
    return false;
  }
  CapSlotHeader header = *cached_header;
  header.version = kHeaderVersion;
  header.installed = 1;
  header.instance_aid_length = instance_aid_length;
  for (auto &byte : header.instance_aid) byte = 0xff;
  for (uint8_t index = 0; index < instance_aid_length; ++index) {
    header.instance_aid[index] = instance_aid[index];
  }
  header.applet_class_index = applet_class_index;
  header.applet_class_crc32 = applet_class_crc(header);
  header.header_crc32 = header_crc(header);

  MetadataContext context{};
  build_header_page(context.page, header);
  const int status = flash_safe_execute(write_metadata_callback, &context, 5000);
  if (status != PICO_OK || !context.success) return false;
  return cap_store_initialize();
}

const uint8_t *cap_store_data(uint32_t *length) noexcept {
  if (length == nullptr || cached_code == nullptr) return nullptr;
  *length = cached_code_length;
  return cached_code;
}

bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept {
  aid_length = 0;
  if (cached_header == nullptr || cached_header->installed != 1 || aid == nullptr ||
      cached_header->instance_aid_length < 5 ||
      cached_header->instance_aid_length > 16) {
    return false;
  }
  aid_length = cached_header->instance_aid_length;
  std::memcpy(aid, cached_header->instance_aid, aid_length);
  return true;
}

bool cap_store_applet_class_index(uint16_t &class_index) noexcept {
  if (cached_header == nullptr || cached_header->installed != 1 ||
      cached_header->version != kHeaderVersion ||
      cached_header->applet_class_crc32 != applet_class_crc(*cached_header) ||
      cached_header->applet_class_index == 0xffff) {
    return false;
  }
  class_index = cached_header->applet_class_index;
  return true;
}
