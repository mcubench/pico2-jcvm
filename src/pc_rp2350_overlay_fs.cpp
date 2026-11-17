// PC/VPCD adapter for exercising the RP2350 filesystem implementation.
// This translation unit includes the production backend with private names,
// then exposes the OS filesystem ABI through GNU ld --wrap shims.

#define fs_init rp2350_overlay_fs_init
#define fs_write rp2350_overlay_fs_write
#define fs_write_range rp2350_overlay_fs_write_range
#define fs_write_applet rp2350_overlay_fs_write_applet
#define fs_erase rp2350_overlay_fs_erase
#define fs_erase_applet rp2350_overlay_fs_erase_applet
#define fs_exists rp2350_overlay_fs_exists
#define fs_next_tag rp2350_overlay_fs_next_tag
#define fs_read_inplace rp2350_overlay_fs_read_inplace
#define fs_read rp2350_overlay_fs_read
#define fs_read_1b_at rp2350_overlay_fs_read_1b_at
#define fs_read_2b_at rp2350_overlay_fs_read_2b_at
#define fs_read_4b_at rp2350_overlay_fs_read_4b_at
#define fs_write_1b_at rp2350_overlay_fs_write_1b_at
#define fs_write_2b_at rp2350_overlay_fs_write_2b_at
#define fs_write_4b_at rp2350_overlay_fs_write_4b_at
#define fs_length rp2350_overlay_fs_length
#define fs_drop rp2350_overlay_fs_drop
#define fs_sync rp2350_overlay_fs_sync
#define fs_erase_prefix rp2350_overlay_fs_erase_prefix
#define fs_erase_package_state rp2350_overlay_fs_erase_package_state
#include "pico_filesystem.cpp"

#undef fs_init
#undef fs_write
#undef fs_write_range
#undef fs_write_applet
#undef fs_erase
#undef fs_erase_applet
#undef fs_exists
#undef fs_next_tag
#undef fs_read_inplace
#undef fs_read
#undef fs_read_1b_at
#undef fs_read_2b_at
#undef fs_read_4b_at
#undef fs_write_1b_at
#undef fs_write_2b_at
#undef fs_write_4b_at
#undef fs_length
#undef fs_drop
#undef fs_sync
#undef fs_erase_prefix
#undef fs_erase_package_state

#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include "hardware/flash.h"
#include "pico/flash.h"

namespace {
constexpr size_t kFlashBytes = 2u * 1024u * 1024u;
std::array<uint8_t, kFlashBytes> fake_flash{};
bool fake_flash_loaded = false;
bool fake_flash_backing_exists = false;
size_t fake_flash_dirty_begin = kFlashBytes;
size_t fake_flash_dirty_end = 0;

bool load_fake_flash() {
  if (fake_flash_loaded) return true;
  fake_flash.fill(0xff);
  const char *path = std::getenv("PICO2_RP2350_OVERLAY_FLASH");
  if (path != nullptr && *path != '\0') {
    FILE *input = std::fopen(path, "rb");
    if (input != nullptr) {
      const size_t read = std::fread(fake_flash.data(), 1, fake_flash.size(),
                                     input);
      const bool exact = read == fake_flash.size() &&
                         std::fgetc(input) == EOF && std::ferror(input) == 0;
      std::fclose(input);
      if (!exact) {
        std::fprintf(stderr,
                     "RP2350_OVERLAY: invalid flash image size path=%s\n",
                     path);
        return false;
      }
      fake_flash_backing_exists = true;
    } else if (errno != ENOENT) {
      std::fprintf(stderr,
                   "RP2350_OVERLAY: cannot read flash image path=%s errno=%d\n",
                   path, errno);
      return false;
    }
  }
  fake_flash_loaded = true;
  return true;
}

bool save_fake_flash() {
  const char *path = std::getenv("PICO2_RP2350_OVERLAY_FLASH");
  if (path == nullptr || *path == '\0') {
    fake_flash_dirty_begin = kFlashBytes;
    fake_flash_dirty_end = 0;
    return true;
  }
  if (fake_flash_dirty_begin >= fake_flash_dirty_end) return true;

  const bool initialize_file = !fake_flash_backing_exists;
  FILE *output = std::fopen(path, initialize_file ? "wb" : "r+b");
  if (output == nullptr) return false;
  const size_t write_begin = initialize_file ? 0 : fake_flash_dirty_begin;
  const size_t write_length = initialize_file
                                  ? fake_flash.size()
                                  : fake_flash_dirty_end - fake_flash_dirty_begin;
  const bool positioned = initialize_file ||
      std::fseek(output, static_cast<long>(write_begin), SEEK_SET) == 0;
  const bool wrote = positioned &&
                     std::fwrite(fake_flash.data() + write_begin, 1,
                                 write_length, output) == write_length &&
                     std::fflush(output) == 0 && fsync(fileno(output)) == 0;
  const bool closed = std::fclose(output) == 0;
  if (!wrote || !closed) return false;
  fake_flash_backing_exists = true;
  fake_flash_dirty_begin = kFlashBytes;
  fake_flash_dirty_end = 0;
  return true;
}

void mark_fake_flash_dirty(uint32_t offset, size_t count) {
  if (count == 0) return;
  fake_flash_dirty_begin = std::min<size_t>(fake_flash_dirty_begin, offset);
  fake_flash_dirty_end = std::max<size_t>(fake_flash_dirty_end,
                                          static_cast<size_t>(offset) + count);
}
} // namespace

extern "C" const uint8_t *rp2350_fs_test_flash_base(void) {
  if (!load_fake_flash()) std::abort();
  return fake_flash.data();
}

extern "C" uint8_t rp2350_fs_test_manual_transaction_active(void) {
  return 0;
}

void flash_range_erase(uint32_t offset, size_t count) {
  if (!load_fake_flash() || offset > fake_flash.size() ||
      count > fake_flash.size() - offset || offset % FLASH_SECTOR_SIZE != 0 ||
      count % FLASH_SECTOR_SIZE != 0) {
    std::abort();
  }
  std::fill(fake_flash.begin() + offset,
            fake_flash.begin() + offset + count, 0xff);
  mark_fake_flash_dirty(offset, count);
}

void flash_range_program(uint32_t offset, const uint8_t *data, size_t count) {
  if (!load_fake_flash() || data == nullptr || offset > fake_flash.size() ||
      count > fake_flash.size() - offset || offset % FLASH_PAGE_SIZE != 0 ||
      count % FLASH_PAGE_SIZE != 0 ||
      reinterpret_cast<uintptr_t>(data) % 4 != 0) {
    std::abort();
  }
  for (size_t index = 0; index < count; ++index) {
    if ((fake_flash[offset + index] & data[index]) != data[index]) {
      std::abort();
    }
    fake_flash[offset + index] &= data[index];
  }
  mark_fake_flash_dirty(offset, count);
}

int flash_safe_execute(void (*function)(void *), void *parameter,
                       uint32_t) {
  if (function == nullptr || !load_fake_flash()) return -1;
  try {
    function(parameter);
  } catch (...) {
    return -1;
  }
  return save_fake_flash() ? 0 : -1;
}

extern "C" uint8_t __wrap_fs_init(void) {
  return rp2350_overlay_fs_init();
}
extern "C" uint8_t __wrap_fs_write(const uint8_t *tag, uint8_t tag_length,
                                    const uint8_t *data, uint32_t length) {
  return rp2350_overlay_fs_write(tag, tag_length, data, length);
}
extern "C" uint8_t __wrap_fs_write_range(const uint8_t *tag,
                                          uint8_t tag_length,
                                          uint32_t offset,
                                          const uint8_t *data,
                                          uint32_t length) {
  return rp2350_overlay_fs_write_range(tag, tag_length, offset, data, length);
}
extern "C" void __wrap_fs_write_applet(const uint8_t *tag,
                                        uint8_t tag_length,
                                        const uint8_t *data,
                                        uint32_t length) {
  (void)rp2350_overlay_fs_write(tag, tag_length, data, length);
}
extern "C" uint8_t __wrap_fs_erase(const uint8_t *tag, uint8_t tag_length) {
  return rp2350_overlay_fs_erase(tag, tag_length);
}
extern "C" void __wrap_fs_erase_applet(const uint8_t *tag,
                                         uint8_t tag_length) {
  (void)rp2350_overlay_fs_erase(tag, tag_length);
}
extern "C" uint8_t __wrap_fs_exists(const uint8_t *tag, uint8_t tag_length) {
  return rp2350_overlay_fs_exists(tag, tag_length);
}
extern "C" uint8_t __wrap_fs_next_tag(const uint8_t *prefix,
                                       uint8_t prefix_length,
                                       fs_tag_next_request_t *request) {
  return rp2350_overlay_fs_next_tag(prefix, prefix_length, request);
}
extern "C" uint8_t __wrap_fs_read_inplace(const uint8_t *tag,
                                           uint8_t tag_length,
                                           const uint8_t **data,
                                           uint32_t *length) {
  return rp2350_overlay_fs_read_inplace(tag, tag_length, data, length);
}
extern "C" uint8_t __wrap_fs_read(const uint8_t *tag, uint8_t tag_length,
                                   uint8_t *data, uint32_t length) {
  return rp2350_overlay_fs_read(tag, tag_length, data, length);
}
extern "C" uint8_t __wrap_fs_read_1b_at(const uint8_t *tag,
                                         uint8_t tag_length, uint32_t offset,
                                         uint8_t *result) {
  return rp2350_overlay_fs_read_1b_at(tag, tag_length, offset, result);
}
extern "C" uint8_t __wrap_fs_read_2b_at(const uint8_t *tag,
                                         uint8_t tag_length, uint32_t offset,
                                         uint16_t *result) {
  return rp2350_overlay_fs_read_2b_at(tag, tag_length, offset, result);
}
extern "C" uint8_t __wrap_fs_read_4b_at(const uint8_t *tag,
                                         uint8_t tag_length, uint32_t offset,
                                         uint32_t *result) {
  return rp2350_overlay_fs_read_4b_at(tag, tag_length, offset, result);
}
extern "C" uint8_t __wrap_fs_write_1b_at(const uint8_t *tag,
                                          uint8_t tag_length, uint32_t offset,
                                          uint8_t data) {
  return rp2350_overlay_fs_write_1b_at(tag, tag_length, offset, data);
}
extern "C" uint8_t __wrap_fs_write_2b_at(const uint8_t *tag,
                                          uint8_t tag_length, uint32_t offset,
                                          uint16_t data) {
  return rp2350_overlay_fs_write_2b_at(tag, tag_length, offset, data);
}
extern "C" uint8_t __wrap_fs_write_4b_at(const uint8_t *tag,
                                          uint8_t tag_length, uint32_t offset,
                                          uint32_t data) {
  return rp2350_overlay_fs_write_4b_at(tag, tag_length, offset, data);
}
extern "C" uint8_t __wrap_fs_length(const uint8_t *tag, uint8_t tag_length,
                                     uint32_t *length) {
  return rp2350_overlay_fs_length(tag, tag_length, length);
}
extern "C" void __wrap_fs_drop(void) {
  (void)rp2350_overlay_fs_drop();
}
extern "C" uint8_t __wrap_fs_sync(void) {
  const uint8_t status = rp2350_overlay_fs_sync();
  const char *scan_diagnostics = std::getenv("PICO2_GC_SCAN_DIAGNOSTICS");
  static uint32_t sync_sequence = 0;
  if (scan_diagnostics != nullptr &&
      std::strcmp(scan_diagnostics, "1") == 0 &&
      (++sync_sequence <= 10 || sync_sequence % 100 == 0)) {
    std::fprintf(stderr,
                 "RP2350_OVERLAY:SYNC seq=%u status=%u generation=%u "
                 "files=%zu bytes=%zu dirty=%u transaction=%u\n",
                 sync_sequence, static_cast<unsigned>(status),
                 static_cast<unsigned>(filesystem_generation),
                 overlay_file_count, overlay_bytes_used,
                 overlay_dirty ? 1u : 0u,
                 static_cast<unsigned>(
                     choupi_persistent_write_transaction_active()));
  }
  if (status != 0) {
    std::fprintf(stderr, "RP2350_OVERLAY:sync_failed status=%u flash_path=%s\n",
                 static_cast<unsigned>(status),
                 std::getenv("PICO2_RP2350_OVERLAY_FLASH") != nullptr
                     ? std::getenv("PICO2_RP2350_OVERLAY_FLASH")
                     : "<unset>");
  }
  return status;
}
extern "C" uint8_t __wrap_fs_erase_prefix(const uint8_t *prefix,
                                            uint8_t prefix_length) {
  return rp2350_overlay_fs_erase_prefix(prefix, prefix_length);
}
extern "C" uint8_t __wrap_fs_erase_package_state(uint8_t package_id) {
  return rp2350_overlay_fs_erase_package_state(package_id);
}
