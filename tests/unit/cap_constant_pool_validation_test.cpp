#include "../../src/cap_constant_pool_validation.hpp"

#include <cstdio>

int main() {
  unsigned int passed = 0;
  unsigned int failed = 0;
  const auto check = [&passed, &failed](const bool condition,
                                        const char *name) {
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    condition ? ++passed : ++failed;
  };

  const uint8_t empty[] = {0, 0};
  check(jcvm::cap_constant_pool_validation::valid_payload(empty,
                                                          sizeof(empty)),
        "empty constant pool is valid");

  const uint8_t all_known_tags[] = {
      0, 6, 1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0,
      0, 4, 0, 0, 0, 5, 0, 0, 0, 6, 0, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_payload(
            all_known_tags, sizeof(all_known_tags)),
        "all six defined reference tags are accepted");

  const uint8_t unknown_tag[] = {0, 1, 7, 0, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_payload(unknown_tag,
                                                           sizeof(unknown_tag)),
        "unknown tag rejected");

  const uint8_t zero_tag[] = {0, 1, 0, 0, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_payload(zero_tag,
                                                           sizeof(zero_tag)),
        "zero tag rejected");

  const uint8_t truncated_entry[] = {0, 1, 1, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_payload(
            truncated_entry, sizeof(truncated_entry)),
        "truncated entry rejected");

  const uint8_t extra_entry[] = {0, 0, 1, 0, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_payload(extra_entry,
                                                           sizeof(extra_entry)),
        "unaccounted entry bytes rejected");

  check(!jcvm::cap_constant_pool_validation::valid_payload(nullptr, 2),
        "null payload rejected");

  const uint8_t external_records[] = {
      1, 0x81, 0, 0, 2, 0x81, 0, 0, 3, 0x81, 0, 0,
      4, 0x81, 0, 0, 5, 0x81, 0, 0, 6, 0x81, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_import_tokens(
            external_records, 6, 2),
        "all six external reference kinds accept declared import token");
  check(!jcvm::cap_constant_pool_validation::valid_import_tokens(
            external_records, 6, 1),
        "out-of-range external import token rejected");

  const uint8_t internal_record[] = {1, 0x7f, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_import_tokens(
            internal_record, 1, 0),
        "internal reference does not require an import entry");
  check(!jcvm::cap_constant_pool_validation::valid_import_tokens(nullptr, 1,
                                                                 0),
        "missing entry storage rejected");
  check(jcvm::cap_constant_pool_validation::valid_export_class_token(0, 1),
        "first exported class token accepted");
  check(!jcvm::cap_constant_pool_validation::valid_export_class_token(1, 1),
        "export token at class count rejected");
  check(!jcvm::cap_constant_pool_validation::valid_export_class_token(255,
                                                                      255),
        "maximum byte token rejected for 255 exports");
  check(jcvm::cap_constant_pool_validation::valid_export_member_token(0, 1),
        "first exported member token accepted");
  check(!jcvm::cap_constant_pool_validation::valid_export_member_token(2, 2),
        "member token at member count rejected");
  uint8_t class_record_starts[1u << 13] = {};
  class_record_starts[2 / 8] |= static_cast<uint8_t>(1u << (2 & 7));
  class_record_starts[7 / 8] |= static_cast<uint8_t>(1u << (7 & 7));
  const uint8_t class_component_payload[] = {0, 0, 0, 0, 0, 0, 0, 0xc0};
  const uint8_t export_class_at_record_start[] = {1, 0, 2, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_class_at_record_start,
            sizeof(export_class_at_record_start), class_record_starts,
            class_component_payload, sizeof(class_component_payload), false),
        "Export class offset at a Class record start accepted");
  const uint8_t export_class_inside_record[] = {1, 0, 3, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_class_inside_record, sizeof(export_class_inside_record),
            class_record_starts, class_component_payload,
            sizeof(class_component_payload), false),
        "Export class offset inside a Class record rejected");
  const uint8_t export_class_outside_component[] = {1, 0, 8, 0, 0};
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_class_outside_component,
            sizeof(export_class_outside_component), class_record_starts,
            class_component_payload, sizeof(class_component_payload), false),
        "Export class offset beyond the Class component rejected");
  const uint8_t export_class_without_classes[] = {0};
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_class_without_classes, sizeof(export_class_without_classes),
            class_record_starts, class_component_payload,
            sizeof(class_component_payload), false),
        "empty Export component rejected");
  const uint8_t export_interface_at_record_start[] = {1, 0, 7, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_interface_at_record_start,
            sizeof(export_interface_at_record_start), class_record_starts,
            class_component_payload, sizeof(class_component_payload), true),
        "applet Export accepts shareable interface with no static members");
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_class_at_record_start, sizeof(export_class_at_record_start),
            class_record_starts, class_component_payload,
            sizeof(class_component_payload), true),
        "applet Export rejects a class");
  const uint8_t export_interface_with_field[] = {1, 0, 7, 1, 0};
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_interface_with_field, sizeof(export_interface_with_field),
            class_record_starts, class_component_payload,
            sizeof(class_component_payload), false),
        "interface Export rejects static fields");
  const uint8_t nonshareable_class_payload[] = {0, 0, 0, 0, 0, 0, 0, 0x80};
  check(!jcvm::cap_constant_pool_validation::valid_export_class_offsets(
            export_interface_at_record_start,
            sizeof(export_interface_at_record_start), class_record_starts,
            nonshareable_class_payload, sizeof(nonshareable_class_payload),
            true),
        "applet Export rejects non-shareable interface");
  check(jcvm::cap_constant_pool_validation::valid_instance_field_token(
            2, 5, 3, 2),
        "instance field token within declared layout accepted");
  check(!jcvm::cap_constant_pool_validation::valid_instance_field_token(
            5, 5, 3, 2),
        "instance field token at declared size rejected");
  check(!jcvm::cap_constant_pool_validation::valid_instance_field_token(
            0, 5, 6, 0),
        "invalid reference-token layout rejected");
  check(jcvm::cap_constant_pool_validation::valid_instance_field_token(
            0, 1, 0xff, 0),
        "primitive-only instance layout accepts no-reference sentinel");
  check(!jcvm::cap_constant_pool_validation::valid_instance_field_token(
            0, 1, 0xfe, 0),
        "invalid empty reference-token layout rejected");
  check(!jcvm::cap_constant_pool_validation::valid_instance_field_token(
            0, 1, 0xff, 1),
        "no-reference sentinel rejected when reference fields are declared");
  const uint8_t internal_member_records[] = {
      5, 0, 0, 3, 6, 0, 0, 7, 5, 0x80, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            internal_member_records, 3, 4, 2, 8),
        "internal static member offsets within component accepted");
  const uint8_t out_of_range_internal_field[] = {5, 0, 0, 4};
  check(!jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            out_of_range_internal_field, 1, 4, 0, 0),
        "internal static field offset at image size rejected");
  const uint8_t last_valid_internal_method[] = {6, 0, 0, 10};
  check(jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            last_valid_internal_method, 1, 0, 2, 8),
        "last one-based method offset within method area accepted");
  const uint8_t out_of_range_internal_method[] = {6, 0, 0, 11};
  check(!jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            out_of_range_internal_method, 1, 0, 2, 8),
        "internal static method offset beyond component rejected");
  const uint8_t handler_table_internal_method[] = {6, 0, 0, 2};
  check(!jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            handler_table_internal_method, 1, 0, 2, 8),
        "internal static method offset in handler table rejected");
  check(!jcvm::cap_constant_pool_validation::valid_internal_member_offsets(
            nullptr, 1, 0, 0, 0),
        "missing internal member records rejected");
  check(jcvm::cap_constant_pool_validation::valid_internal_class_token(
            4, 8, true, false, true),
        "internal class reference to class record accepted");
  check(jcvm::cap_constant_pool_validation::valid_internal_class_token(
            2, 8, true, true, false),
        "generic internal class reference to interface accepted");
  check(!jcvm::cap_constant_pool_validation::valid_internal_class_token(
            2, 8, true, true, true),
        "class-only internal reference to interface rejected");
  check(!jcvm::cap_constant_pool_validation::valid_internal_class_token(
            3, 8, false, false, false),
        "internal reference into record body rejected");
  check(!jcvm::cap_constant_pool_validation::valid_internal_class_token(
            8, 8, true, false, false),
        "internal class reference at component size rejected");
  const uint8_t internal_static_method[] = {6, 0, 0, 0x17};
  const uint8_t descriptor_for_method_0x17[] = {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0x17,
      0, 0, 0, 0, 0, 0, 0, 0};
  check(jcvm::cap_constant_pool_validation::
            valid_descriptor_static_method_offsets(
                internal_static_method, 1, descriptor_for_method_0x17,
                sizeof(descriptor_for_method_0x17)),
        "Descriptor method offset accepts internal StaticMethodref");
  const uint8_t descriptor_with_different_method[] = {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0x18,
      0, 0, 0, 0, 0, 0, 0, 0};
  check(!jcvm::cap_constant_pool_validation::
            valid_descriptor_static_method_offsets(
                internal_static_method, 1, descriptor_with_different_method,
                sizeof(descriptor_with_different_method)),
        "Descriptor rejects StaticMethodref without matching method offset");
  check(!jcvm::cap_constant_pool_validation::
            valid_descriptor_static_method_offsets(
                internal_static_method, 1, descriptor_for_method_0x17, 10),
        "truncated Descriptor method record rejected");

  const uint8_t one_exported_method[] = {1, 0, 0, 0, 1, 0, 3};
  const uint8_t method_stream[] = {0, 0, 0, 0};
  const uint8_t descriptor_for_exported_method[] = {
      1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 3,
      0, 0, 0, 0, 0, 0, 0, 0};
  check(jcvm::cap_constant_pool_validation::valid_export_offsets(
            one_exported_method, sizeof(one_exported_method), 4, 2,
            method_stream, sizeof(method_stream),
            descriptor_for_exported_method,
            sizeof(descriptor_for_exported_method)),
        "all Export offsets resolve within component bounds and Descriptor");
  const uint8_t exported_field_out_of_range[] = {1, 0, 0, 1, 0, 4};
  check(!jcvm::cap_constant_pool_validation::valid_export_offsets(
            exported_field_out_of_range, sizeof(exported_field_out_of_range),
            4, 2, method_stream, sizeof(method_stream)),
        "unreferenced exported static field outside image rejected");
  const uint8_t exported_method_out_of_range[] = {
      1, 0, 0, 0, 1, 0xff, 0xff};
  check(!jcvm::cap_constant_pool_validation::valid_export_offsets(
            exported_method_out_of_range,
            sizeof(exported_method_out_of_range), 4, 2, method_stream,
            sizeof(method_stream)),
        "unreferenced exported static method beyond component rejected");
  const uint8_t exported_method_not_in_descriptor[] = {
      1, 0, 0, 0, 1, 0, 4};
  check(!jcvm::cap_constant_pool_validation::valid_export_offsets(
            exported_method_not_in_descriptor,
            sizeof(exported_method_not_in_descriptor), 4, 2, method_stream,
            sizeof(method_stream), descriptor_for_exported_method,
            sizeof(descriptor_for_exported_method)),
        "Export method offset must identify a Descriptor method record");

  unsigned int resolved_external_tokens = 0;
  check(jcvm::cap_constant_pool_validation::validate_external_class_tokens(
            external_records, 6,
            [&resolved_external_tokens](uint8_t kind, uint8_t package_token,
                                        uint8_t class_token,
                                        uint8_t member_token) {
              ++resolved_external_tokens;
              return kind >= 1 && kind <= 6 && package_token == 1 &&
                     class_token == 0 && member_token == 0;
            }) &&
            resolved_external_tokens == 6,
        "all external reference kinds invoke export validation");
  check(!jcvm::cap_constant_pool_validation::validate_external_class_tokens(
            external_records, 6,
            [](uint8_t, uint8_t, uint8_t, uint8_t) { return false; }),
        "unresolved external export token rejected");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
