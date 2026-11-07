#include "execution_budget.hpp"

#include <cassert>
#include <cstdint>
#include <cstdio>

namespace {
struct FakeClock {
  uint32_t now_us;
  void advance(uint32_t delta_us) { now_us += delta_us; }
};

bool nestedCall(jcvm::ExecutionBudget &budget, FakeClock &clock) {
  jcvm::ExecutionBudgetScope nested(budget, true, clock.now_us, 1);
  assert(budget.depth() == 2);
  return budget.expired(clock.now_us);
}
} // namespace

int main() {
  jcvm::ExecutionBudget budget;
  FakeClock clock{0xfffffff0u};
  {
    jcvm::ExecutionBudgetScope applet(budget, true, clock.now_us, 50);
    assert(budget.depth() == 1);
    assert(budget.startedAtUs() == clock.now_us);

    // Native progress and repeated SIO/nested interpreter scopes must not
    // restart the top-level clock, even when a nested scope requests a new
    // budget.
    for (unsigned call = 0; call < 10; ++call) {
      clock.advance(4);
      budget.reportProgress(clock.now_us);
      assert(!nestedCall(budget, clock));
      assert(budget.depth() == 1);
      assert(budget.startedAtUs() == 0xfffffff0u);
      assert(budget.lastProgressUs() == clock.now_us);
      assert(!budget.expired(clock.now_us));
    }
    clock.advance(9);
    budget.reportProgress(clock.now_us);
    assert(!budget.expired(clock.now_us));
    clock.advance(1);
    assert(budget.expired(clock.now_us));
    assert(budget.depth() == 1);
  }
  assert(!budget.active()); // RAII timeout unwind releases the top-level.

  // A following APDU receives a fresh deadline only after the prior top-level
  // scope has unwound.
  {
    jcvm::ExecutionBudgetScope next_applet(budget, true, clock.now_us, 20);
    assert(budget.depth() == 1);
    assert(budget.startedAtUs() == clock.now_us);
    assert(!budget.expired(clock.now_us));
  }
  {
    jcvm::ExecutionBudgetScope disabled(budget, false, clock.now_us, 1);
    assert(!budget.active());
  }

  std::puts("EXECUTION_BUDGET:PASS immutable_deadline=1 nested_sio=1 "
            "native_progress=1 wraparound=1 timeout_unwind=1 fresh_apdu=1");
  return 0;
}
