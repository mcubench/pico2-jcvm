#include "pc_rommask.hpp"

#ifdef PC_VERSION

#include <array>
#include <utility>

namespace choupi::pc {
namespace {

constexpr uint8_t kHeaderTag = 1;
constexpr uint8_t kMaxComponentTag = 11;
constexpr size_t kMinHeaderPayload = 13;

uint16_t read_be16(const uint8_t *bytes) {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                               static_cast<uint16_t>(bytes[1]));
}

bool parse_package_at(const uint8_t *image, size_t image_length,
                      size_t start, RomPackage &package, size_t &end) {
  if (image == nullptr || start > image_length || image_length - start < 3 ||
      image[start] != kHeaderTag) {
    return false;
  }

  const size_t header_length = read_be16(image + start + 1);
  if (header_length < kMinHeaderPayload ||
      header_length > image_length - start - 3) {
    return false;
  }
  const uint8_t *header = image + start + 3;
  if (header[0] != 0xde || header[1] != 0xca || header[2] != 0xff ||
      header[3] != 0xed) {
    return false;
  }
  const uint8_t aid_length = header[9];
  if (aid_length < 5 || aid_length > 16 ||
      header_length < static_cast<size_t>(10 + aid_length)) {
    return false;
  }

  std::array<bool, kMaxComponentTag + 1> seen{};
  size_t cursor = start;
  size_t component_count = 0;
  while (cursor < image_length) {
    if (image_length - cursor < 3) break;
    const uint8_t tag = image[cursor];
    if (tag == kHeaderTag && cursor != start) break;
    if (tag == 0 || tag > kMaxComponentTag || seen[tag]) break;
    const size_t component_length = read_be16(image + cursor + 1);
    if (component_length > image_length - cursor - 3) break;
    seen[tag] = true;
    ++component_count;
    cursor += component_length + 3;
  }
  if (!seen[kHeaderTag] || !seen[2] || component_count < 3 || cursor <= start) {
    return false;
  }

  package.aid.assign(header + 10, header + 10 + aid_length);
  package.components.assign(image + start, image + cursor);
  end = cursor;
  return true;
}

} // namespace

bool parse_rommask_packages(const uint8_t *image, const size_t image_length,
                            std::vector<RomPackage> &packages) {
  packages.clear();
  if (image == nullptr || image_length < 16) return false;

  size_t offset = 0;
  while (offset + 3 <= image_length) {
    if (image[offset] != kHeaderTag) {
      ++offset;
      continue;
    }
    RomPackage package;
    size_t package_end = offset;
    if (!parse_package_at(image, image_length, offset, package, package_end)) {
      ++offset;
      continue;
    }
    if (packages.size() >= 64) {
      packages.clear();
      return false;
    }
    package.package_id = static_cast<uint8_t>(packages.size());
    packages.push_back(std::move(package));
    // The ROM-mask filesystem can place one CAP image inside a component
    // payload belonging to another package. Keep scanning so those package
    // headers retain the same identifiers as the ROM-mask generator.
    ++offset;
  }
  return !packages.empty();
}

} // namespace choupi::pc

#endif // PC_VERSION
