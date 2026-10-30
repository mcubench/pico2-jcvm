#ifndef JCVM_PERSISTENT_INVENTORY_HPP
#define JCVM_PERSISTENT_INVENTORY_HPP

#include <cstdint>

namespace jcvm {
namespace PersistentInventory {

constexpr uint8_t kTagCapacity = 128;

struct Tag {
  uint8_t len = 0;
  uint8_t value[kTagCapacity] = {};
};

enum class VisitResult : uint8_t { Complete, Stopped, InvalidCursor, InvalidTag };

using TagVisitor = bool (*)(const Tag &tag, void *context);

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

} // namespace PersistentInventory
} // namespace jcvm

#endif
