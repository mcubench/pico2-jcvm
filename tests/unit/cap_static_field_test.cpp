// Review finding #28: the StaticField component accessors must find the
// counts after variable-length array_init records. Delete this test together
// with the accessors if they are removed instead of fixed.
#include "jc_cap/jc_cap_static_field.hpp"

#include <cstdint>
#include <cstdio>

int main() {
  // tag, size, image_size=6, reference_count=1, array_init_count=2,
  // array_init[0] = {type 2 (byte), count 3, 01 02 03},
  // array_init[1] = {type 3 (short), count 4, 00 05 00 06},
  // default_value_count=2, non_default_value_count=2, values AA BB.
  alignas(4) const uint8_t component[] = {
      0x08, 0x00, 0x19, 0x00, 0x06, 0x00, 0x01, 0x00, 0x02,
      0x02, 0x00, 0x03, 0x01, 0x02, 0x03,
      0x03, 0x00, 0x04, 0x00, 0x05, 0x00, 0x06,
      0x00, 0x02, 0x00, 0x02, 0xaa, 0xbb};
  const auto *info =
      reinterpret_cast<const jcvm::jc_cap_static_field_component *>(component);
  unsigned failures = 0;
  const auto check = [&failures](const bool condition, const char *name) {
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
  };
  check(info->default_value_count() == 2,
        "default_value_count follows variable-length array_init records");
  check(info->non_default_value_count() == 2,
        "non_default_value_count follows variable-length array_init records");
  const auto values = info->non_default_values();
  check(values.size() == 2 && values[0] == 0xaa && values[1] == 0xbb,
        "non_default_values points at the trailing value bytes");
  std::printf("TEST:SUMMARY pass=%u fail=%u\n", 3 - failures, failures);
  return failures == 0 ? 0 : 1;
}
