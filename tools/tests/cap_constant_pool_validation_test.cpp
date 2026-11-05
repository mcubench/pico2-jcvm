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

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
