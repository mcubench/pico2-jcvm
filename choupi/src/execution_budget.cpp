#include "execution_budget.hpp"

namespace jcvm {

void ExecutionBudget::enter(const uint32_t now_us,
                            const uint32_t budget_us) noexcept {
  if (this->nesting_depth == 0) {
    this->started_at_us = now_us;
    this->last_progress_us = now_us;
    this->budget_us = budget_us;
  }
  ++this->nesting_depth;
}

void ExecutionBudget::leave() noexcept {
  if (this->nesting_depth != 0) {
    --this->nesting_depth;
  }
  if (this->nesting_depth == 0) {
    this->budget_us = 0;
  }
}

void ExecutionBudget::reportProgress(const uint32_t now_us) noexcept {
  if (this->active()) {
    this->last_progress_us = now_us;
  }
}

bool ExecutionBudget::expired(const uint32_t now_us) const noexcept {
  return this->active() &&
         static_cast<uint32_t>(now_us - this->started_at_us) >=
             this->budget_us;
}

ExecutionBudgetScope::ExecutionBudgetScope(ExecutionBudget &budget,
                                           const bool enabled,
                                           const uint32_t now_us,
                                           const uint32_t budget_us) noexcept
    : budget(enabled ? &budget : nullptr) {
  if (this->budget != nullptr) {
    this->budget->enter(now_us, budget_us);
  }
}

ExecutionBudgetScope::~ExecutionBudgetScope() noexcept {
  if (this->budget != nullptr) {
    this->budget->leave();
  }
}

} // namespace jcvm
