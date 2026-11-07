#include "persistent_inventory.hpp"
#include "ffi.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace {
struct CursorEntry {
  std::vector<uint8_t> tag;
  std::vector<uint8_t> data{0};
};
std::vector<CursorEntry> entries;
bool stall_cursor = false;
bool malformed_tag = false;

struct Result {
  unsigned count = 0;
  bool saw_root = false;
  bool saw_array_body = false;
  bool saw_long_tag = false;
  unsigned read_count = 0;
  unsigned root_count = 0;
  uint16_t last_static_offset = 0;
  bool fail_static_read = false;
  bool stop_static_root = false;
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

bool read_static_reference(const uint8_t, const uint16_t offset,
                           uint16_t &reference, void *context) {
  auto &result = *static_cast<Result *>(context);
  ++result.read_count;
  if (result.fail_static_read && offset == 4) return false;
  reference = offset == 0 ? 0 : 0x0101;
  return true;
}

bool visit_static_root(const uint8_t package_id, const uint16_t offset,
                       const uint16_t reference, void *context) {
  auto &result = *static_cast<Result *>(context);
  ++result.root_count;
  result.last_static_offset = offset;
  return package_id == 9 && reference != 0 &&
         !result.stop_static_root;
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

extern "C" uint8_t fs_read_inplace(const uint8_t *tag, uint8_t tag_length,
                                    const uint8_t **data, uint32_t *length) {
  for (const auto &entry : entries) {
    if (entry.tag.size() == tag_length &&
        std::memcmp(entry.tag.data(), tag, tag_length) == 0) {
      *data = entry.data.data();
      *length = static_cast<uint32_t>(entry.data.size());
      return 0;
    }
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
  entries[3].data = {0xfe, 5, 0x03, 7, 0x38, 1, 0xff};
  entries[4].data = {0xfd, 5, 0x03, 8, 0x38, 1, 0xff};
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

  bool alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   7, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   alias_found,
               "finds_object_alias_into_deleted_applet_namespace");
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   8, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   alias_found,
               "finds_array_alias_into_deleted_applet_namespace");
  entries[3].data = {0};
  entries[4].data = {0};
  entries[2].data = {0xfe, 5, 0x03, 8, 0x38, 1, 0xff};
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   8, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                   !alias_found,
               "ignores_aliases_owned_by_target_applet_itself");
  entries[2].data = {0};
  entries[3].data = {0xfe, 5, 0x02, 9, 2, 0xff, 0xff};
  entries[4].data = {0xfd, 5, 0x03, 7, 0x38, 1, 0xff};
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   7, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   alias_found,
               "follows_alias_chain_to_applet_owned_record");
  entries[3].data = {0xfe, 20, 0x03};
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   7, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::InvalidTag &&
                   !alias_found,
               "rejects_malformed_persistent_alias");
  entries[3].data = {0};
  entries[4].data = {0};

  jcvm::PersistentInventory::AppletRootTag applet_root{};
  const jcvm::PersistentInventory::Tag short_applet_root{
      5, {0x03, 7, 9, 0x2a, 0xff}};
  ok &= expect(jcvm::PersistentInventory::decodeAppletRootTag(
                   short_applet_root, 7, applet_root) &&
                   applet_root.owner == 7 && applet_root.package_id == 9 &&
                   applet_root.class_index == 0x2a,
               "decodes_eight_bit_applet_instance_root");
  const jcvm::PersistentInventory::Tag long_applet_root{
      6, {0x03, 7, 9, 0x01, 0x23, 0xff}};
  ok &= expect(jcvm::PersistentInventory::decodeAppletRootTag(
                   long_applet_root, 7, applet_root) &&
                   applet_root.class_index == 0x0123,
               "decodes_sixteen_bit_applet_instance_root");
  ok &= expect(!jcvm::PersistentInventory::decodeAppletRootTag(
                   short_applet_root, 8, applet_root) &&
                   !jcvm::PersistentInventory::decodeAppletRootTag(
                       jcvm::PersistentInventory::Tag{
                           7, {0x03, 7, 9, 0x01, 0x23, 0x45, 0xff}},
                       7, applet_root),
               "rejects_wrong_owner_and_malformed_applet_root_tags");
  const jcvm::PersistentInventory::Tag array_body_ending_ff{
      7, {0x03, 7, 9, 1, 0x20, 0xff, 0xff}};
  ok &= expect(!jcvm::PersistentInventory::isAppletInstanceRootTag(
                   array_body_ending_ff, 7),
               "does_not_treat_array_element_ending_ff_as_applet_root");

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

  Result static_values;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticRoots(
                   9, 10, 3, read_static_reference, visit_static_root,
                   &static_values) ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                   static_values.read_count == 3 &&
                   static_values.root_count == 2 &&
                   static_values.last_static_offset == 4,
               "reads_all_static_slots_and_visits_only_non_null_roots");
  Result failed_static_values;
  failed_static_values.fail_static_read = true;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticRoots(
                   9, 10, 3, read_static_reference, visit_static_root,
                   &failed_static_values) ==
                       jcvm::PersistentInventory::VisitResult::ReadFailure &&
                   failed_static_values.root_count == 1,
               "fails_closed_on_static_root_read_failure");
  Result stopped_static_values;
  stopped_static_values.stop_static_root = true;
  ok &= expect(jcvm::PersistentInventory::visitPackageStaticRoots(
                   9, 10, 3, read_static_reference, visit_static_root,
                   &stopped_static_values) ==
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   stopped_static_values.root_count == 1,
               "supports_early_stop_during_static_root_walk");

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
  malformed_tag = false;

  entries.resize(5);
  const std::vector<uint8_t> deleted_target{0x03, 7, 0x38, 1, 0xff};
  constexpr uint8_t chain_length = 32;
  for (uint8_t index = 0; index < chain_length; ++index) {
    std::vector<uint8_t> tag{0x02, 9, static_cast<uint8_t>(0x10 + index)};
    std::vector<uint8_t> target{0x02, 9,
                                static_cast<uint8_t>(0x11 + index)};
    if (index + 1 == chain_length) target = deleted_target;
    std::vector<uint8_t> alias{0xfe, static_cast<uint8_t>(target.size())};
    alias.insert(alias.end(), target.begin(), target.end());
    entries.push_back(CursorEntry{std::move(tag), std::move(alias)});
  }
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   7, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::Stopped &&
                   alias_found,
               "accepts_maximum_supported_persistent_alias_depth");

  std::vector<uint8_t> extra_tag{0x02, 9,
                                 static_cast<uint8_t>(0x10 + chain_length)};
  std::vector<uint8_t> extra_target{0x03, 7, 0x39, 1, 0xff};
  std::vector<uint8_t> extra_alias{0xfd,
                                   static_cast<uint8_t>(extra_target.size())};
  extra_alias.insert(extra_alias.end(), extra_target.begin(), extra_target.end());
  std::vector<uint8_t> last_target{0x02, 9,
                                   static_cast<uint8_t>(0x10 + chain_length)};
  std::vector<uint8_t> last_alias{0xfd,
                                  static_cast<uint8_t>(last_target.size())};
  last_alias.insert(last_alias.end(), last_target.begin(), last_target.end());
  entries.back().data = std::move(last_alias);
  entries.push_back(CursorEntry{std::move(extra_tag), std::move(extra_alias)});
  alias_found = false;
  ok &= expect(jcvm::PersistentInventory::findAliasesToAppletOwner(
                   7, alias_found) ==
                       jcvm::PersistentInventory::VisitResult::InvalidTag &&
                   !alias_found,
               "rejects_persistent_alias_chain_beyond_supported_depth");

  std::printf("TEST:SUMMARY fail=%u\n", ok ? 0u : 1u);
  return ok ? 0 : 1;
}
