#include "../../choupi/src/jcvm_types/pc_t.hpp"
#include "../../choupi/src/jc_bytecodes/branch_offset.hpp"

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
  check(truncated.canReadAtOffset(bytes + 2, 1, 2),
        "relative diagnostic read within method is allowed");
  check(!truncated.canReadAtOffset(bytes + 2, -3, 1),
        "relative diagnostic read before method is rejected");
  check(!truncated.canReadAtOffset(bytes + 4, 1, 1),
        "relative diagnostic read beyond method is rejected");

  const uint8_t code[] = {0x00, 0x11, 0x00, 0x00, 0x00};
  jcvm::pc_t branch(code, code, code + sizeof(code));
  branch.setBranchFromOffset(code, 4, true);
  check(branch.getValue() == code + 4,
        "dynamic branch to a decoded instruction start is accepted");
  check(throws_security([&] {
          branch.setBranchFromOffset(code, 2, true);
        }),
        "dynamic branch into a decoded instruction operand is rejected");
  branch.setBranchFromOffset(code, 2, false);
  check(branch.getValue() == code + 2,
        "trusted branch skips dynamic instruction-alignment policy");

  check(jcvm::branch_offset::after_operand(-64, 2) == -67 &&
            jcvm::branch_offset::after_operand(64, 2) == 61,
        "signed branch displacement remains signed after operand adjustment");
  const uint8_t long_code[2400] = {};
  jcvm::pc_t backward_branch(long_code + 2201, long_code,
                             long_code + sizeof(long_code));
  backward_branch.updateBranchFromOffset(
      jcvm::branch_offset::after_operand(-64, 2), false);
  check(backward_branch.getValue() == long_code + 2134,
        "backward branch with operand adjustment stays in method bounds");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed ? count : 0,
              passed ? 0 : 1);
  return passed ? 0 : 1;
}
