#include "../../choupi/src/jcvm_types/pc_t.hpp"

#include <cstdio>
#include <cstdint>

namespace {
template <typename Function>
bool throws_security(Function function) {
  try {
    function();
  } catch (jcvm::Exceptions exception) {
    return exception == jcvm::Exceptions::SecurityException;
  }
  return false;
}
} // namespace

int main() {
  bool passed = true;
  unsigned int count = 0;
  const auto check = [&passed, &count](bool condition, const char *name) {
    ++count;
    std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
    passed = passed && condition;
  };

  const uint8_t bytes[] = {0x01, 0x23, 0x45, 0x67, 0x89};
  const uint8_t *const end = bytes + sizeof(bytes);
  jcvm::pc_t pc(bytes, bytes, end);
  check(pc.getNextByte() == 1 && pc.getValue() == bytes + 1,
        "one-byte fetch remains inside method");
  check(pc.getNextShort() == 0x2345 && pc.getValue() == bytes + 3,
        "two-byte fetch remains inside method");
  check(pc.getNextShort() == static_cast<int16_t>(0x6789) &&
            pc.getValue() == end,
        "exact-end two-byte fetch succeeds");
  check(throws_security([&] { pc.getNextByte(); }),
        "fetch at method end is rejected");

  jcvm::pc_t truncated(bytes + 2, bytes, bytes + 5);
  check(throws_security([&] { truncated.getNextInt(); }) &&
            truncated.getValue() == bytes + 2,
        "truncated four-byte fetch is rejected without advancing");
  check(throws_security([&] { truncated.setFromOffset(bytes, -1); }),
        "branch before method start is rejected");
  check(throws_security([&] { truncated.setFromOffset(bytes + 4, 1); }),
        "branch beyond method end is rejected");

  truncated.setFromOffset(bytes + 2, 1);
  check(truncated.getValue() == bytes + 3,
        "in-range positive branch target is accepted");
  truncated.updateFromOffset(-2);
  check(truncated.getValue() == bytes + 1,
        "in-range negative relative update is accepted");
  check(throws_security([&] {
          truncated.updateFromOffset(INT64_MIN);
        }),
        "minimum signed branch offset is rejected without overflow");
  check(throws_security([&] { truncated.setValue(end); }),
        "one-past-end program counter is rejected");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed ? count : 0,
              passed ? 0 : 1);
  return passed ? 0 : 1;
}
