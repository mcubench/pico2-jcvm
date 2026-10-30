#include "persistent_inventory.hpp"

#include "heap.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/package.hpp"
#include "jc_utils.hpp"

namespace jcvm {
namespace PersistentInventory {
namespace {

struct StaticRootReadContext {
  Heap *heap;
};

bool readPersistentStaticReference(const uint8_t package_id,
                                   const uint16_t offset,
                                   uint16_t &compact_reference,
                                   void *opaque) {
  auto &context = *static_cast<StaticRootReadContext *>(opaque);
  jref_t reference;
  if (!context.heap->getStaticReferenceRoot(package_id, offset, reference)) {
    const fs::Tag tag =
        FlashMemory_Handler::getStaticFieldTag(package_id, offset);
    try {
      reference =
          FlashMemory_Handler::getPersistentField_ReferenceStrict(tag,
                                                                  *context.heap);
      context.heap->setStaticReferenceRoot(package_id, offset, reference);
    } catch (...) {
      return false;
    }
  }
  compact_reference = reference.compact();
  return true;
}

} // namespace

VisitResult visitPersistentPackageStaticRoots(
    const uint8_t package_id, const uint16_t image_size,
    const uint16_t reference_count, Heap &heap,
    const StaticRootVisitor visitor, void *context) {
  StaticRootReadContext read_context{&heap};
  return visitPackageStaticRoots(package_id, image_size, reference_count,
                                 readPersistentStaticReference, visitor,
                                 &read_context);
}

VisitResult visitInstalledPackageStaticRoots(
    const uint8_t package_id, Heap &heap, const StaticRootVisitor visitor,
    void *context) {
  const JC_Cap cap = Package(package_id).getCap();
  const auto *static_fields = cap.getStaticField();
  if (static_fields == nullptr) {
    return VisitResult::InvalidStaticLayout;
  }
  return visitPersistentPackageStaticRoots(
      package_id, NTOHS(static_fields->image_size),
      NTOHS(static_fields->reference_count), heap, visitor, context);
}

} // namespace PersistentInventory
} // namespace jcvm
