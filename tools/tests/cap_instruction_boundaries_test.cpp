#include "../../src/cap_instruction_boundaries.hpp"

#include <array>
#include <cstdio>
#include <cstdint>

int main() {
  bool passed = true;
  unsigned int count = 0;
  const auto check = [&passed, &count](bool condition, const char *name) {
    ++count;
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    passed = passed && condition;
  };

  const uint8_t short_branch[] = {0x70, 0x02, 0x00};
  check(jcvm::cap_instruction_boundaries::validate_control_flow(
            short_branch, sizeof(short_branch)),
        "short branch to instruction start is accepted");

  const uint8_t branch_into_operand[] = {
      0x00, 0x11, 0x00, 0x00, 0x70, 0xfe};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            branch_into_operand, sizeof(branch_into_operand)),
        "short branch into push operand is rejected");

  const uint8_t branch_outside[] = {0x70, 0x01};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            branch_outside, sizeof(branch_outside)),
        "branch to method end is rejected");

  const uint8_t wide_branch[] = {0xa8, 0x00, 0x03, 0x00};
  check(jcvm::cap_instruction_boundaries::validate_control_flow(
            wide_branch, sizeof(wide_branch)),
        "wide branch to instruction start is accepted");

  const uint8_t wide_branch_into_operand[] = {
      0x11, 0x00, 0x00, 0xa8, 0xff, 0xfe};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            wide_branch_into_operand, sizeof(wide_branch_into_operand)),
        "wide branch into push operand is rejected");

  const uint8_t valid_stableswitch[] = {
      0x73, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x7a};
  check(jcvm::cap_instruction_boundaries::validate_control_flow(
            valid_stableswitch, sizeof(valid_stableswitch)),
        "all stable-switch destinations are checked");

  const uint8_t switch_into_operand[] = {
      0x73, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0x7a};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            switch_into_operand, sizeof(switch_into_operand)),
        "stable-switch default into operand is rejected");

  const uint8_t valid_slookupswitch[] = {
      0x75, 0x00, 0x09, 0x00, 0x01, 0x00, 0x00, 0x00, 0x09, 0x7a};
  check(jcvm::cap_instruction_boundaries::validate_control_flow(
            valid_slookupswitch, sizeof(valid_slookupswitch)),
        "short lookup-switch default and case are checked");

  const uint8_t truncated_push[] = {0x11, 0x00};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            truncated_push, sizeof(truncated_push)),
        "truncated fixed-width instruction is rejected");

  const uint8_t type_test_instruction[] = {0x94, 0x00, 0x00, 0x00};
  std::size_t type_test_width = 0;
  check(jcvm::cap_instruction_boundaries::instruction_width(
            type_test_instruction, sizeof(type_test_instruction), 0,
            type_test_width) &&
            type_test_width == sizeof(type_test_instruction),
        "checkcast includes atype and constant-pool operands");

  const uint8_t malformed_switch[] = {0x73, 0, 0, 0, 2, 0, 1, 0, 0, 0};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            malformed_switch, sizeof(malformed_switch)),
        "malformed switch range is rejected");

  const uint8_t unknown_opcode[] = {0xb9};
  check(!jcvm::cap_instruction_boundaries::validate_control_flow(
            unknown_opcode, sizeof(unknown_opcode)),
        "unknown opcode is rejected");

  uint32_t random = 0x8f31a5c7u;
  constexpr std::size_t fuzz_inputs = 100000;
  constexpr std::size_t fuzz_size = 32;
  for (std::size_t input = 0; input < fuzz_inputs; ++input) {
    std::array<uint8_t, fuzz_size> bytes{};
    for (auto &byte : bytes) {
      random ^= random << 13;
      random ^= random >> 17;
      random ^= random << 5;
      byte = static_cast<uint8_t>(random >> 24);
    }
    const std::size_t size = input % (fuzz_size + 1);
    (void)jcvm::cap_instruction_boundaries::validate_control_flow(
        bytes.data(), size);
    for (std::size_t offset = 0; offset < size; ++offset) {
      std::size_t width = 0;
      if (jcvm::cap_instruction_boundaries::instruction_width(
              bytes.data(), size, offset, width)) {
        if (width == 0 || width > size - offset) passed = false;
      }
      (void)jcvm::cap_instruction_boundaries::is_instruction_start(
          bytes.data(), size, offset);
    }
  }
  check(passed, "100000 deterministic malformed bytecode inputs stay bounded");
  std::printf("FUZZ:SUMMARY inputs=%zu max_bytes=%zu\n", fuzz_inputs,
              fuzz_size);

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed ? count : count - 1,
              passed ? 0 : 1);
  return passed ? 0 : 1;
}
