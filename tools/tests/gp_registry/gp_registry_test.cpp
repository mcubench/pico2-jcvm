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
      0x67, 0x54, 0x65, 0x73, 0x74, 0x31, 0xc4, 0x09,
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

  constexpr std::array<uint8_t, 19> isd_entry = {
      0xe3, 0x11, 0x4f, 0x08, 0xa0, 0x00, 0x00, 0x01, 0x51, 0x00,
      0x00, 0x00, 0x9f, 0x70, 0x01, 0x01, 0xc5, 0x01, 0x00};
  output_length = 0;
  expect(gp::append_isd_registry_entry(output.data(), output.size(),
                                       output_length),
         "build_isd_entry");
  expect(output_length == isd_entry.size(), "isd_exact_length");
  expect(std::memcmp(output.data(), isd_entry.data(), isd_entry.size()) == 0,
         "isd_exact_bytes");
  output_length = 99;
  expect(!gp::append_isd_registry_entry(output.data(), isd_entry.size() - 1,
                                        output_length),
         "reject_short_isd_capacity");
  expect(output_length == 0, "clear_isd_length_on_failure");

  const uint8_t wildcard_search[] = {0x4f, 0x00};
  gp::AidSearch search;
  expect(gp::parse_aid_search(wildcard_search, sizeof(wildcard_search), search),
         "parse_wildcard_aid_search");
  expect(gp::aid_matches_search(search, instance_aid, sizeof(instance_aid)),
         "wildcard_matches_applet");
  const uint8_t search_with_ignored_criteria[] = {
      0x4f, 0x00, 0x5c, 0x01, 0x9f};
  expect(gp::parse_aid_search(search_with_ignored_criteria,
                              sizeof(search_with_ignored_criteria), search),
         "ignore_additional_well_formed_criteria");
  expect(!gp::parse_aid_search(search_with_ignored_criteria + 2,
                               sizeof(search_with_ignored_criteria) - 2,
                               search),
         "require_aid_search_first");

  const uint8_t rid_search[] = {0x4f, 0x05, 0x4a, 0x43, 0x41, 0x6c, 0x67};
  expect(gp::parse_aid_search(rid_search, sizeof(rid_search), search),
         "parse_rid_search");
  expect(gp::aid_matches_search(search, instance_aid, sizeof(instance_aid)),
         "rid_matches_applet_aid");
  constexpr uint8_t other_rid_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x51, 0x00};
  expect(!gp::aid_matches_search(search, other_rid_aid,
                                 sizeof(other_rid_aid)),
         "rid_rejects_mismatch");

  const uint8_t malformed_search[] = {0x4f, 0x03, 0x4a, 0x43, 0x41};
  expect(!gp::parse_aid_search(malformed_search, sizeof(malformed_search),
                               search),
         "reject_invalid_aid_search_length");
  expect(!gp::parse_aid_search(nullptr, 0, search), "reject_missing_search");

  constexpr std::array<uint8_t, 17> load_file_entry = {
      0xe3, 0x0f, 0x4f, 0x09, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54,
      0x65, 0x73, 0x74, 0x9f, 0x70, 0x01, 0x01};
  output_length = 0;
  expect(gp::append_load_file_entry(output.data(), output.size(), output_length,
                                    package_aid, sizeof(package_aid), nullptr,
                                    0, false),
         "build_load_file_entry");
  expect(output_length == load_file_entry.size(), "load_file_exact_length");
  expect(std::memcmp(output.data(), load_file_entry.data(),
                     load_file_entry.size()) == 0,
         "load_file_exact_bytes");

  output_length = 0;
  expect(gp::append_load_file_entry(output.data(), output.size(), output_length,
                                    package_aid, sizeof(package_aid),
                                    instance_aid, sizeof(instance_aid), true),
         "build_load_file_with_module");
  expect(output_length == 29 && output[1] == output_length - 2 &&
             output[output_length - sizeof(instance_aid) - 2] == 0x84,
         "load_file_module_tlv");

  const gp::ExecutableModuleAid modules[] = {
      {static_cast<uint8_t>(sizeof(instance_aid)),
       {0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31}},
      {static_cast<uint8_t>(sizeof(instance_aid)),
       {0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x32}},
  };
  constexpr std::array<uint8_t, 41> expected_multiple_modules = {
      0xe3, 0x27, 0x4f, 0x09, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54,
      0x65, 0x73, 0x74, 0x9f, 0x70, 0x01, 0x01, 0x84, 0x0a, 0x4a,
      0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74, 0x31, 0x84,
      0x0a, 0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x65, 0x73, 0x74,
      0x32};
  output_length = 0;
  expect(gp::append_load_file_entry_modules(
             output.data(), output.size(), output_length, package_aid,
             sizeof(package_aid), modules, 2),
         "build_single_load_file_entry_with_multiple_modules");
  expect(output_length == expected_multiple_modules.size() &&
             std::memcmp(output.data(), expected_multiple_modules.data(),
                         expected_multiple_modules.size()) == 0,
         "module_aids_are_repeated_tag84_in_one_entry");

  gp::ExecutableModuleAid long_modules[7]{};
  for (size_t index = 0; index < 7; ++index) {
    long_modules[index].length = 16;
  }
  std::array<uint8_t, 160> long_output{};
  output_length = 0;
  expect(gp::append_load_file_entry_modules(
             long_output.data(), long_output.size(), output_length, package_aid,
             sizeof(package_aid), long_modules, 7) &&
             long_output[0] == 0xe3 && long_output[1] == 0x81 &&
             long_output[2] == 141,
         "module_list_uses_ber_long_length_when_needed");

  gp::RegistryView registry{};
  registry.applet_installed = true;
  registry.instance_aid = instance_aid;
  registry.instance_aid_length = sizeof(instance_aid);
  registry.package_loaded = true;
  registry.package_aid = package_aid;
  registry.package_aid_length = sizeof(package_aid);
  registry.module_aid = instance_aid;
  registry.module_aid_length = sizeof(instance_aid);

  const uint8_t malformed_isd_search[] = {0x4f, 0x03, 0x01};
  output_length = 0;
  expect(gp::build_get_status_first_response(
             0x80, malformed_isd_search, sizeof(malformed_isd_search),
             registry, output.data(), output.size() - 2, output_length) ==
             0x9000,
         "isd_query_ignores_search_qualifier");
  expect(output_length == isd_entry.size() &&
             std::memcmp(output.data(), isd_entry.data(), isd_entry.size()) ==
                 0,
         "isd_query_returns_only_isd_entry");

  output_length = 0;
  expect(gp::build_get_status_first_response(
             0x40, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000,
         "application_status_query");
  expect(output_length == expected.size() &&
             std::memcmp(output.data(), expected.data(), expected.size()) ==
                 0,
         "application_status_entry_exact_bytes");

  output_length = 0;
  expect(gp::build_get_status_first_response(
             0x20, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000,
         "load_file_status_query");
  expect(output_length == load_file_entry.size() &&
             std::memcmp(output.data(), load_file_entry.data(),
                         load_file_entry.size()) == 0,
         "load_file_status_exact_bytes");

  output_length = 0;
  expect(gp::build_get_status_first_response(
             0x10, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000,
         "module_status_query");
  expect(output_length == 29 && output[1] == output_length - 2 &&
             output[output_length - sizeof(instance_aid) - 2] == 0x84,
         "module_status_entry_contains_module_aid");

  output_length = 99;
  expect(gp::build_get_status_first_response(
             0x40, nullptr, 0, registry, output.data(), output.size() - 2,
             output_length) == 0x6a80,
         "application_query_requires_aid_search");
  expect(output_length == 0, "application_query_error_has_empty_data");
  output_length = 99;
  expect(gp::build_get_status_first_response(
             0x08, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x6a86,
         "reject_unsupported_status_category");
  expect(output_length == 0, "unsupported_category_has_empty_data");

  registry.applet_installed = false;
  output_length = 99;
  expect(gp::build_get_status_first_response(
             0x40, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000 &&
             output_length == 0,
         "empty_application_registry_is_successful");

  output_length = 0;
  expect(gp::build_get_status_first_response(
             0x20, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000,
         "package_remains_after_instance_delete");
  expect(output_length == load_file_entry.size() &&
             std::memcmp(output.data(), load_file_entry.data(),
                         load_file_entry.size()) == 0,
         "package_status_bytes_after_instance_delete");

  registry.package_loaded = false;
  output_length = 99;
  expect(gp::build_get_status_first_response(
             0x20, wildcard_search, sizeof(wildcard_search), registry,
             output.data(), output.size() - 2, output_length) == 0x9000 &&
             output_length == 0,
         "package_registry_empty_after_package_delete");

  expect(gp::validate_get_status_next(false, 0x40, wildcard_search,
                                      sizeof(wildcard_search)) == 0x6985,
         "reject_next_without_first_query");
  expect(gp::validate_get_status_next(true, 0x80, malformed_isd_search,
                                      sizeof(malformed_isd_search)) == 0x6985,
         "reject_next_after_isd_only_query_and_ignore_search");
  expect(gp::validate_get_status_next(true, 0x40, wildcard_search,
                                      sizeof(wildcard_search)) == 0x9000,
         "allow_next_after_application_query");
  expect(gp::validate_get_status_next(true, 0x20, nullptr, 0) == 0x6a80,
         "next_query_requires_aid_search");
  expect(gp::validate_get_status_next(true, 0x08, wildcard_search,
                                      sizeof(wildcard_search)) == 0x6a86,
         "reject_next_with_unsupported_category");

  constexpr uint8_t exact_search[] = {0x4f, 0x05, 0x4a, 0x43, 0x41,
                                      0x6c, 0x67};
  constexpr uint8_t other_search[] = {0x4f, 0x05, 0xa0, 0x00, 0x00,
                                      0x01, 0x51};
  expect(gp::get_status_query_matches(
             0x40, exact_search, sizeof(exact_search), 0x40,
             exact_search, sizeof(exact_search)),
         "allow_next_for_exact_prior_query");
  expect(!gp::get_status_query_matches(
             0x40, exact_search, sizeof(exact_search), 0x20,
             exact_search, sizeof(exact_search)),
         "reject_next_with_changed_category");
  expect(!gp::get_status_query_matches(
             0x40, exact_search, sizeof(exact_search), 0x40,
             other_search, sizeof(other_search)),
         "reject_next_with_changed_aid_search");
  expect(!gp::get_status_query_matches(
             0x40, exact_search, sizeof(exact_search), 0x40, nullptr, 0),
         "reject_next_with_missing_search");

  std::printf("TEST:SUMMARY pass=%d fail=0\n", checks);
  return EXIT_SUCCESS;
}
