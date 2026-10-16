#include "gp_registry.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
int checks = 0;

void expect(bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::printf("TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
  std::printf("TEST:PASS %s\n", name);
}
} // namespace

int main() {
  constexpr uint8_t instance_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                      0x54, 0x65, 0x73, 0x74, 0x31};
  constexpr uint8_t package_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                     0x54, 0x65, 0x73, 0x74};
  constexpr std::array<uint8_t, 32> expected = {
      0xe3, 0x1e, 0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c,
      0x67, 0x54, 0x65, 0x73, 0x74, 0x31, 0xcc, 0x09,
      0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73,
      0x74, 0x9f, 0x70, 0x01, 0x07, 0xc5, 0x01, 0x00};
  std::array<uint8_t, 64> output{};
  size_t output_length = 99;
  expect(gp::append_registry_entry(output.data(), output.size(), output_length,
                                   instance_aid, sizeof(instance_aid),
                                   package_aid, sizeof(package_aid), 0x07),
         "build_entry");
  expect(output_length == expected.size(), "exact_encoded_length");
  expect(std::memcmp(output.data(), expected.data(), expected.size()) == 0,
         "exact_tlv_bytes");
  expect(output[1] == output_length - 2, "outer_length_matches_payload");

  output_length = 99;
  expect(!gp::append_registry_entry(output.data(), expected.size() - 1,
                                    output_length, instance_aid,
                                    sizeof(instance_aid), package_aid,
                                    sizeof(package_aid), 0x07),
         "reject_short_capacity");
  expect(output_length == 0, "clear_length_on_failure");

  output_length = 99;
  expect(!gp::append_registry_entry(output.data(), output.size(), output_length,
                                    instance_aid, 4, package_aid,
                                    sizeof(package_aid), 0x07),
         "reject_short_instance_aid");
  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
