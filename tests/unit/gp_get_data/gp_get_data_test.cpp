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

  constexpr std::array<uint8_t, 10> applet_aid = {
      0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31};
  constexpr std::array<uint8_t, 8> second_aid = {
      0xa0, 0x00, 0x00, 0x15, 0x10, 0x00, 0x00, 0x01};
  constexpr std::array<uint8_t, 2> application_tag_list = {0x5c, 0x00};
  constexpr std::array<uint8_t, 2> malformed_tag_list = {0x5c, 0x01};
  expect(gp::valid_get_applications_tag_list(application_tag_list.data(),
                                             application_tag_list.size()),
         "accept_required_application_tag_list");
  expect(!gp::valid_get_applications_tag_list(nullptr, 0) &&
             !gp::valid_get_applications_tag_list(application_tag_list.data(),
                                                  1) &&
             !gp::valid_get_applications_tag_list(
                 malformed_tag_list.data(), malformed_tag_list.size()),
         "reject_missing_or_malformed_application_tag_list");
  constexpr std::array<uint8_t, 40> expected_applications = {
      0x61, 0x0a, 0x4f, 0x08, 0xa0, 0x00, 0x00, 0x01,
      0x51, 0x00, 0x00, 0x00,
      0x61, 0x0c, 0x4f, 0x0a, 0x4a, 0x43, 0x41, 0x6c,
      0x67, 0x54, 0x65, 0x73, 0x74, 0x31,
      0x61, 0x0a, 0x4f, 0x08, 0xa0, 0x00, 0x00, 0x15,
      0x10, 0x00, 0x00, 0x01,
      0x90, 0x00};
  auto enumerate_two_apps = [&](auto append) {
    return append(applet_aid.data(), applet_aid.size()) &&
           append(second_aid.data(), second_aid.size());
  };
  expect(gp::write_get_applications_response(
             response.data(), response.size(), response_length,
             enumerate_two_apps),
         "encode_isd_and_multiple_application_templates");
  expect(response_length == expected_applications.size() &&
             std::memcmp(response.data(), expected_applications.data(),
                         expected_applications.size()) == 0,
         "application_list_matches_template_order_and_status");
  expect(gp::write_get_applications_response(
             response.data(), response.size(), response_length,
             [](auto) { return true; }) &&
             response_length == 14 && response[0] == 0x61 &&
             response[1] == 0x0a && response[12] == 0x90 &&
             response[13] == 0x00,
         "empty_application_registry_returns_isd_template");
  expect(!gp::write_get_applications_response(
             response.data(), 14, response_length, enumerate_two_apps) &&
             response_length == 0,
         "application_list_capacity_failure_is_bounded");
  expect(!gp::write_get_applications_response(
             response.data(), response.size(), response_length,
             [](auto append) {
               constexpr uint8_t invalid_aid[] = {0x01, 0x02, 0x03, 0x04};
               return append(invalid_aid, sizeof(invalid_aid));
             }) &&
             response_length == 0,
         "reject_invalid_application_aid");

  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
