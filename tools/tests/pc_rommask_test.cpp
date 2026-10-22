#include "pc_rommask.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

void add_component(std::vector<uint8_t> &image, uint8_t tag,
                   const std::vector<uint8_t> &payload) {
  image.push_back(tag);
  image.push_back(static_cast<uint8_t>(payload.size() >> 8));
  image.push_back(static_cast<uint8_t>(payload.size()));
  image.insert(image.end(), payload.begin(), payload.end());
}

void add_package(std::vector<uint8_t> &image,
                 const std::vector<uint8_t> &aid) {
  std::vector<uint8_t> header{0xde, 0xca, 0xff, 0xed, 1, 2, 0, 0, 1,
                              static_cast<uint8_t>(aid.size())};
  header.insert(header.end(), aid.begin(), aid.end());
  add_component(image, 1, header);
  add_component(image, 2, {0, 0});
  add_component(image, 4, {0});
  image.insert(image.end(), {0xff, 0x45});
}

bool expect(bool condition, const char *name) {
  if (!condition) std::fprintf(stderr, "FAIL: %s\n", name);
  return condition;
}

} // namespace

int main() {
  unsigned int passed = 0;
  unsigned int failed = 0;
  auto check = [&](bool condition, const char *name) {
    if (expect(condition, name)) {
      ++passed;
    } else {
      ++failed;
    }
  };

  std::vector<uint8_t> image(16, 0x00);
  const std::vector<uint8_t> aid0{0xa0, 0x00, 0x00, 0x01, 0x01};
  const std::vector<uint8_t> aid1{0xa0, 0x00, 0x00, 0x01, 0x02, 0x03};
  add_package(image, aid0);
  add_package(image, aid1);
  std::vector<choupi::pc::RomPackage> packages;
  check(choupi::pc::parse_rommask_packages(image.data(), image.size(), packages),
        "valid package stream parses");
  check(packages.size() == 2 && packages[0].package_id == 0 &&
            packages[1].package_id == 1,
        "package identifiers follow ROM mask order");
  check(packages.size() == 2 && packages[0].aid == aid0 &&
            packages[1].aid == aid1,
        "package AIDs are read from CAP headers");
  check(packages.size() == 2 && packages[0].components.size() == 27 &&
            packages[1].components.size() == 28,
        "component boundaries exclude inter-package padding");

  std::vector<uint8_t> nested_image;
  std::vector<uint8_t> nested_header{0xde, 0xca, 0xff, 0xed, 1, 2, 0, 0,
                                     1, static_cast<uint8_t>(aid0.size())};
  nested_header.insert(nested_header.end(), aid0.begin(), aid0.end());
  add_component(nested_image, 1, nested_header);
  add_component(nested_image, 2, {0, 0});
  add_component(nested_image, 4, {0});
  std::vector<uint8_t> containing_component{0x00, 0x01};
  std::vector<uint8_t> embedded_package;
  add_package(embedded_package, aid1);
  containing_component.insert(containing_component.end(),
                               embedded_package.begin(), embedded_package.end());
  add_component(nested_image, 8, containing_component);
  check(choupi::pc::parse_rommask_packages(nested_image.data(),
                                           nested_image.size(), packages) &&
            packages.size() == 2 && packages[1].aid == aid1,
        "nested package headers retain ROM-mask package order");

  const std::vector<uint8_t> invalid{0x01, 0x00, 0x01, 0xde};
  check(!choupi::pc::parse_rommask_packages(invalid.data(), invalid.size(),
                                             packages) &&
            packages.empty(),
        "truncated header is rejected and output is cleared");
  check(!choupi::pc::parse_rommask_packages(nullptr, 0, packages),
        "null image is rejected");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
