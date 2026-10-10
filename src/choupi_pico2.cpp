#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "ffi.h"
#include "jni.hpp"
#include "pico/stdlib.h"

extern "C" const uint8_t choupi_rommask_start[];
extern "C" const uint8_t choupi_rommask_end[];
extern "C" uint16_t runtime(uint8_t package_id, uint8_t class_id,
                             uint8_t method_id);

namespace {

constexpr size_t kRomFsOffset = 0x20000;
constexpr size_t kMaxOverlayFiles = 4096;
constexpr size_t kMaxOverlayBytes = 128 * 1024;

struct OverlayFile {
  bool used = false;
  uint8_t tag_length = 0;
  uint8_t tag[32]{};
  uint32_t offset = 0;
  uint32_t size = 0;
};

OverlayFile overlay[kMaxOverlayFiles];
size_t overlay_file_count = 0;
uint8_t overlay_bytes[kMaxOverlayBytes];
size_t overlay_bytes_used = 0;

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

bool tags_equal(const uint8_t *left, uint8_t left_length,
                const uint8_t *right, uint8_t right_length) {
  return left_length == right_length &&
         std::memcmp(left, right, left_length) == 0;
}

OverlayFile *find_overlay(const uint8_t *tag, uint8_t tag_length) {
  for (size_t index = 0; index < overlay_file_count; ++index) {
    auto &file = overlay[index];
    if (file.used && tags_equal(file.tag, file.tag_length, tag, tag_length)) {
      return &file;
    }
  }
  return nullptr;
}

OverlayFile *allocate_overlay(const uint8_t *tag, uint8_t tag_length) {
  if (tag_length > sizeof(overlay[0].tag)) {
    return nullptr;
  }
  if (auto *file = find_overlay(tag, tag_length)) {
    return file;
  }
  if (overlay_file_count == kMaxOverlayFiles) {
    return nullptr;
  }
  auto &file = overlay[overlay_file_count++];
  file = OverlayFile{};
  file.used = true;
  file.tag_length = tag_length;
  std::memcpy(file.tag, tag, tag_length);
  return &file;
}

bool append_overlay(OverlayFile &file, const uint8_t *data, uint32_t length) {
  const size_t old_size = file.size;
  const size_t new_used = overlay_bytes_used - old_size + length;
  if (new_used > kMaxOverlayBytes) {
    return false;
  }
  const size_t old_offset = file.offset;
  if (old_size != 0) {
    std::memmove(overlay_bytes + old_offset,
                 overlay_bytes + old_offset + old_size,
                 overlay_bytes_used - old_offset - old_size);
    for (size_t index = 0; index < overlay_file_count; ++index) {
      if (&overlay[index] != &file && overlay[index].offset > old_offset) {
        overlay[index].offset -= static_cast<uint32_t>(old_size);
      }
    }
  }
  file.offset = static_cast<uint32_t>(overlay_bytes_used - old_size);
  if (length != 0) {
    std::memcpy(overlay_bytes + file.offset, data, length);
  }
  file.size = length;
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
    *data = overlay_bytes + file->offset;
    *length = file->size;
    return true;
  }
  return find_rom_file(tag, tag_length, data, length);
}

bool write_at(const uint8_t *tag, uint8_t tag_length, uint32_t offset,
              const uint8_t *data, uint32_t length) {
  const uint8_t *existing = nullptr;
  uint32_t existing_length = 0;
  if (!find_file(tag, tag_length, &existing, &existing_length) ||
      offset > existing_length || length > existing_length - offset) {
    return false;
  }
  auto *file = find_overlay(tag, tag_length);
  if (file != nullptr) {
    if (file->size != existing_length) {
      report_fs_write_failure("update-size", tag, tag_length, existing_length);
      return false;
    }
    std::memcpy(overlay_bytes + file->offset + offset, data, length);
    return true;
  }

  file = allocate_overlay(tag, tag_length);
  if (file == nullptr ||
      overlay_bytes_used + existing_length > kMaxOverlayBytes) {
    report_fs_write_failure("update", tag, tag_length, existing_length);
    return false;
  }
  file->offset = static_cast<uint32_t>(overlay_bytes_used);
  file->size = existing_length;
  if (existing_length != 0) {
    std::memcpy(overlay_bytes + file->offset, existing, existing_length);
  }
  overlay_bytes_used += existing_length;
  std::memcpy(overlay_bytes + file->offset + offset, data, length);
  return true;
}

void set_path(uint8_t (*tag)[32], uint8_t *length,
              std::initializer_list<uint8_t> bytes) {
  *length = static_cast<uint8_t>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), (*tag));
}

} // namespace

extern "C" uint8_t fs_init(void) {
  overlay_file_count = 0;
  overlay_bytes_used = 0;
  return 0;
}

extern "C" uint8_t fs_exists(const uint8_t *tag, uint8_t tag_length) {
  const uint8_t *data = nullptr;
  uint32_t length = 0;
  return find_file(tag, tag_length, &data, &length) ? 1 : 0;
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
  auto *file = allocate_overlay(tag, tag_length);
  if (file == nullptr || !append_overlay(*file, data, length)) {
    report_fs_write_failure("new", tag, tag_length, length);
    return 1;
  }
  return 0;
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

extern "C" void path_package_list(uint8_t (*tag)[32], uint8_t *length) {
  set_path(tag, length, {0x00});
}

extern "C" void path_cap(uint8_t package, uint8_t (*tag)[32],
                         uint8_t *length) {
  set_path(tag, length, {0x01, package});
}

extern "C" void path_static(uint8_t package, uint8_t field,
                            uint8_t (*tag)[32], uint8_t *length) {
  set_path(tag, length, {0x02, package, field});
}

extern "C" void path_applet_field(uint8_t applet, uint8_t package,
                                  uint8_t class_id, uint8_t field,
                                  uint8_t (*tag)[32], uint8_t *length) {
  set_path(tag, length, {0x03, applet, package, class_id, field});
}

int main_arm(void) {
  stdio_init_all();
  for (unsigned int attempt = 0; attempt < 120 && !stdio_usb_connected();
       ++attempt) {
    printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
    fflush(stdout);
    sleep_ms(250);
  }
  sleep_ms(500);
  printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
  fflush(stdout);
  if (fs_init() != 0) {
    printf("TEST:FAIL choupi_fs_init\r\n");
    for (;;) {
      tight_loop_contents();
    }
  }

  uint16_t status = 0x6F00;
  try {
    status = runtime(STARTING_JAVACARD_PACKAGE, STARTING_JAVACARD_CLASS,
                     STARTING_JAVACARD_METHOD);
  } catch (...) {
    printf("TEST:FAIL choupi_uncaught_exception\r\n");
  }
  printf("CHOUPI:STATUS sw=%04X\r\n", status);
  const bool passed = status == 0x9000;
  for (;;) {
    printf("CHOUPI:STATUS sw=%04X\r\n", status);
    printf("TEST:%s choupi_runtime\r\n", passed ? "PASS" : "FAIL");
    printf("TEST:SUMMARY pass=%u fail=%u\r\n", passed ? 1u : 0u,
           passed ? 0u : 1u);
    fflush(stdout);
    sleep_ms(1000);
    tight_loop_contents();
  }
}
