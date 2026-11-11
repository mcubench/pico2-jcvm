#include "../../../choupi/src/jc_handlers/flashmemory.hpp"

namespace jcvm {

// This allocation-failure fixture does not exercise persistent rollback. Keep
// the generation stable while allowing JC_Array's cache guard to link without
// pulling the full flash backend into this standalone test executable.
uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return 0;
}

} // namespace jcvm
