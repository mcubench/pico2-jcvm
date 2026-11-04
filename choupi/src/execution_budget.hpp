#ifndef CHOUPI_EXECUTION_BUDGET_HPP
#define CHOUPI_EXECUTION_BUDGET_HPP

#include <cstdint>

namespace jcvm {

// One immutable elapsed-time budget is shared by nested interpreter entries.
// Progress timestamps are diagnostic/service state and never extend it.
class ExecutionBudget {
public:
  void enter(uint32_t now_us, uint32_t budget_us) noexcept;
  void leave() noexcept;
  void reportProgress(uint32_t now_us) noexcept;
  bool expired(uint32_t now_us) const noexcept;
  uint32_t startedAtUs() const noexcept { return started_at_us; }
  uint32_t lastProgressUs() const noexcept { return last_progress_us; }
  uint32_t depth() const noexcept { return nesting_depth; }
  bool active() const noexcept { return nesting_depth != 0; }

private:
  uint32_t started_at_us = 0;
  uint32_t last_progress_us = 0;
  uint32_t budget_us = 0;
  uint32_t nesting_depth = 0;
};

class ExecutionBudgetScope {
public:
  ExecutionBudgetScope(ExecutionBudget &budget, bool enabled,
                       uint32_t now_us, uint32_t budget_us) noexcept;
  ~ExecutionBudgetScope() noexcept;
  ExecutionBudgetScope(const ExecutionBudgetScope &) = delete;
  ExecutionBudgetScope &operator=(const ExecutionBudgetScope &) = delete;

private:
  ExecutionBudget *budget;
};

} // namespace jcvm

#endif
