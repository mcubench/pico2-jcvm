#include "persistent_inventory.hpp"

#include "heap.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/package.hpp"
#include "jc_utils.hpp"

#include <cstring>

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

struct AppletRootReadContext {
  uint8_t owner;
  Heap *heap;
  AppletRootVisitor visitor;
  void *visitor_context;
  VisitResult failure = VisitResult::Complete;
};

bool visitAppletRootTag(const Tag &tag, void *opaque) {
  auto &context = *static_cast<AppletRootReadContext *>(opaque);
  if (tag.len < 2 || tag.value[0] != 0x03 || tag.value[1] != context.owner ||
      tag.value[tag.len - 1] != 0xff) {
    return true;
  }

  AppletRootTag root{};
  if (!decodeAppletRootTag(tag, context.owner, root)) {
    context.failure = VisitResult::InvalidTag;
    return false;
  }

  fs::Tag flash_tag{};
  flash_tag.len = tag.len;
  std::memcpy(flash_tag.value, tag.value, tag.len);
  try {
    const jref_t reference = FlashMemory_Handler::getPersistentField_ReferenceStrict(
        flash_tag, *context.heap);
    if (reference.isNullPointer() || reference.isArray()) {
      context.failure = VisitResult::InvalidTag;
      return false;
    }
    const auto instance = context.heap->getInstance(reference);
    if (instance->getPackageID() != root.package_id ||
        instance->getClassIndex() != root.class_index) {
      context.failure = VisitResult::InvalidTag;
      return false;
    }
    return context.visitor(root.owner, root.package_id, root.class_index,
                           reference.compact(), context.visitor_context);
  } catch (...) {
    context.failure = VisitResult::ReadFailure;
    return false;
  }
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
  try {
    const JC_Cap cap = Package(package_id).getCap();
    const auto *static_fields = cap.getStaticField();
    if (static_fields == nullptr) {
      return VisitResult::InvalidStaticLayout;
    }
    return visitPersistentPackageStaticRoots(
        package_id, NTOHS(static_fields->image_size),
        NTOHS(static_fields->reference_count), heap, visitor, context);
  } catch (...) {
    return VisitResult::InvalidStaticLayout;
  }
}

VisitResult visitAppletInstanceRoots(const uint8_t applet_owner, Heap &heap,
                                     const AppletRootVisitor visitor,
                                     void *context) {
  if (visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  AppletRootReadContext root_context{applet_owner, &heap, visitor, context};
  const VisitResult result =
      visitApplet(applet_owner, visitAppletRootTag, &root_context);
  return root_context.failure == VisitResult::Complete ? result
                                                       : root_context.failure;
}

} // namespace PersistentInventory
} // namespace jcvm
