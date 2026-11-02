#include "cap_store.hpp"
#include "flash_layout.hpp"
#include "applet_owner_allocator.hpp"

#include <cstddef>
#include <cstring>

#include "hardware/flash.h"
#include "pico/flash.h"
#include "pico/platform.h"

#ifdef RP2350_FS_OVERLAY_TEST
extern "C" const uint8_t *rp2350_fs_test_flash_base(void);
#endif

extern "C" const uint8_t choupi_rommask_end[];
#ifdef RP2350_FS_OVERLAY_TEST
extern "C" bool choupi_vm_applet_owner_in_use(uint8_t owner_id) noexcept
    __attribute__((weak));
#else
extern "C" bool choupi_vm_applet_owner_in_use(uint8_t owner_id) noexcept;
#endif

namespace {
// The first 1 MiB is reserved for the firmware and ROM mask. Dynamic CAPs
// occupy eight 68 KiB slots; each slot has a 4 KiB metadata sector and up to
// 64 KiB of page-programmed CAP data.
constexpr uint32_t kSlotsOffset = flash_layout::cap_offset;
constexpr uint32_t kSlotCount = flash_layout::cap_slot_count;
constexpr uint32_t kHeaderSectorSize = FLASH_SECTOR_SIZE;
// JC_Cap exposes component images through a 16-bit length, so reserve one
// byte below 64 KiB rather than allowing a value that would wrap to zero.
constexpr uint32_t kMaxCodeLength = 64 * 1024 - 1;
constexpr uint32_t kSlotStride = flash_layout::cap_slot_size;
constexpr uint32_t kCodeOffset = kHeaderSectorSize;
constexpr uint32_t kMetadataMirrorOffset =
    flash_layout::cap_metadata_mirror_offset;
constexpr uint32_t kMetadataMirrorSectorSize =
    flash_layout::cap_metadata_mirror_sector_size;
constexpr uint8_t kFirstPackageId = 56;
constexpr uint8_t kMaxPackageInstances = 40;
constexpr uint32_t kXipBase = 0x10000000;
constexpr uint32_t kHeaderMagic = 0x33415043; // "CAP3" in little endian
constexpr uint16_t kHeaderVersion = 5;
constexpr uint16_t kLegacyHeaderVersion = 3;
constexpr uint16_t kPreviousHeaderVersion = 4;
constexpr uint32_t kOwnerMapMagic = 0x31524f41; // "AOR1"
constexpr uint8_t kOwnerMapVersion = 1;

struct __attribute__((packed)) AppletInstanceRecord {
  uint8_t applet_aid_length;
  uint8_t instance_aid_length;
  uint16_t class_index;
  uint8_t applet_aid[16];
  uint8_t instance_aid[16];
};

struct __attribute__((packed)) CapSlotHeader {
  uint32_t magic;
  uint16_t version;
  uint8_t package_id;
  uint8_t package_aid_length;
  uint32_t code_length;
  uint32_t code_crc32;
  uint8_t package_aid[16];
  uint8_t instance_count;
  // Version 3 stored 0xffff here. Version 4 uses it as an update generation.
  uint16_t generation;
  AppletInstanceRecord instances[kMaxPackageInstances];
  uint32_t header_crc32;
};

// Keep owner assignments in the unused tail of the existing metadata sector.
// This preserves the v3/v4 header layout and lets old cards retain owner 0
// until their applet instance metadata is explicitly rewritten.
struct __attribute__((packed)) AppletOwnerEntry {
  uint8_t instance_aid_length;
  uint8_t instance_aid[16];
  uint8_t owner_id;
};

struct __attribute__((packed)) AppletOwnerMap {
  uint32_t magic;
  uint8_t version;
  uint8_t count;
  AppletOwnerEntry entries[kMaxPackageInstances];
  uint32_t crc32;
};

struct __attribute__((packed)) SlotMetadataImage {
  CapSlotHeader header;
  AppletOwnerMap owner_map;
};

static_assert(kSlotCount * kSlotStride == 0x88000,
              "CAP slot region size changed; review flash partition map");
static_assert(kSlotStride == kHeaderSectorSize + 64 * 1024,
              "CAP slot must fit metadata and code");
static_assert(sizeof(CapSlotHeader) <= FLASH_SECTOR_SIZE,
              "CAP slot header must fit in one metadata sector");
static_assert(sizeof(SlotMetadataImage) <= FLASH_SECTOR_SIZE,
              "CAP slot metadata and applet owner map must fit one sector");
static_assert((kSlotsOffset % FLASH_SECTOR_SIZE) == 0,
              "CAP slot region must be sector aligned");
static_assert((kSlotStride % FLASH_SECTOR_SIZE) == 0,
              "CAP slot stride must be sector aligned");
static_assert((kCodeOffset % FLASH_PAGE_SIZE) == 0,
              "CAP code must be page aligned");
static_assert(PICO_FLASH_SIZE_BYTES >= kSlotsOffset + kSlotCount * kSlotStride,
              "Pico flash is too small for the CAP slot region");
static_assert(PICO_FLASH_SIZE_BYTES >= flash_layout::filesystem_end,
              "Pico flash is too small for CAP and filesystem regions");

const CapSlotHeader *cached_headers[kSlotCount]{};
const uint8_t *cached_codes[kSlotCount]{};
uint32_t cached_lengths[kSlotCount]{};
bool cached_header_is_mirror[kSlotCount]{};
const AppletOwnerMap *cached_owner_maps[kSlotCount]{};

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

uint32_t owner_map_crc(const AppletOwnerMap &owner_map) noexcept {
  return crc32(reinterpret_cast<const uint8_t *>(&owner_map),
               offsetof(AppletOwnerMap, crc32));
}

uint32_t slot_offset(uint8_t slot) noexcept {
  return kSlotsOffset + static_cast<uint32_t>(slot) * kSlotStride;
}

const CapSlotHeader *slot_header(uint8_t slot) noexcept {
#ifdef RP2350_FS_OVERLAY_TEST
  return reinterpret_cast<const CapSlotHeader *>(
      rp2350_fs_test_flash_base() + slot_offset(slot));
#else
  return reinterpret_cast<const CapSlotHeader *>(
      kXipBase + slot_offset(slot));
#endif
}

const CapSlotHeader *mirror_header(uint8_t slot) noexcept {
#ifdef RP2350_FS_OVERLAY_TEST
  return reinterpret_cast<const CapSlotHeader *>(
      rp2350_fs_test_flash_base() + kMetadataMirrorOffset +
      static_cast<uint32_t>(slot) * kMetadataMirrorSectorSize);
#else
  return reinterpret_cast<const CapSlotHeader *>(
      kXipBase + kMetadataMirrorOffset +
      static_cast<uint32_t>(slot) * kMetadataMirrorSectorSize);
#endif
}

const uint8_t *slot_code(uint8_t slot) noexcept {
#ifdef RP2350_FS_OVERLAY_TEST
  return rp2350_fs_test_flash_base() + slot_offset(slot) + kCodeOffset;
#else
  return reinterpret_cast<const uint8_t *>(
      kXipBase + slot_offset(slot) + kCodeOffset);
#endif
}

bool slot_is_inside_reserved_flash() noexcept {
#ifdef RP2350_FS_OVERLAY_TEST
  return true;
#else
  const uintptr_t image_end = reinterpret_cast<uintptr_t>(choupi_rommask_end);
  return image_end < static_cast<uintptr_t>(kXipBase + kSlotsOffset) &&
         kSlotsOffset + kSlotCount * kSlotStride <= PICO_FLASH_SIZE_BYTES;
#endif
}

bool aid_valid(const uint8_t *aid, uint8_t length) noexcept {
  return aid != nullptr && length >= 5 && length <= 16;
}

bool validate_header(const CapSlotHeader &header, uint8_t slot) noexcept {
  return header.magic == kHeaderMagic &&
         (header.version == kHeaderVersion ||
          header.version == kPreviousHeaderVersion ||
          header.version == kLegacyHeaderVersion) &&
         header.package_id == static_cast<uint8_t>(kFirstPackageId + slot) &&
         header.package_aid_length >= 5 && header.package_aid_length <= 16 &&
         header.code_length != 0 && header.code_length <= kMaxCodeLength &&
         header.instance_count <= kMaxPackageInstances &&
         header.header_crc32 == header_crc(header);
}

bool validate_owner_map(const CapSlotHeader &header,
                        const AppletOwnerMap &owner_map) noexcept {
  if (header.version != kHeaderVersion ||
      owner_map.magic != kOwnerMapMagic ||
      owner_map.version != kOwnerMapVersion ||
      owner_map.count != header.instance_count ||
      owner_map.count > kMaxPackageInstances ||
      owner_map.crc32 != owner_map_crc(owner_map)) return false;

  bool used_owner[256]{};
  for (uint8_t index = 0; index < owner_map.count; ++index) {
    const auto &entry = owner_map.entries[index];
    const auto &instance = header.instances[index];
    if (entry.instance_aid_length < 5 || entry.instance_aid_length > 16 ||
        entry.instance_aid_length != instance.instance_aid_length ||
        std::memcmp(entry.instance_aid, instance.instance_aid,
                    entry.instance_aid_length) != 0 ||
        (entry.owner_id != 0 && used_owner[entry.owner_id])) return false;
    if (entry.owner_id != 0) used_owner[entry.owner_id] = true;
  }
  return true;
}

void finalize_owner_map(AppletOwnerMap &owner_map,
                        uint8_t instance_count) noexcept {
  owner_map.magic = kOwnerMapMagic;
  owner_map.version = kOwnerMapVersion;
  owner_map.count = instance_count;
  owner_map.crc32 = owner_map_crc(owner_map);
}

void current_owner_map(uint8_t slot, const CapSlotHeader &header,
                       AppletOwnerMap &owner_map) noexcept {
  std::memset(&owner_map, 0xff, sizeof(owner_map));
  owner_map.count = header.instance_count;
  const AppletOwnerMap *cached = cached_owner_maps[slot];
  for (uint8_t index = 0; index < header.instance_count; ++index) {
    const auto &instance = header.instances[index];
    auto &entry = owner_map.entries[index];
    entry.instance_aid_length = instance.instance_aid_length;
    std::memset(entry.instance_aid, 0xff, sizeof(entry.instance_aid));
    std::memcpy(entry.instance_aid, instance.instance_aid,
                instance.instance_aid_length);
    entry.owner_id = 0; // legacy v3/v4 records use the original owner.
    if (cached != nullptr && index < cached->count &&
        cached->entries[index].instance_aid_length ==
            instance.instance_aid_length &&
        std::memcmp(cached->entries[index].instance_aid,
                    instance.instance_aid,
                    instance.instance_aid_length) == 0) {
      entry.owner_id = cached->entries[index].owner_id;
    }
  }
  finalize_owner_map(owner_map, header.instance_count);
}

int find_slot(uint8_t package_id) noexcept {
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    if (cached_headers[slot] != nullptr &&
        cached_headers[slot]->package_id == package_id) return slot;
  }
  return -1;
}

struct WriteContext {
  uint8_t slot;
  const uint8_t *data;
  size_t length;
  SlotMetadataImage image;
  bool success = false;
};

void __not_in_flash_func(write_slot_callback)(void *opaque) {
  auto *context = static_cast<WriteContext *>(opaque);
  const uint32_t base = slot_offset(context->slot);
  flash_range_erase(
      kMetadataMirrorOffset +
          static_cast<uint32_t>(context->slot) * kMetadataMirrorSectorSize,
      kMetadataMirrorSectorSize);
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
  const auto *metadata_bytes =
      reinterpret_cast<const uint8_t *>(&context->image);
  for (uint32_t page_offset = 0; page_offset < FLASH_SECTOR_SIZE;
       page_offset += FLASH_PAGE_SIZE) {
    std::memset(page, 0xff, sizeof(page));
    if (page_offset < sizeof(context->image)) {
      const size_t remaining = sizeof(context->image) - page_offset;
      const size_t chunk = remaining < sizeof(page) ? remaining : sizeof(page);
      std::memcpy(page, metadata_bytes + page_offset, chunk);
    }
    flash_range_program(base + page_offset, page, FLASH_PAGE_SIZE);
  }
  context->success = true;
}

struct MetadataContext {
  uint8_t slot;
  bool mirror;
  SlotMetadataImage image;
  bool success = false;
};

void __not_in_flash_func(write_metadata_callback)(void *opaque) {
  auto *context = static_cast<MetadataContext *>(opaque);
  const uint32_t offset = context->mirror
      ? kMetadataMirrorOffset +
            static_cast<uint32_t>(context->slot) * kMetadataMirrorSectorSize
      : slot_offset(context->slot);
  flash_range_erase(offset, kHeaderSectorSize);
  uint8_t page[FLASH_PAGE_SIZE];
  const auto *metadata_bytes =
      reinterpret_cast<const uint8_t *>(&context->image);
  for (uint32_t page_offset = 0; page_offset < FLASH_SECTOR_SIZE;
       page_offset += FLASH_PAGE_SIZE) {
    std::memset(page, 0xff, sizeof(page));
    if (page_offset < sizeof(context->image)) {
      const size_t remaining = sizeof(context->image) - page_offset;
      const size_t chunk = remaining < sizeof(page) ? remaining : sizeof(page);
      std::memcpy(page, metadata_bytes + page_offset, chunk);
    }
    flash_range_program(offset + page_offset, page,
                        FLASH_PAGE_SIZE);
  }
  context->success = true;
}

struct DeleteContext {
  uint8_t slot;
  bool success = false;
};

void __not_in_flash_func(delete_slot_callback)(void *opaque) {
  auto *context = static_cast<DeleteContext *>(opaque);
  flash_range_erase(
      kMetadataMirrorOffset +
          static_cast<uint32_t>(context->slot) * kMetadataMirrorSectorSize,
      kMetadataMirrorSectorSize);
  flash_range_erase(slot_offset(context->slot), kSlotStride);
  context->success = true;
}

bool write_metadata(uint8_t slot, CapSlotHeader &header,
                    const AppletOwnerMap &owner_map) noexcept {
  MetadataContext context{};
  context.slot = slot;
  context.mirror = !cached_header_is_mirror[slot];
  const auto *current = cached_headers[slot];
  header.version = kHeaderVersion;
  header.generation = current != nullptr && current->version >= 4
                         ? static_cast<uint16_t>(current->generation + 1u)
                         : 1u;
  header.header_crc32 = header_crc(header);
  context.image.header = header;
  context.image.owner_map = owner_map;
  context.image.owner_map.magic = kOwnerMapMagic;
  context.image.owner_map.version = kOwnerMapVersion;
  context.image.owner_map.count = header.instance_count;
  context.image.owner_map.crc32 =
      owner_map_crc(context.image.owner_map);
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
    cached_header_is_mirror[slot] = false;
    cached_owner_maps[slot] = nullptr;
    const CapSlotHeader *primary = slot_header(slot);
    const CapSlotHeader *mirror = mirror_header(slot);
    const auto *primary_map = reinterpret_cast<const AppletOwnerMap *>(
        reinterpret_cast<const uint8_t *>(primary) + sizeof(CapSlotHeader));
    const auto *mirror_map = reinterpret_cast<const AppletOwnerMap *>(
        reinterpret_cast<const uint8_t *>(mirror) + sizeof(CapSlotHeader));
    const bool primary_valid =
        validate_header(*primary, slot) &&
        (primary->version != kHeaderVersion ||
         validate_owner_map(*primary, *primary_map));
    const bool mirror_valid =
        validate_header(*mirror, slot) &&
        (mirror->version != kHeaderVersion ||
         validate_owner_map(*mirror, *mirror_map));
    const CapSlotHeader *header = nullptr;
    const AppletOwnerMap *owner_map = nullptr;
    if (primary_valid && mirror_valid) {
      const bool mirror_is_newer = mirror->version > primary->version ||
          (mirror->version == primary->version && mirror->version >= 4 &&
           static_cast<int16_t>(static_cast<uint16_t>(
               mirror->generation - primary->generation)) > 0);
      header = mirror_is_newer ? mirror : primary;
      owner_map = mirror_is_newer ? mirror_map : primary_map;
      cached_header_is_mirror[slot] = mirror_is_newer;
    } else if (primary_valid) {
      header = primary;
      owner_map = primary_map;
    } else if (mirror_valid) {
      header = mirror;
      owner_map = mirror_map;
      cached_header_is_mirror[slot] = true;
    } else {
      continue;
    }
    const uint8_t *code = slot_code(slot);
    if (crc32(code, header->code_length) != header->code_crc32) continue;
    cached_headers[slot] = header;
    cached_codes[slot] = code;
    cached_lengths[slot] = header->code_length;
    if (header->version == kHeaderVersion) cached_owner_maps[slot] = owner_map;
  }

  bool globally_used_owners[256]{};
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    const auto *owner_map = cached_owner_maps[slot];
    if (owner_map == nullptr) continue;
    for (uint8_t index = 0; index < owner_map->count; ++index) {
      const uint8_t owner_id = owner_map->entries[index].owner_id;
      if (owner_id != 0 && globally_used_owners[owner_id]) return false;
      if (owner_id != 0) globally_used_owners[owner_id] = true;
    }
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
  header.instance_count = 0;
  header.generation = 1;
  std::memset(header.instances, 0xff, sizeof(header.instances));
  header.header_crc32 = header_crc(header);
  AppletOwnerMap owner_map{};
  std::memset(&owner_map, 0xff, sizeof(owner_map));
  finalize_owner_map(owner_map, 0);
  WriteContext context{};
  context.slot = slot;
  context.data = data;
  context.length = length;
  context.image.header = header;
  context.image.owner_map = owner_map;
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

bool cap_store_allocate_applet_owner(uint8_t &owner_id) noexcept {
  bool used[256]{};
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    const auto *owner_map = cached_owner_maps[slot];
    if (owner_map == nullptr) continue;
    for (uint8_t index = 0; index < owner_map->count; ++index) {
      const uint8_t existing = owner_map->entries[index].owner_id;
      if (existing != 0) used[existing] = true;
    }
  }
  return jcvm::allocate_applet_owner_id(
      used, [](uint8_t candidate) noexcept {
#ifdef RP2350_FS_OVERLAY_TEST
        return choupi_vm_applet_owner_in_use != nullptr &&
               choupi_vm_applet_owner_in_use(candidate);
#else
        return choupi_vm_applet_owner_in_use(candidate);
#endif
      }, owner_id);
}

bool cap_store_applet_owner(const uint8_t *instance_aid,
                            const uint8_t instance_aid_length,
                            uint8_t &owner_id) noexcept {
  if (!aid_valid(instance_aid, instance_aid_length)) return false;
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    const auto *header = cached_headers[slot];
    if (header == nullptr) continue;
    for (uint8_t index = 0; index < header->instance_count; ++index) {
      const auto &instance = header->instances[index];
      if (instance.instance_aid_length != instance_aid_length ||
          std::memcmp(instance.instance_aid, instance_aid,
                      instance_aid_length) != 0) continue;
      const auto *owner_map = cached_owner_maps[slot];
      if (owner_map == nullptr) {
        owner_id = 0; // Existing v3/v4 registry entry.
        return true;
      }
      if (index >= owner_map->count) return false;
      owner_id = owner_map->entries[index].owner_id;
      return true;
    }
  }
  return false;
}

bool cap_store_mark_package_installed(uint8_t package_id,
                                      const uint8_t *applet_aid,
                                      uint8_t applet_aid_length,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index,
                                      uint8_t owner_id) noexcept {
  if (!aid_valid(applet_aid, applet_aid_length) ||
      !aid_valid(instance_aid, instance_aid_length) ||
      applet_class_index == 0xffff) return false;
  const int slot_index = find_slot(package_id);
  if (slot_index < 0) return false;
  const auto slot = static_cast<uint8_t>(slot_index);
  CapSlotHeader header = *cached_headers[slot];
  AppletOwnerMap owner_map{};
  current_owner_map(slot, header, owner_map);
  for (uint8_t other_slot = 0; other_slot < kSlotCount; ++other_slot) {
    const auto *other_header = cached_headers[other_slot];
    if (other_header == nullptr) continue;
    for (uint8_t index = 0; index < other_header->instance_count; ++index) {
      const auto &record = other_header->instances[index];
      if (record.instance_aid_length == instance_aid_length &&
          std::memcmp(record.instance_aid, instance_aid,
                      instance_aid_length) == 0) {
        const auto *existing_map = cached_owner_maps[other_slot];
        const uint8_t existing_owner = existing_map == nullptr
            ? 0 : existing_map->entries[index].owner_id;
        return other_slot == slot &&
               record.applet_aid_length == applet_aid_length &&
               std::memcmp(record.applet_aid, applet_aid,
                           applet_aid_length) == 0 &&
               record.class_index == applet_class_index &&
               existing_owner == owner_id;
      }
    }
  }
  bool owner_used[256]{};
  for (uint8_t other_slot = 0; other_slot < kSlotCount; ++other_slot) {
    const auto *other_map = cached_owner_maps[other_slot];
    if (other_map == nullptr) continue;
    for (uint8_t index = 0; index < other_map->count; ++index) {
      const uint8_t existing = other_map->entries[index].owner_id;
      if (existing != 0) owner_used[existing] = true;
    }
  }
  if (owner_id != 0 && owner_used[owner_id]) return false;
  if (header.instance_count >= kMaxPackageInstances) return false;
  auto &record = header.instances[header.instance_count++];
  record.applet_aid_length = applet_aid_length;
  record.instance_aid_length = instance_aid_length;
  record.class_index = applet_class_index;
  std::memset(record.applet_aid, 0xff, sizeof(record.applet_aid));
  std::memset(record.instance_aid, 0xff, sizeof(record.instance_aid));
  std::memcpy(record.applet_aid, applet_aid, applet_aid_length);
  std::memcpy(record.instance_aid, instance_aid, instance_aid_length);
  auto &owner_entry = owner_map.entries[header.instance_count - 1];
  owner_entry.instance_aid_length = instance_aid_length;
  std::memset(owner_entry.instance_aid, 0xff, sizeof(owner_entry.instance_aid));
  std::memcpy(owner_entry.instance_aid, instance_aid, instance_aid_length);
  owner_entry.owner_id = owner_id;
  finalize_owner_map(owner_map, header.instance_count);
  return write_metadata(slot, header, owner_map);
}

bool cap_store_mark_package_installed(uint8_t package_id,
                                     const uint8_t *applet_aid,
                                     uint8_t applet_aid_length,
                                     const uint8_t *instance_aid,
                                     uint8_t instance_aid_length,
                                     uint16_t applet_class_index) noexcept {
  uint8_t owner_id = 0;
  return cap_store_allocate_applet_owner(owner_id) &&
         cap_store_mark_package_installed(
             package_id, applet_aid, applet_aid_length, instance_aid,
             instance_aid_length, applet_class_index, owner_id);
}

bool cap_store_remove_package_instance(uint8_t package_id,
                                       const uint8_t *instance_aid,
                                       uint8_t instance_aid_length) noexcept {
  if (!aid_valid(instance_aid, instance_aid_length)) return false;
  const int slot_index = find_slot(package_id);
  if (slot_index < 0) return false;
  const auto slot = static_cast<uint8_t>(slot_index);
  CapSlotHeader header = *cached_headers[slot];
  AppletOwnerMap owner_map{};
  current_owner_map(slot, header, owner_map);
  for (uint8_t index = 0; index < header.instance_count; ++index) {
    auto &record = header.instances[index];
    if (record.instance_aid_length != instance_aid_length ||
        std::memcmp(record.instance_aid, instance_aid,
                    instance_aid_length) != 0) continue;
    for (uint8_t move = index; move + 1 < header.instance_count; ++move)
      header.instances[move] = header.instances[move + 1];
    for (uint8_t move = index; move + 1 < owner_map.count; ++move)
      owner_map.entries[move] = owner_map.entries[move + 1];
    --header.instance_count;
    --owner_map.count;
    std::memset(&header.instances[header.instance_count], 0xff,
                sizeof(AppletInstanceRecord));
    std::memset(&owner_map.entries[owner_map.count], 0xff,
                sizeof(AppletOwnerEntry));
    finalize_owner_map(owner_map, header.instance_count);
    return write_metadata(slot, header, owner_map);
  }
  return false;
}

bool cap_store_find_instance(const uint8_t *instance_aid,
                             uint8_t instance_aid_length,
                             uint8_t &package_id, uint8_t *applet_aid,
                             uint8_t &applet_aid_length,
                             uint16_t &class_index) noexcept {
  if (!aid_valid(instance_aid, instance_aid_length) || applet_aid == nullptr)
    return false;
  for (uint8_t slot = 0; slot < kSlotCount; ++slot) {
    const auto *header = cached_headers[slot];
    if (header == nullptr) continue;
    for (uint8_t index = 0; index < header->instance_count; ++index) {
      const auto &record = header->instances[index];
      if (record.instance_aid_length != instance_aid_length ||
          std::memcmp(record.instance_aid, instance_aid,
                      instance_aid_length) != 0) continue;
      package_id = header->package_id;
      applet_aid_length = record.applet_aid_length;
      std::memcpy(applet_aid, record.applet_aid, applet_aid_length);
      class_index = record.class_index;
      return true;
    }
  }
  return false;
}

uint8_t cap_store_package_instance_count(uint8_t package_id) noexcept {
  const int slot = find_slot(package_id);
  return slot < 0 ? 0 : cached_headers[slot]->instance_count;
}

bool cap_store_package_instance_at(uint8_t package_id, uint8_t index,
                                   uint8_t *applet_aid,
                                   uint8_t &applet_aid_length,
                                   uint8_t *instance_aid,
                                   uint8_t &instance_aid_length,
                                   uint16_t &class_index) noexcept {
  const int slot = find_slot(package_id);
  if (slot < 0 || applet_aid == nullptr || instance_aid == nullptr ||
      index >= cached_headers[slot]->instance_count) return false;
  const auto &record = cached_headers[slot]->instances[index];
  applet_aid_length = record.applet_aid_length;
  instance_aid_length = record.instance_aid_length;
  class_index = record.class_index;
  std::memcpy(applet_aid, record.applet_aid, applet_aid_length);
  std::memcpy(instance_aid, record.instance_aid, instance_aid_length);
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
  uint8_t owner_id = 0;
  if (!cap_store_allocate_applet_owner(owner_id)) return false;
  return cap_store_mark_package_installed(63, aid, aid_length, aid, aid_length,
                                          applet_class_index, owner_id);
}

const uint8_t *cap_store_data(uint32_t *length) noexcept {
  return cap_store_data_for_package(63, length);
}

bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept {
  uint8_t applet_aid[16]{};
  uint8_t applet_aid_length = 0;
  uint16_t class_index = 0xffff;
  return cap_store_package_instance_at(63, 0, applet_aid, applet_aid_length, aid,
                                       aid_length, class_index);
}

bool cap_store_applet_class_index(uint16_t &class_index) noexcept {
  uint8_t applet_aid[16]{};
  uint8_t applet_aid_length = 0;
  uint8_t instance_aid[16]{};
  uint8_t instance_aid_length = 0;
  return cap_store_package_instance_at(63, 0, applet_aid, applet_aid_length,
                                       instance_aid, instance_aid_length,
                                       class_index);
}
