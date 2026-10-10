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
constexpr size_t kMaxOverlayFiles = 24;
constexpr size_t kMaxOverlayBytes = 4096;

struct OverlayFile {
  bool used = false;
  uint8_t tag_length = 0;
  uint8_t tag[32]{};
  uint32_t size = 0;
  uint8_t bytes[kMaxOverlayBytes]{};
};

OverlayFile overlay[kMaxOverlayFiles];

bool tags_equal(const uint8_t *left, uint8_t left_length,
                const uint8_t *right, uint8_t right_length) {
  return left_length == right_length &&
         std::memcmp(left, right, left_length) == 0;
}

OverlayFile *find_overlay(const uint8_t *tag, uint8_t tag_length) {
  for (auto &file : overlay) {
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
  for (auto &file : overlay) {
    if (!file.used) {
      file.used = true;
      file.tag_length = tag_length;
      std::memcpy(file.tag, tag, tag_length);
      return &file;
    }
  }
  return nullptr;
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
    *data = file->bytes;
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
  auto *file = allocate_overlay(tag, tag_length);
  if (file == nullptr || existing_length > sizeof(file->bytes)) {
    return false;
  }
  if (file->size != existing_length) {
    std::memcpy(file->bytes, existing, existing_length);
    file->size = existing_length;
  }
  std::memcpy(file->bytes + offset, data, length);
  return true;
}

void set_path(uint8_t (*tag)[32], uint8_t *length,
              std::initializer_list<uint8_t> bytes) {
  *length = static_cast<uint8_t>(bytes.size());
  std::copy(bytes.begin(), bytes.end(), (*tag));
}

} // namespace

extern "C" uint8_t fs_init(void) {
  for (auto &file : overlay) {
    file = OverlayFile{};
  }
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
  if (length > kMaxOverlayBytes) {
    return 1;
  }
  auto *file = allocate_overlay(tag, tag_length);
  if (file == nullptr) {
    return 1;
  }
  std::memcpy(file->bytes, data, length);
  file->size = length;
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
  for (unsigned int attempt = 0; attempt < 16 && !stdio_usb_connected();
       ++attempt) {
    printf("BOOT board=pico2 chip=rp2350 arch=arm runtime=choupi\r\n");
    fflush(stdout);
    sleep_ms(250);
  }
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
