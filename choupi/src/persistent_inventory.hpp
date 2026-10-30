#ifndef JCVM_PERSISTENT_INVENTORY_HPP
#define JCVM_PERSISTENT_INVENTORY_HPP

#include <cstdint>

namespace jcvm {
class Heap;

namespace PersistentInventory {

constexpr uint8_t kTagCapacity = 128;

struct Tag {
  uint8_t len = 0;
  uint8_t value[kTagCapacity] = {};
};

enum class VisitResult : uint8_t {
  Complete,
  Stopped,
  InvalidCursor,
  InvalidTag,
  InvalidStaticLayout,
  InvalidArgument,
  ReadFailure
};

using TagVisitor = bool (*)(const Tag &tag, void *context);
using StaticReferenceReader = bool (*)(uint8_t package_id, uint16_t offset,
                                       uint16_t &reference, void *context);
using StaticRootVisitor = bool (*)(uint8_t package_id, uint16_t offset,
                                   uint16_t reference, void *context);

/// Visit active persistent tags below a prefix without constructing an FS.
/// The cursor is local to this call and storage must not mutate during it.
VisitResult visitPrefix(const Tag &prefix, TagVisitor visitor,
                        void *context);

/// Visit the serialized instance/array namespace belonging to one applet.
/// Package static fields use a different namespace and are not included.
VisitResult visitApplet(uint8_t applet_owner, TagVisitor visitor,
                        void *context);

/// Visit the serialized static-field namespace belonging to one package.
VisitResult visitPackageStatics(uint8_t package_id, TagVisitor visitor,
                                void *context);

/// Visit every reference-valued static slot tag described by CAP metadata.
/// The CAP Static component places reference slots first, two bytes each.
VisitResult visitPackageStaticReferenceSlots(uint8_t package_id,
                                             uint16_t image_size,
                                             uint16_t reference_count,
                                             TagVisitor visitor,
                                             void *context);

/// Read and visit non-null 16-bit static references one slot at a time.
/// The reader must report failure distinctly from a null reference and should
/// register loaded references in the heap's static-root cache.
VisitResult visitPackageStaticRoots(uint8_t package_id, uint16_t image_size,
                                    uint16_t reference_count,
                                    StaticReferenceReader reader,
                                    StaticRootVisitor visitor, void *context);

/// Resolve package static roots through the heap cache and strict flash reader.
/// The caller supplies the decoded CAP Static Field metadata.
VisitResult visitPersistentPackageStaticRoots(
    uint8_t package_id, uint16_t image_size, uint16_t reference_count,
    Heap &heap, StaticRootVisitor visitor, void *context);

} // namespace PersistentInventory
} // namespace jcvm

#endif
