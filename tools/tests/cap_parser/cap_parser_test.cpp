#include "jc_handlers/jc_cap.hpp"
#include "exceptions.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace {

unsigned checks = 0;

void expect(bool condition, const char *label) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", label);
    std::exit(1);
  }
}

bool parses(const uint8_t *bytes, uint16_t length) {
  try {
    const jcvm::JC_Cap parsed(length, bytes);
    (void)parsed;
    return true;
  } catch (jcvm::Exceptions) {
    return false;
  }
}

bool parses_with_payload_validation(const uint8_t *bytes, uint16_t length,
                                    const bool validate_payloads) {
  try {
    const jcvm::JC_Cap parsed(length, bytes, validate_payloads);
    (void)parsed;
    return true;
  } catch (jcvm::Exceptions) {
    return false;
  }
}

void test_trusted_rom_payload_validation_boundary() {
  // This Descriptor has a valid frame but an invalid serialized type nibble.
  // ROM-mask payloads may be prelinked native structures, so only this payload
  // check is bypassed for that trusted path.
  const std::array<uint8_t, 8> malformed_descriptor = {
      11, 0, 5, 0, 0, 0, 1, 0x90};
  expect(!parses(malformed_descriptor.data(), malformed_descriptor.size()),
         "dynamic_malformed_descriptor_rejected");
  expect(parses_with_payload_validation(malformed_descriptor.data(),
                                       malformed_descriptor.size(), false),
         "rom_payload_bypass_accepts_prelinked_payload");

  const std::array<uint8_t, 7> truncated_frame = {11, 0, 5, 0, 0, 0, 1};
  expect(!parses_with_payload_validation(truncated_frame.data(),
                                         truncated_frame.size(), false),
         "rom_payload_bypass_keeps_frame_bounds");

  const std::array<uint8_t, 8> duplicate_frame = {7, 0, 1, 0, 7, 0, 1, 0};
  expect(!parses_with_payload_validation(duplicate_frame.data(),
                                         duplicate_frame.size(), false),
         "rom_payload_bypass_keeps_duplicate_component_check");
}

void expect_payload_mutation_rejected(const std::vector<uint8_t> &cap,
                                      const size_t component_offset,
                                      const size_t payload_offset,
                                      const uint8_t value) {
  std::vector<uint8_t> mutated = cap;
  mutated[component_offset + 3 + payload_offset] = value;
  expect(mutated[component_offset + 3 + payload_offset] !=
             cap[component_offset + 3 + payload_offset],
         "payload_mutation_changes_byte");
  ++checks;
  if (parses(mutated.data(), static_cast<uint16_t>(mutated.size()))) {
    std::fprintf(stderr,
                 "TEST:FAIL malformed_payload_accepted tag=%u offset=%zu "
                 "value=%u component_size=%u\n",
                 cap[component_offset], payload_offset,
                 mutated[component_offset + 3 + payload_offset],
                 static_cast<unsigned>((cap[component_offset + 1] << 8) |
                                       cap[component_offset + 2]));
    std::exit(1);
  }
}

void expect_descriptor_counts_rejected(const std::vector<uint8_t> &cap,
                                       const size_t component_offset) {
  const size_t payload_size =
      (static_cast<size_t>(cap[component_offset + 1]) << 8) |
      cap[component_offset + 2];
  const size_t payload = component_offset + 3;
  if (payload_size < 10 || cap[payload] == 0) return;

  for (const size_t count_offset : {size_t{5}, size_t{6}, size_t{8}}) {
    if (count_offset == 5 && payload_size >= 10 + 255 * 2) continue;
    std::vector<uint8_t> mutated = cap;
    mutated[payload + count_offset] = 0xff;
    if (count_offset != 4) mutated[payload + count_offset + 1] = 0xff;
    expect(!parses(mutated.data(), static_cast<uint16_t>(mutated.size())),
           "descriptor_count_exceeding_component_rejected");
  }
}

void test_compact_descriptor_counts() {
  std::array<uint8_t, 15> descriptor = {11, 0, 12, 1};
  expect(parses(descriptor.data(), descriptor.size()),
         "empty_compact_descriptor_parses");
  for (const size_t count_offset : {size_t{5}, size_t{6}, size_t{8}}) {
    auto mutated = descriptor;
    mutated[3 + count_offset] = 1;
    expect(!parses(mutated.data(), mutated.size()),
           "compact_descriptor_count_overrun_rejected");
  }
}

void test_class_component_21() {
  const std::array<uint8_t, 14> class_component = {
      6, 0, 11, 0x80, 0, 0xff, 0xff, 0, 0xff, 0, 0, 0, 0, 0};
  expect(parses(class_component.data(), class_component.size()),
         "minimal_class_component_parses");

  auto bad_interface_count = class_component;
  bad_interface_count[3] = 0x8f;
  expect(!parses(bad_interface_count.data(), bad_interface_count.size()),
         "class_component_superinterface_count_overrun_rejected");

  auto bad_public_method_count = class_component;
  bad_public_method_count[11] = 1;
  expect(!parses(bad_public_method_count.data(),
                 bad_public_method_count.size()),
         "class_component_public_method_count_overrun_rejected");

  auto bad_package_method_count = class_component;
  bad_package_method_count[13] = 1;
  expect(!parses(bad_package_method_count.data(), bad_package_method_count.size()),
         "class_component_package_method_count_overrun_rejected");

  auto bad_implemented_interface_count = class_component;
  bad_implemented_interface_count[4] = 0x0f;
  expect(!parses(bad_implemented_interface_count.data(),
                 bad_implemented_interface_count.size()),
         "class_component_implemented_interface_count_overrun_rejected");

  const std::array<uint8_t, 17> implemented_interface = {
      6, 0, 14, 0x80, 0x01, 0xff, 0xff, 0, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
  expect(parses(implemented_interface.data(), implemented_interface.size()),
         "class_component_implemented_interface_parses");
  auto bad_method_map_count = implemented_interface;
  bad_method_map_count[16] = 0xff;
  expect(!parses(bad_method_map_count.data(), bad_method_map_count.size()),
         "class_component_interface_method_map_overrun_rejected");

  const std::array<uint8_t, 28> matching_descriptor = {
      6, 0, 10, 0, 0xff, 0xff, 0, 0xff, 0, 0, 0, 0, 0,
      11, 0, 12, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  expect(parses(matching_descriptor.data(), matching_descriptor.size()),
         "descriptor_class_reference_boundary_parses");
  auto invalid_class_reference = matching_descriptor;
  invalid_class_reference[20] = 1;
  expect(!parses(invalid_class_reference.data(),
                 invalid_class_reference.size()),
         "descriptor_class_reference_not_at_record_boundary_rejected");
}

void test_type_descriptor_grammar() {
  const auto descriptor_component = [](const std::vector<uint8_t> &type) {
    std::vector<uint8_t> cap = {11, 0,
                                static_cast<uint8_t>(3 + type.size()),
                                0, 0, 0};
    cap.insert(cap.end(), type.begin(), type.end());
    return cap;
  };
  const auto check = [&descriptor_component](const std::vector<uint8_t> &type,
                                             const bool expected,
                                             const char *label) {
    const std::vector<uint8_t> cap = descriptor_component(type);
    expect(parses(cap.data(), static_cast<uint16_t>(cap.size())) == expected,
           label);
  };

  check({1, 0x20}, true, "primitive_type_descriptor_parses");
  check({1, 0xb0}, true, "primitive_array_descriptor_parses");
  check({5, 0x60, 0x00, 0x00}, true,
        "class_reference_type_descriptor_parses");
  check({1, 0x10}, true, "void_method_return_descriptor_parses");
  check({6, 0x60, 0x00, 0x04}, true,
        "method_parameter_and_return_descriptor_parses");

  const auto invalid_code = descriptor_component({1, 0x90});
  expect(!parses(invalid_code.data(), invalid_code.size()),
         "reserved_type_nibble_rejected");
  const auto missing_class_ref = descriptor_component({4, 0x60, 0x00});
  expect(!parses(missing_class_ref.data(), missing_class_ref.size()),
         "truncated_type_class_reference_rejected");
  const auto bad_padding = descriptor_component({1, 0x21});
  expect(!parses(bad_padding.data(), bad_padding.size()),
         "nonzero_type_descriptor_padding_rejected");
  const auto misplaced_void = descriptor_component({2, 0x12});
  expect(!parses(misplaced_void.data(), misplaced_void.size()),
         "void_parameter_type_rejected");
}

void test_type_descriptor_offsets() {
  const auto wrap_descriptor = [](std::vector<uint8_t> payload) {
    std::vector<uint8_t> cap = {
        11, static_cast<uint8_t>(payload.size() >> 8),
        static_cast<uint8_t>(payload.size())};
    cap.insert(cap.end(), payload.begin(), payload.end());
    return cap;
  };

  std::vector<uint8_t> method_payload(26, 0);
  method_payload[0] = 1;
  method_payload[9] = 1;  // One method descriptor.
  method_payload[15] = 2; // Offset from the type_descriptor_info start.
  method_payload[24] = 1; // Return type: void.
  method_payload[25] = 0x10;
  auto valid_method = wrap_descriptor(method_payload);
  expect(parses(valid_method.data(), valid_method.size()),
         "method_type_offset_at_record_start_parses");
  method_payload[15] = 0; // Also accept offsets based at constant_pool_types.
  auto valid_method_array_base = wrap_descriptor(method_payload);
  expect(parses(valid_method_array_base.data(), valid_method_array_base.size()),
         "method_type_offset_at_array_base_parses");
  method_payload[15] = 1;
  auto invalid_method = wrap_descriptor(method_payload);
  expect(!parses(invalid_method.data(), invalid_method.size()),
         "method_type_offset_inside_record_rejected");

  std::vector<uint8_t> cp_payload(28, 0);
  cp_payload[0] = 1;
  cp_payload[9] = 1;
  cp_payload[15] = 4;
  cp_payload[23] = 1; // One constant-pool type offset.
  cp_payload[25] = 4; // Offset from the type_descriptor_info start.
  cp_payload[26] = 1;
  cp_payload[27] = 0x10;
  auto valid_cp_type = wrap_descriptor(cp_payload);
  expect(parses(valid_cp_type.data(), valid_cp_type.size()),
         "constant_pool_type_offset_at_record_start_parses");
  cp_payload[15] = 2;
  cp_payload[25] = 2; // Also accept offsets based at constant_pool_types.
  auto valid_cp_array_base = wrap_descriptor(cp_payload);
  expect(parses(valid_cp_array_base.data(), valid_cp_array_base.size()),
         "constant_pool_type_offset_at_array_base_parses");
  cp_payload[15] = 4;
  cp_payload[25] = 3;
  auto invalid_cp_type = wrap_descriptor(cp_payload);
  expect(!parses(invalid_cp_type.data(), invalid_cp_type.size()),
         "constant_pool_type_offset_inside_record_rejected");

  std::vector<uint8_t> field_payload(21, 0);
  field_payload[0] = 1;
  field_payload[7] = 1; // One field descriptor.
  field_payload[16] = 2; // Offset from the type_descriptor_info start.
  field_payload[19] = 1;
  field_payload[20] = 0x20;
  auto valid_field = wrap_descriptor(field_payload);
  expect(parses(valid_field.data(), valid_field.size()),
         "field_type_offset_at_record_start_parses");
  field_payload[16] = 0; // Also accept offsets based at constant_pool_types.
  auto valid_field_array_base = wrap_descriptor(field_payload);
  expect(parses(valid_field_array_base.data(), valid_field_array_base.size()),
         "field_type_offset_at_array_base_parses");
  field_payload[16] = 1;
  auto invalid_field = wrap_descriptor(field_payload);
  expect(!parses(invalid_field.data(), invalid_field.size()),
         "field_type_offset_inside_record_rejected");

  auto shifted_base_descriptor = wrap_descriptor({
      0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
      0x00, 0x81, 0x00, 0x01, 0x00, 0x04, 0x00, 0x05, 0x00, 0x00,
      0x00, 0x00, 0x01, 0x09, 0x00, 0x08, 0x00, 0x04, 0x00, 0x05,
      0x00, 0x00, 0x00, 0x00, 0x02, 0x09, 0x00, 0x0f, 0x00, 0x04,
      0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x04,
      0x00, 0x06, 0x01, 0x10, 0x05, 0x68, 0x10, 0xb0});
  expect(parses(shifted_base_descriptor.data(), shifted_base_descriptor.size()),
         "shifted_constant_pool_type_base_parses");
}

void test_class_component_22() {
  const auto append_component = [](std::vector<uint8_t> &cap, uint8_t tag,
                                   const std::vector<uint8_t> &payload) {
    cap.push_back(tag);
    cap.push_back(static_cast<uint8_t>(payload.size() >> 8));
    cap.push_back(static_cast<uint8_t>(payload.size()));
    cap.insert(cap.end(), payload.begin(), payload.end());
  };
  const auto make_cap = [&](std::vector<uint8_t> class_payload) {
    std::vector<uint8_t> cap;
    std::vector<uint8_t> header(16, 0);
    header[4] = 2; // CAP format minor version.
    header[5] = 2; // CAP format major version.
    header[9] = 5; // Package AID length.
    header[10] = 0xf0;
    header[11] = 0x00;
    header[12] = 0x00;
    header[13] = 0x00;
    header[14] = 0x01;
    header[15] = 0; // Empty CAP 2.2 package name.
    append_component(cap, 1, header);
    append_component(cap, 6, class_payload);
    return cap;
  };

  std::vector<uint8_t> class_payload = {
      0x00, 0x02, 0x01, 0x10, // Signature pool: one ()V descriptor.
      0x80,                   // Non-remote interface.
      0xa0, 0x01, 'I',        // Remote interface with a one-byte name.
      0x00, 0xff, 0xff, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x20, 0xff, 0xff, 0x01, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x01, 'C', 0x00};
  auto valid = make_cap(class_payload);
  expect(parses(valid.data(), valid.size()),
         "class_component_22_signature_and_remote_records_parse");

  class_payload[3] = 0x00;
  auto invalid_signature = make_cap(class_payload);
  expect(!parses(invalid_signature.data(), invalid_signature.size()),
         "class_component_22_invalid_signature_rejected");

  class_payload[3] = 0x10;
  class_payload[6] = 0x05; // Remote interface name exceeds remaining bytes.
  auto truncated_remote_interface = make_cap(class_payload);
  expect(!parses(truncated_remote_interface.data(),
                 truncated_remote_interface.size()),
         "class_component_22_truncated_remote_interface_rejected");

  class_payload[6] = 0x01;
  class_payload.pop_back(); // Truncate remote class metadata.
  auto truncated_remote_class = make_cap(class_payload);
  expect(!parses(truncated_remote_class.data(), truncated_remote_class.size()),
         "class_component_22_truncated_remote_class_rejected");
}

} // namespace

int main(int argc, char **argv) {
  expect(parses(nullptr, 0), "empty_null_input");
  expect(!parses(nullptr, 1), "null_nonempty_input_rejected");

  const std::array<uint8_t, 4> framed_method = {7, 0, 1, 0};
  expect(parses(framed_method.data(), framed_method.size()),
         "complete_component_frame");
  for (uint16_t prefix = 1; prefix < framed_method.size(); ++prefix) {
    expect(!parses(framed_method.data(), prefix),
           "every_truncated_component_prefix_rejected");
  }

  const std::array<uint8_t, 4> oversized_frame = {7, 0xff, 0xff, 0};
  expect(!parses(oversized_frame.data(), oversized_frame.size()),
         "oversized_component_rejected");

  const std::array<uint8_t, 8> duplicate_method = {7, 0, 1, 0,
                                                   7, 0, 1, 0};
  expect(!parses(duplicate_method.data(), duplicate_method.size()),
         "duplicate_component_rejected");
  test_trusted_rom_payload_validation_boundary();
  test_compact_descriptor_counts();
  test_class_component_21();
  test_type_descriptor_grammar();
  test_type_descriptor_offsets();
  test_class_component_22();

  std::mt19937 generator(0x4a43564d);
  std::uniform_int_distribution<unsigned> byte_distribution(0, 255);
  std::uniform_int_distribution<unsigned> length_distribution(0, 128);
  constexpr unsigned iterations = 100000;
  for (unsigned iteration = 0; iteration < iterations; ++iteration) {
    const uint16_t length =
        static_cast<uint16_t>(length_distribution(generator));
    std::vector<uint8_t> input(length);
    for (uint8_t &value : input) {
      value = static_cast<uint8_t>(byte_distribution(generator));
    }
    (void)parses(input.empty() ? nullptr : input.data(), length);
  }

  unsigned corpus_files = 0;
  for (int argument = 1; argument < argc; ++argument) {
    std::ifstream file(argv[argument], std::ios::binary);
    expect(file.good(), "corpus_file_open");
    std::vector<uint8_t> cap((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    expect(!cap.empty() && cap.size() <= UINT16_MAX,
           "corpus_file_length_valid");
    if (!parses(cap.data(), static_cast<uint16_t>(cap.size()))) {
      std::fprintf(stderr, "TEST:FAIL upstream_cap_parses path=%s\n",
                   argv[argument]);
      return 1;
    }
    ++checks;
    ++corpus_files;

    size_t offset = 0;
    while (offset < cap.size()) {
      expect(cap.size() - offset >= 3, "corpus_component_header_present");
      const size_t payload_size =
          (static_cast<size_t>(cap[offset + 1]) << 8) | cap[offset + 2];
      const size_t frame_size = 3 + payload_size;
      expect(frame_size <= cap.size() - offset,
             "corpus_component_frame_fits");

      std::vector<uint8_t> oversized = cap;
      oversized[offset + 1] = 0xff;
      oversized[offset + 2] = 0xff;
      expect(!parses(oversized.data(), static_cast<uint16_t>(oversized.size())),
             "mutated_component_length_rejected");
      expect(!parses(cap.data(), static_cast<uint16_t>(offset + 1)),
             "truncated_component_tag_rejected");
      expect(!parses(cap.data(), static_cast<uint16_t>(offset + 2)),
             "truncated_component_length_rejected");
      if (payload_size != 0) {
        expect(!parses(cap.data(), static_cast<uint16_t>(offset + frame_size - 1)),
               "truncated_component_payload_rejected");
      }
      switch (cap[offset]) {
      case 1:
        expect_payload_mutation_rejected(cap, offset, 9, 0xff);
        break;
      case 2:
        expect_payload_mutation_rejected(cap, offset, 30, 0xff);
        break;
      case 3:
      case 4:
      case 10:
        expect_payload_mutation_rejected(cap, offset, 0, 0xff);
        break;
      case 5:
        expect_payload_mutation_rejected(cap, offset, 0, 0xff);
        break;
      case 7: {
        const size_t impossible_count = (payload_size - 1) / 8 + 1;
        if (impossible_count <= 255) {
          expect_payload_mutation_rejected(
              cap, offset, 0, static_cast<uint8_t>(impossible_count));
        }
        break;
      }
      case 8:
        expect_payload_mutation_rejected(cap, offset, 4, 0xff);
        break;
      case 9:
        expect_payload_mutation_rejected(cap, offset, 0, 0xff);
        break;
      case 11:
        expect_descriptor_counts_rejected(cap, offset);
        break;
      default:
        break;
      }
      offset += frame_size;
    }
  }

  std::printf(
      "TEST:PASS cap_parser_framing checks=%u fuzz_inputs=%u corpus_files=%u\n",
      checks, iterations, corpus_files);
  return 0;
}
