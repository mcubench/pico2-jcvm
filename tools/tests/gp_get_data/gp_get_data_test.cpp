#include "gp_get_data.hpp"

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
  constexpr std::array<uint8_t, 26> expected = {
      0x66, 0x18, 0x73, 0x16, 0x06, 0x07, 0x2a, 0x86, 0x48,
      0x86, 0xfc, 0x6b, 0x01, 0x64, 0x0b, 0x06, 0x09, 0x2a,
      0x86, 0x48, 0x86, 0xfc, 0x6b, 0x04, 0x03, 0x10};
  constexpr std::array<uint8_t, 45> expected_cplc = {
      0x9f, 0x7f, 0x2a,
      0x09, 0x6e, 0x23, 0x50, 0x4a, 0x43, 0x63, 0x04, 0x00, 0x01,
      0x63, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x6e,
      0x63, 0x04, 0x09, 0x6e, 0x63, 0x04, 0x09, 0x6e, 0x63, 0x04,
      0x00, 0x00, 0x00, 0x00, 0x09, 0x6e, 0x63, 0x04,
      0x00, 0x00, 0x00, 0x00};
  const uint8_t *data = nullptr;
  size_t length = 0;

  expect(gp::get_data_object(0x00, 0x66, data, length),
         "recognize_card_data_tag");
  expect(data == gp::kCardRecognitionData && length == expected.size(),
         "return_card_data_object_length");
  expect(std::memcmp(data, expected.data(), expected.size()) == 0,
         "card_recognition_data_exact_tlv");

  std::array<uint8_t, 64> response{};
  size_t response_length = 0;
  expect(gp::write_get_data_response(0x00, 0x66, response.data(),
                                     response.size(), response_length),
         "encode_card_data_response");
  expect(response_length == expected.size() + 2 &&
             std::memcmp(response.data(), expected.data(), expected.size()) == 0 &&
             response[response_length - 2] == 0x90 &&
             response[response_length - 1] == 0x00,
         "card_data_response_has_success_status");
  expect(gp::write_get_data_response(0x00, 0x66, response.data(),
                                     response.size(), response_length, false),
         "encode_iso_card_data_value");
  expect(response_length == expected.size() &&
             std::memcmp(response.data(), expected.data() + 2,
                         expected.size() - 2) == 0 &&
             response[response_length - 2] == 0x90 &&
             response[response_length - 1] == 0x00,
         "iso_card_data_response_has_value_and_success_status");

  expect(gp::get_data_object(0x9f, 0x7f, data, length),
         "recognize_cplc_tag");
  expect(data == gp::kCplcData && length == expected_cplc.size(),
         "return_cplc_length");
  expect(std::memcmp(data, expected_cplc.data(), expected_cplc.size()) == 0,
         "cplc_exact_tlv_fields_and_dates");
  expect(gp::write_get_data_response(0x9f, 0x7f, response.data(),
                                     response.size(), response_length),
         "encode_cplc_response");
  expect(response_length == expected_cplc.size() + 2 &&
             std::memcmp(response.data(), expected_cplc.data(),
                         expected_cplc.size()) == 0 &&
             response[response_length - 2] == 0x90 &&
             response[response_length - 1] == 0x00,
         "cplc_response_has_success_status");
  expect(gp::write_get_data_response(0x9f, 0x7f, response.data(),
                                     response.size(), response_length, false),
         "encode_iso_cplc_value");
  expect(response_length == expected_cplc.size() - 1 &&
             std::memcmp(response.data(), expected_cplc.data() + 3,
                         expected_cplc.size() - 3) == 0 &&
             response[response_length - 2] == 0x90 &&
             response[response_length - 1] == 0x00,
         "iso_cplc_response_has_value_and_success_status");
  expect(!gp::get_data_object(0x00, 0x65, data, length),
         "reject_unknown_data_tag");
  expect(data == nullptr && length == 0,
         "clear_output_for_unknown_tag");

  expect(gp::write_get_data_response(0x00, 0x65, response.data(),
                                     response.size(), response_length) &&
             response_length == 2 && response[0] == 0x6a &&
             response[1] == 0x88,
         "unknown_data_tag_returns_not_found");
  expect(!gp::write_get_data_response(0x00, 0x66, response.data(), 2,
                                      response_length),
         "reject_short_response_capacity");

  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
