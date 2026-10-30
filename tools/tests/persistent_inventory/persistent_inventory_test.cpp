#include "persistent_inventory.hpp"
#include "ffi.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {
struct CursorEntry {
  std::vector<uint8_t> tag;
};
std::vector<CursorEntry> entries;
bool stall_cursor = false;
bool malformed_tag = false;

struct Result {
  unsigned count = 0;
  bool saw_root = false;
  bool saw_array_body = false;
  bool saw_long_tag = false;
};

bool collect(const jcvm::PersistentInventory::Tag &tag, void *context) {
  auto &result = *static_cast<Result *>(context);
  ++result.count;
  result.saw_root |= tag.len == 5 && tag.value[4] == 0xff;
  result.saw_array_body |= tag.len == 7 && tag.value[5] == 0xff &&
                           tag.value[6] == 0xff;
  result.saw_long_tag |= tag.len == jcvm::PersistentInventory::kTagCapacity;
  return true;
}

bool stop_immediately(const jcvm::PersistentInventory::Tag &, void *) {
  return false;
}

bool expect(bool condition, const char *name) {
  std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
  return condition;
}
} // namespace

extern "C" uint8_t fs_next_tag(const uint8_t *prefix, uint8_t prefix_len,
                                fs_tag_next_request_t *request) {
  if (stall_cursor) {
    request->tag[0] = 5;
    std::memcpy(request->tag + 1, entries.front().tag.data(), 5);
    return 0;
  }
  while (request->bucket < entries.size()) {
    const size_t index = request->bucket;
    ++request->bucket;
    const auto &tag = entries[index].tag;
    if (tag.size() < prefix_len ||
        std::memcmp(tag.data(), prefix, prefix_len) != 0) {
      continue;
    }
    request->tag[0] = malformed_tag ? 1 : static_cast<uint8_t>(tag.size());
    std::memcpy(request->tag + 1, tag.data(), tag.size());
    return 0;
  }
  return 1;
}

extern "C" void path_static(uint8_t package_id, uint8_t offset,
                             uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x02;
  (*tag)[1] = package_id;
  (*tag)[2] = offset;
  *length = 3;
}

extern "C" void path_static16(uint8_t package_id, uint16_t offset,
                               uint8_t (*tag)[128], uint8_t *length) {
  (*tag)[0] = 0x02;
  (*tag)[1] = package_id;
  (*tag)[2] = static_cast<uint8_t>(offset >> 8);
  (*tag)[3] = static_cast<uint8_t>(offset);
  *length = 4;
}

int main() {
  std::vector<uint8_t> max_length_package_tag(128, 0x5a);
  max_length_package_tag[0] = 0x02;
  max_length_package_tag[1] = 9;
  entries = {{{0x03, 7, 0x38, 1, 0xff}},
             {{0x03, 7, 0x38, 1, 0x00, 0xff, 0xff}},
             {{0x03, 8, 0x38, 1, 0xff}},
             {{0x02, 9, 1}},
             {{0x02, 9, 2, 0xff, 0xff}},
             {max_length_package_tag}};
  Result result;
  bool ok = expect(jcvm::PersistentInventory::visitApplet(7, collect,
                                                           &result) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                       result.count == 2 && result.saw_root &&
                       result.saw_array_body,
                   "enumerates_only_applet_owner_namespace");

  Result package_result;
  ok &= expect(jcvm::PersistentInventory::visitPackageStatics(
                   9, collect, &package_result) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                   package_result.count == 3 && package_result.saw_long_tag,
               "enumerates_package_statics_including_maximum_tag");

  Result static_roots;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticReferenceSlots(
                   9, 10, 3, collect, &static_roots) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                   static_roots.count == 3,
               "enumerates_two_byte_static_reference_slots");
  Result extended_static_roots;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticReferenceSlots(
                   9, 258, 129, collect, &extended_static_roots) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                   extended_static_roots.count == 129,
               "uses_16_bit_static_offsets_after_255");
  Result invalid_static_roots;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticReferenceSlots(
                   9, 3, 2, collect, &invalid_static_roots) ==
                       jcvm::PersistentInventory::VisitResult::
                           InvalidStaticLayout &&
                   invalid_static_roots.count == 0,
               "rejects_static_reference_segment_larger_than_image");

  ok &= expect(jcvm::PersistentInventory::visitApplet(
                   7, stop_immediately, nullptr) ==
                   jcvm::PersistentInventory::VisitResult::Stopped,
               "visitor_can_stop_without_mutating_storage");

  stall_cursor = true;
  ok &= expect(jcvm::PersistentInventory::visitApplet(7, collect, &result) ==
                   jcvm::PersistentInventory::VisitResult::InvalidCursor,
               "rejects_nonadvancing_cursor");
  stall_cursor = false;

  malformed_tag = true;
  ok &= expect(jcvm::PersistentInventory::visitApplet(7, collect, &result) ==
                   jcvm::PersistentInventory::VisitResult::InvalidTag,
               "rejects_tag_that_does_not_match_requested_prefix");

  std::printf("TEST:SUMMARY fail=%u\n", ok ? 0u : 1u);
  return ok ? 0 : 1;
}
