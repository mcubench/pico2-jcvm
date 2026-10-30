#include "persistent_inventory.hpp"

#include "ffi.h"

#include <cstring>

namespace jcvm {
namespace PersistentInventory {

VisitResult visitPrefix(const Tag &prefix, const TagVisitor visitor,
                        void *context) {
  if (prefix.len > kTagCapacity || visitor == nullptr) {
    return VisitResult::InvalidTag;
  }

  fs_tag_next_request_t request{};
  while (true) {
    const uint16_t previous_bucket = request.bucket;
    const uint16_t previous_offset = request.offset;
    const uint8_t status =
        fs_next_tag(prefix.value, prefix.len, &request);
    if (status == 1) {
      return VisitResult::Complete;
    }
    if (status != 0 ||
        (request.bucket < previous_bucket ||
         (request.bucket == previous_bucket &&
          request.offset <= previous_offset))) {
      return VisitResult::InvalidCursor;
    }

    const uint8_t tag_length = request.tag[0];
    if (tag_length == 0 || tag_length > kTagCapacity ||
        tag_length < prefix.len ||
        std::memcmp(request.tag + 1, prefix.value, prefix.len) != 0) {
      return VisitResult::InvalidTag;
    }

    Tag tag{};
    tag.len = tag_length;
    std::memcpy(tag.value, request.tag + 1, tag_length);
    if (!visitor(tag, context)) {
      return VisitResult::Stopped;
    }
  }
}

VisitResult visitApplet(const uint8_t applet_owner, const TagVisitor visitor,
                        void *context) {
  Tag prefix{};
  prefix.len = 2;
  prefix.value[0] = 0x03; // applet-owned persistent instance/array records
  prefix.value[1] = applet_owner;
  return visitPrefix(prefix, visitor, context);
}

VisitResult visitPackageStatics(const uint8_t package_id,
                                const TagVisitor visitor, void *context) {
  Tag prefix{};
  prefix.len = 2;
  prefix.value[0] = 0x02; // package static fields and their persistent graphs
  prefix.value[1] = package_id;
  return visitPrefix(prefix, visitor, context);
}

} // namespace PersistentInventory
} // namespace jcvm
