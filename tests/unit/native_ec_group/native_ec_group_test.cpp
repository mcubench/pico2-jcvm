// Every curve loaded by native_ec::loadGroupForBits() must be released by
// mbedtls_ecp_group_free(). The 128/160-bit curves are built from hex at run
// time; marking them as static constants (h = 1) made the free a no-op and
// leaked about 160 bytes of RAM per Signature/KeyPair/KeyAgreement call
// (physical JCAlgTest option 1 ended in "PANIC Out of memory" during the
// EC_FP_128 ECDSA Signature_init() loop). LeakSanitizer reports any leak at
// exit; the test also checks that each loaded group is a usable curve.
#include "native_ec_group.hpp"

#include <cstdio>

namespace {
int failures = 0;

void expect(bool condition, const char *name) {
  std::printf("%s %s\n", condition ? "TEST:PASS" : "TEST:FAIL", name);
  if (!condition) ++failures;
}
} // namespace

int main() {
  static constexpr uint16_t kBits[] = {128, 160, 192, 224, 256, 384, 521};
  for (const uint16_t bits : kBits) {
    bool loaded = true;
    bool generator_on_curve = true;
    for (int round = 0; round < 64; ++round) {
      mbedtls_ecp_group group;
      mbedtls_ecp_group_init(&group);
      if (jcvm::native_ec::loadGroupForBits(group, bits) != 0) {
        loaded = false;
      } else if (mbedtls_ecp_check_pubkey(&group, &group.G) != 0) {
        generator_on_curve = false;
      }
      mbedtls_ecp_group_free(&group);
    }
    char name[96];
    std::snprintf(name, sizeof(name),
                  "native_ec_group_load_free_bits_%u", static_cast<unsigned>(bits));
    expect(loaded && generator_on_curve, name);
  }
  std::printf("TEST:SUMMARY fail=%d\n", failures);
  return failures == 0 ? 0 : 1;
}
