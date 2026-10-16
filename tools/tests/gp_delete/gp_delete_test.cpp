#include "gp_delete.hpp"

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
  constexpr std::array<uint8_t, 12> instance = {
      0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31};
  constexpr std::array<uint8_t, 11> package = {
      0x4f, 0x09, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74};
  gp::DeleteRequest request;

  expect(gp::parse_delete_request(0x00, 0x00, instance.data(), instance.size(),
                                  request),
         "parse_instance_aid");
  expect(request.aid_length == 10 &&
             std::memcmp(request.aid, instance.data() + 2, 10) == 0 &&
             !request.delete_related_objects,
         "preserve_instance_target_and_flags");
  expect(gp::parse_delete_request(0x00, 0x80, package.data(), package.size(),
                                  request) &&
             request.aid_length == 9 && request.delete_related_objects,
         "parse_package_delete_related_flag");

  expect(!gp::parse_delete_request(0x80, 0x00, instance.data(), instance.size(),
                                   request),
         "reject_chained_p1");
  expect(!gp::parse_delete_request(0x00, 0x01, instance.data(), instance.size(),
                                   request),
         "reject_reserved_p2_bits");
  expect(!gp::parse_delete_request(0x00, 0x00, nullptr, 0, request),
         "reject_missing_data");

  constexpr std::array<uint8_t, 7> short_aid = {0x4f, 0x05, 1, 2, 3, 4, 5};
  expect(gp::parse_delete_request(0x00, 0x00, short_aid.data(),
                                  short_aid.size(), request),
         "accept_minimum_aid_length");
  constexpr std::array<uint8_t, 6> undersized_aid = {0x4f, 0x04, 1, 2, 3, 4};
  expect(!gp::parse_delete_request(0x00, 0x00, undersized_aid.data(),
                                   undersized_aid.size(), request),
         "reject_short_aid");

  constexpr std::array<uint8_t, 13> trailing_data = {
      0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74,
      0x31, 0x00};
  expect(!gp::parse_delete_request(0x00, 0x00, trailing_data.data(),
                                   trailing_data.size(), request),
         "reject_trailing_bytes");
  constexpr std::array<uint8_t, 12> truncated = {
      0x4f, 0x0b, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31};
  expect(!gp::parse_delete_request(0x00, 0x00, truncated.data(),
                                   truncated.size(), request),
         "reject_truncated_tlv");
  constexpr std::array<uint8_t, 12> nonminimal_length = {
      0x4f, 0x81, 0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74};
  expect(!gp::parse_delete_request(0x00, 0x00, nonminimal_length.data(),
                                   nonminimal_length.size(), request),
         "reject_nonminimal_length");

  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
