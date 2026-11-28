#include "../../../choupi/src/jc_handlers/flashmemory.hpp"

namespace jcvm {

// This allocation-failure fixture does not exercise persistent rollback. Keep
// the generation stable while allowing JC_Array's cache guard to link without
// pulling the full flash backend into this standalone test executable.
uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return 0;
}

// Applet-transaction RAM undo is not exercised by this unit test.
std::vector<uint8_t> *FlashMemory_Handler::findVolatileUndo(const void *) noexcept {
  return nullptr;
}
bool FlashMemory_Handler::needsVolatileUndo(const void *) noexcept { return false; }
void FlashMemory_Handler::addVolatileUndo(void *, VolatileRestore,
                                          std::vector<uint8_t>) {}
void FlashMemory_Handler::forgetVolatileUndo(const void *) noexcept {}
void FlashMemory_Handler::setAppletTransactionEndHook(void (*)(bool)) noexcept {}
} // namespace jcvm
