#ifndef JCVM_PERSISTENT_INVENTORY_HPP
#define JCVM_PERSISTENT_INVENTORY_HPP

#include "jc_types/jref_t.hpp"

#include <cstdint>
#include <vector>

namespace jcvm {
class Heap;
namespace PersistentInventory {

using RootReferenceEnumerator = bool (*)(void (*)(jref_t, void *), void *,
                                         void *);

constexpr uint8_t kTagCapacity = 128;

struct Tag {
  uint8_t len = 0;
  uint8_t value[kTagCapacity] = {};
};

struct AppletRootTag {
  uint8_t owner;
  uint8_t package_id;
  uint16_t class_index;
};

enum class VisitResult : uint8_t {
  Complete,
  Stopped,
  InvalidCursor,
  InvalidTag,
  InvalidStaticLayout,
  InvalidArgument,
  ReadFailure,
  IncompleteReachability
};

using TagVisitor = bool (*)(const Tag &tag, void *context);
using StaticReferenceReader = bool (*)(uint8_t package_id, uint16_t offset,
                                       uint16_t &reference, void *context);
using StaticRootVisitor = bool (*)(uint8_t package_id, uint16_t offset,
                                   uint16_t reference, void *context);
using AppletRootVisitor = bool (*)(uint8_t owner, uint8_t package_id,
                                   uint16_t class_index, uint16_t reference,
                                   void *context);

/// Visit active persistent tags below a prefix without constructing an FS.
/// The cursor is local to this call and storage must not mutate during it.
VisitResult visitPrefix(const Tag &prefix, TagVisitor visitor,
                        void *context);

/// Visit the serialized instance/array namespace belonging to one applet.
/// Package static fields use a different namespace and are not included.
VisitResult visitApplet(uint8_t applet_owner, TagVisitor visitor,
                        void *context);

/// Decode an applet-instance root tag; returns false for non-root or malformed tags.
bool isAppletInstanceRootTag(const Tag &tag,
                             uint8_t expected_owner) noexcept;
bool decodeAppletRootTag(const Tag &tag, uint8_t expected_owner,
                         AppletRootTag &root) noexcept;

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

/// Load static reference metadata from an installed package CAP, then visit it.
VisitResult visitInstalledPackageStaticRoots(uint8_t package_id, Heap &heap,
                                             StaticRootVisitor visitor,
                                             void *context);

/// Resolve registered applet-instance roots owned by one applet in a heap.
VisitResult visitAppletInstanceRoots(uint8_t applet_owner, Heap &heap,
                                     AppletRootVisitor visitor,
                                     void *context);

/// Visit owner-scoped serialized records outside every reachable persistent
/// object's tag subtree. A complete root trace is required before the first
/// candidate is reported. This function never erases tags.
VisitResult visitUnreachableAppletTags(
    uint8_t applet_owner, Heap &heap, const std::vector<jref_t> &roots,
    TagVisitor candidate_visitor, void *context);
VisitResult visitUnreachableAppletTags(
    uint8_t applet_owner, Heap &heap,
    RootReferenceEnumerator enumerate_roots, void *root_context,
    TagVisitor candidate_visitor, void *candidate_context);

} // namespace PersistentInventory
} // namespace jcvm

#endif
