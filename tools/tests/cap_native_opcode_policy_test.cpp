#include "../../src/cap_execution_policy.hpp"

#include <cstdint>
#include <cstdio>

int main() {
  bool passed = true;
  const auto check = [&passed](bool condition, const char *name) {
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    passed = passed && condition;
  };

  check(!jcvm::cap_execution_policy::is_dynamic_package(0),
        "ROM package zero is not uploaded code");
  check(!jcvm::cap_execution_policy::is_dynamic_package(55),
        "last fixed package ID is not uploaded code");
  check(jcvm::cap_execution_policy::is_dynamic_package(56),
        "first CAP slot is untrusted uploaded code");
  check(jcvm::cap_execution_policy::is_dynamic_package(63),
        "last CAP slot is untrusted uploaded code");
  check(!jcvm::cap_execution_policy::is_dynamic_package(64),
        "IDs above the CAP slots are not allocated to uploaded code");

  std::puts(passed ? "TEST:SUMMARY pass=5 fail=0"
                   : "TEST:SUMMARY pass<5 fail>0");
  return passed ? 0 : 1;
}
