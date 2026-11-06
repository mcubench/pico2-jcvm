#include "../../src/cap_method_bounds.hpp"
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "../../choupi/src/jc_cap/jc_cap_method.hpp"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <cstdio>

int main() {
  static_assert(sizeof(jcvm::jc_cap_extended_method_header_info) == 4,
                "extended CAP method header layout must be four bytes");
  bool passed = true;
  unsigned int count = 0;
  const auto check = [&passed, &count](bool condition, const char *name) {
    ++count;
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    passed = passed && condition;
  };

  check(jcvm::cap_method_bounds::header_fits(2, 0, 0x00),
        "two-byte normal method header fits exactly");
  check(!jcvm::cap_method_bounds::header_fits(1, 0, 0x00),
        "truncated normal method header is rejected");
  check(jcvm::cap_method_bounds::header_fits(4, 0, 0x80),
        "four-byte extended method header fits exactly");
  check(!jcvm::cap_method_bounds::header_fits(3, 0, 0x80),
        "truncated extended method header is rejected");
  check(!jcvm::cap_method_bounds::header_fits(10, 10, 0x00),
        "method offset at end is rejected");
  check(jcvm::cap_method_bounds::header_fits(12, 7, 0x00),
        "normal method header fits at nonzero offset");
  check(!jcvm::cap_method_bounds::header_fits(12, 9, 0x80),
        "extended header crossing method component end is rejected");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed ? count : 0,
              passed ? 0 : 1);
  return passed ? 0 : 1;
}
