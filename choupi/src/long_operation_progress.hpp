#ifndef _LONG_OPERATION_PROGRESS_HPP
#define _LONG_OPERATION_PROGRESS_HPP

#include <cstdint>

#ifndef PC_VERSION
extern "C" void choupi_vm_cooperative_progress(void);
extern "C" uint32_t choupi_time_us_32(void);
#endif

namespace jcvm {

/// Called from native loops whose length grows with the heap or the
/// persistent store (object-deletion scans, heap tracing). Those loops run
/// outside the interpreter, which otherwise refreshes the 15 s watchdog,
/// services USB and sends CCID time extensions. The clock is read only every
/// 32 calls and the cooperative hook runs at most every 20 ms.
inline void longOperationProgress() noexcept {
#ifndef PC_VERSION
  static uint32_t calls = 0;
  static uint32_t last_progress_us = 0;
  if ((++calls & 31u) != 0) return;
  const uint32_t now = choupi_time_us_32();
  if (now - last_progress_us < 20000u) return;
  last_progress_us = now;
  choupi_vm_cooperative_progress();
#endif
}

} // namespace jcvm

#endif /* _LONG_OPERATION_PROGRESS_HPP */
