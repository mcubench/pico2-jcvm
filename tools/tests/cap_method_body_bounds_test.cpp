#include "../../src/cap_method_body_bounds.hpp"

#include <cstdio>

int main() {
  bool passed = true;
  unsigned int count = 0;
  const auto check = [&passed, &count](bool condition, const char *name) {
    ++count;
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    passed = passed && condition;
  };

  // One class descriptor with one method record, followed by the required
  // zero-entry constant-pool-types count. Offset 1 is the method header.
  const uint8_t normal_descriptor[] = {
      1,                         // class_count
      0, 0, 0, 0, 0, 0, 0, 0, 1, // class header: one method
      0, 0, 0, 1,                // token, flags, method offset
      0, 0,                      // type offset
      0, 3,                      // bytecode_count
      0, 0, 0, 0,                // exception handler fields
      0, 0};                     // constant_pool_count
  const uint8_t normal_methods[] = {0x00, 0x00, 0x11, 0x22, 0x33, 0x44};
  jcvm::cap_method_body_bounds::Range range{};
  check(jcvm::cap_method_body_bounds::find(
            normal_descriptor, sizeof(normal_descriptor), normal_methods,
            sizeof(normal_methods), 0, 1, range) &&
            range.begin == 2 && range.end == 5,
        "normal method descriptor yields exact bytecode range");

  check(!jcvm::cap_method_body_bounds::find(
            normal_descriptor, sizeof(normal_descriptor), normal_methods,
            sizeof(normal_methods), 0, 2, range),
        "unlisted method offset is rejected");

  const uint8_t extended_descriptor[] = {
      1,                         // class_count
      0, 0, 0, 0, 0, 0, 0, 0, 1, // class header: one method
      0, 0, 0, 1,                // token, flags, method offset
      0, 0,                      // type offset
      0, 1,                      // bytecode_count
      0, 0, 0, 0,                // exception handler fields
      0, 0};                     // constant_pool_count
  const uint8_t extended_methods[] = {0x80, 0, 0, 0, 0x55};
  check(jcvm::cap_method_body_bounds::find(
            extended_descriptor, sizeof(extended_descriptor),
            extended_methods, sizeof(extended_methods), 0, 1, range) &&
            range.begin == 4 && range.end == 5,
        "extended method header yields exact bytecode range");

  check(!jcvm::cap_method_body_bounds::find(
            normal_descriptor, sizeof(normal_descriptor), normal_methods,
            4, 0, 1, range),
        "descriptor bytecode count beyond Method bytes is rejected");

  const uint8_t overlapping_descriptor[] = {
      1,                         // class_count
      0, 0, 0, 0, 0, 0, 0, 0, 2, // class header: two methods
      0, 0, 0, 1,                // first method offset
      0, 0, 0, 4,                // type offset, bytecode_count
      0, 0, 0, 0,                // exception handler fields
      0, 0, 0, 6,                // second method offset
      0, 0, 0, 1,                // type offset, bytecode_count
      0, 0, 0, 0,                // exception handler fields
      0, 0};                     // constant_pool_count
  const uint8_t overlapping_methods[] = {
      0, 0, 0x11, 0x22, 0x33, 0, 0, 0x44};
  check(jcvm::cap_method_body_bounds::find(
            overlapping_descriptor, sizeof(overlapping_descriptor),
            overlapping_methods, sizeof(overlapping_methods), 0, 1, range) &&
            range.begin == 2 && range.end == 5,
        "method body is clamped to the next descriptor method");
  check(!jcvm::cap_method_body_bounds::find(
            normal_descriptor, sizeof(normal_descriptor) - 3, normal_methods,
            sizeof(normal_methods), 0, 1, range),
        "truncated descriptor method record is rejected");
  check(!jcvm::cap_method_body_bounds::find(
            normal_descriptor, sizeof(normal_descriptor), normal_methods,
            sizeof(normal_methods), 8, 8, range),
        "method offset inside exception-handler table is rejected");

  const uint8_t descriptor_free_methods[] = {
      0x00, 0x00, 0x11, 0x22, 0x33, 0x00, 0x00, 0x44};
  const auto two_method_offsets = [](const auto &visit) {
    return visit(9) && visit(14);
  };
  check(jcvm::cap_method_body_bounds::find_from_offsets(
            descriptor_free_methods, sizeof(descriptor_free_methods), 8, 9,
            two_method_offsets, range) &&
            range.begin == 2 && range.end == 5,
        "descriptor-free method stops at next structural method entry");
  check(jcvm::cap_method_body_bounds::find_from_offsets(
            descriptor_free_methods, sizeof(descriptor_free_methods), 8, 14,
            two_method_offsets, range) &&
            range.begin == 7 && range.end == 8,
        "last descriptor-free method stops at Method component end");

  const auto missing_current = [](const auto &visit) {
    return visit(14);
  };
  check(!jcvm::cap_method_body_bounds::find_from_offsets(
            descriptor_free_methods, sizeof(descriptor_free_methods), 8, 9,
            missing_current, range),
        "descriptor-free unlisted current method is rejected");
  const auto bad_offset = [](const auto &visit) {
    return visit(9) && visit(16);
  };
  check(!jcvm::cap_method_body_bounds::find_from_offsets(
            descriptor_free_methods, sizeof(descriptor_free_methods), 8, 9,
            bad_offset, range),
        "descriptor-free out-of-component method entry is rejected");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed ? count : 0,
              passed ? 0 : 1);
  return passed ? 0 : 1;
}
