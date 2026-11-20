// Opt-in recoverable C++ allocation (see src/recoverable_allocation.cpp).
//
// On the RP2350 firmware a failed allocation panics unless it happens inside
// a RecoverableAllocationScope, where operator new throws std::bad_alloc.
// Only code that fails closed on std::bad_alloc may open a scope. On the host
// build operator new already throws, so the scope does nothing.

#ifndef CHOUPI_RECOVERABLE_ALLOCATION_HPP
#define CHOUPI_RECOVERABLE_ALLOCATION_HPP

#include <cstdint>

#ifndef PC_VERSION
extern "C" void choupi_recoverable_allocation_enter(void);
extern "C" void choupi_recoverable_allocation_exit(void);
extern "C" uint32_t choupi_recoverable_allocation_failures(void);
#endif

namespace jcvm {

class RecoverableAllocationScope {
public:
#ifndef PC_VERSION
  RecoverableAllocationScope() { choupi_recoverable_allocation_enter(); }
  ~RecoverableAllocationScope() { choupi_recoverable_allocation_exit(); }
#else
  RecoverableAllocationScope() = default;
#endif
  RecoverableAllocationScope(const RecoverableAllocationScope &) = delete;
  RecoverableAllocationScope &
  operator=(const RecoverableAllocationScope &) = delete;
};

} // namespace jcvm

#endif // CHOUPI_RECOVERABLE_ALLOCATION_HPP
