#include "object_deletion_policy.hpp"

#include <cstdio>
#include <cstdlib>

namespace {
unsigned checks = 0;

void check(const bool condition, const char *name) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", name);
    std::exit(EXIT_FAILURE);
  }
}
} // namespace

int main() {
  check(!pico2_jcvm::may_service_object_deletion(false, true, false),
        "no_request");
  check(!pico2_jcvm::may_service_object_deletion(true, false, false),
        "active_java_frames");
  check(!pico2_jcvm::may_service_object_deletion(true, true, true),
        "active_persistent_transaction");
  check(pico2_jcvm::may_service_object_deletion(true, true, false),
        "safe_apdu_boundary");
  std::printf("TEST:PASS object_deletion_policy checks=%u\n", checks);
  return EXIT_SUCCESS;
}
