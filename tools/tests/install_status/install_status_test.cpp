#include "cap_load.hpp"

#include <cstdio>
#include <cstdlib>

int main() {
  struct StatusCase {
    uint8_t stage;
    uint16_t status;
    const char *name;
  };
  constexpr StatusCase cases[] = {
      {1, 0x6a80, "malformed_install"},
      {2, 0x6a80, "invalid_applet_data"},
      {3, 0x6a80, "invalid_parameter_field"},
      {4, 0x6a82, "package_not_found"},
      {5, 0x6a80, "applet_install_failed"},
      {6, 0x6581, "instance_metadata_write_failed"},
      {7, 0x6985, "transaction_unavailable"},
      {8, 0x6581, "rollback_failed"},
      {9, 0x6f00, "unexpected_exception"},
      {0, 0x6a80, "unknown_stage"},
  };
  for (const auto &test : cases) {
    if (CapLoadSession::failureStatusWordForStage(test.stage) != test.status) {
      std::printf("TEST:FAIL %s\n", test.name);
      return EXIT_FAILURE;
    }
    std::printf("TEST:PASS %s\n", test.name);
  }
  std::printf("TEST:SUMMARY fail=0\n");
  return EXIT_SUCCESS;
}
