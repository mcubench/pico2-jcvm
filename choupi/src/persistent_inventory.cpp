#include "persistent_inventory.hpp"

#include "heap.hpp"
#include "ffi.h"
#include "jc_handlers/flashmemory.hpp"

#include <array>
#include <cstring>
#include <utility>

namespace jcvm {
namespace PersistentInventory {

namespace {
constexpr uint8_t kPersistentForeignReference = 8;

struct VectorRoots {
  const std::vector<jref_t> *references;
};

bool enumerateVectorRoots(const Heap::ReferenceVisitor visitor,
                          void *visitor_context, void *root_context) {
  if (visitor == nullptr || root_context == nullptr) return false;
  const auto &roots = *static_cast<VectorRoots *>(root_context)->references;
  for (const auto &reference : roots) visitor(reference, visitor_context);
  return true;
}

bool decodeForeignStaticTarget(const Tag &source_tag, const uint8_t *data,
                               const uint32_t length, Tag &target_tag) {
  if (data == nullptr || length < 3 || data[1] == 0xfe || data[1] == 0xff ||
      data[2] < 6 || data[2] > kTagCapacity ||
      length != 3u + data[2] ||
      data[2] != static_cast<uint32_t>(6u + source_tag.len)) {
    return false;
  }
  const uint8_t *target = data + 3;
  if (target[0] != 0x03 || target[1] == 0xfe || target[1] == 0xff ||
      target[2] != data[1] || target[3] != 0xff ||
      target[4] != 0xfc || target[5] != source_tag.len ||
      std::memcmp(target + 6, source_tag.value, source_tag.len) != 0) {
    return false;
  }

  target_tag = Tag{};
  target_tag.len = data[2];
  std::memcpy(target_tag.value, target, target_tag.len);
  return true;
}

struct CandidateContext {
  Heap *heap;
  uint8_t applet_owner;
  TagVisitor visitor;
  void *visitor_context;
  const std::vector<Tag> *foreign_alias_targets;
};

bool hasForeignAliasRoot(const Tag &candidate,
                         const std::vector<Tag> &targets) {
  for (const Tag &target : targets) {
    if (candidate.len >= target.len &&
        std::memcmp(candidate.value, target.value, target.len) == 0) {
      return true;
    }
  }
  return false;
}

bool visitUnreachableTag(const Tag &tag, void *opaque) {
  auto &context = *static_cast<CandidateContext *>(opaque);
  // Applet instance roots are registration records, not collectible object
  // payload. Their lifetime is controlled by applet uninstall/recovery.
  if (isAppletInstanceRootTag(tag, context.applet_owner)) return true;
  fs::Tag heap_tag{};
  heap_tag.len = tag.len;
  std::memcpy(heap_tag.value, tag.value, tag.len);
  if (context.heap->hasReachablePersistentTagPrefix(heap_tag) ||
      hasForeignAliasRoot(tag, *context.foreign_alias_targets)) return true;
  return context.visitor(tag, context.visitor_context);
}
} // namespace

VisitResult visitPrefix(const Tag &prefix, const TagVisitor visitor,
                        void *context) {
  if (prefix.len > kTagCapacity) {
    return VisitResult::InvalidTag;
  }
  if (visitor == nullptr) return VisitResult::InvalidArgument;

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

namespace {
constexpr uint8_t kPersistentObjectAlias = 0xfe;
constexpr uint8_t kPersistentArrayAlias = 0xfd;
constexpr uint8_t kMaximumAliasDepth = 32;

struct AliasTargetSearch {
  uint8_t owner;
  TagVisitor visitor;
  void *visitor_context;
  VisitResult failure = VisitResult::Complete;
};

bool collect_foreign_alias_target(const Tag &source_tag, void *opaque) {
  auto &search = *static_cast<AliasTargetSearch *>(opaque);
  if (source_tag.value[0] == 0x03 && source_tag.len >= 2 &&
      source_tag.value[1] == search.owner) {
    return true;
  }

  const uint8_t *data = nullptr;
  uint32_t length = 0;
  if (fs_read_inplace(source_tag.value, source_tag.len, &data, &length) != 0 ||
      data == nullptr || length == 0) {
    search.failure = VisitResult::ReadFailure;
    return false;
  }

  if (data[0] == kPersistentForeignReference) {
    Tag target{};
    if (!decodeForeignStaticTarget(source_tag, data, length, target)) {
      search.failure = VisitResult::InvalidTag;
      return false;
    }
    if (target.value[1] == search.owner &&
        !search.visitor(target, search.visitor_context)) {
      search.failure = VisitResult::Stopped;
      return false;
    }
    return true;
  }

  for (uint8_t depth = 0;; ++depth) {
    if (data[0] != kPersistentObjectAlias &&
        data[0] != kPersistentArrayAlias) {
      return true;
    }
    if (depth >= kMaximumAliasDepth) {
      search.failure = VisitResult::InvalidTag;
      return false;
    }
    if (length < 2 || data[1] == 0 || data[1] > kTagCapacity ||
        length != 2u + data[1]) {
      search.failure = VisitResult::InvalidTag;
      return false;
    }

    Tag target{};
    target.len = data[1];
    std::memcpy(target.value, data + 2, target.len);
    if (target.len >= 2 && target.value[0] == 0x03 &&
        target.value[1] == search.owner) {
      if (!search.visitor(target, search.visitor_context)) {
        search.failure = VisitResult::Stopped;
        return false;
      }
      return true;
    }

    if (fs_read_inplace(target.value, target.len, &data, &length) != 0 ||
        data == nullptr || length == 0) {
      search.failure = VisitResult::ReadFailure;
      return false;
    }
  }
}
} // namespace

VisitResult findAliasesToAppletOwner(const uint8_t applet_owner,
                                     bool &found) {
  found = false;
  const VisitResult result = visitForeignAliasTargetsToAppletOwner(
      applet_owner,
      [](const Tag &, void *opaque) {
        *static_cast<bool *>(opaque) = true;
        return false;
      },
      &found);
  return result == VisitResult::Stopped && found ? VisitResult::Stopped
                                                 : result;
}

VisitResult visitForeignAliasTargetsToAppletOwner(
    const uint8_t applet_owner, const TagVisitor visitor, void *context) {
  if (visitor == nullptr) return VisitResult::InvalidArgument;
  AliasTargetSearch search{applet_owner, visitor, context};
  const uint8_t namespaces[] = {0x02, 0x03};
  for (const uint8_t name_space : namespaces) {
    Tag prefix{};
    prefix.len = 1;
    prefix.value[0] = name_space;
    const VisitResult result =
        visitPrefix(prefix, collect_foreign_alias_target, &search);
    if (search.failure != VisitResult::Complete) return search.failure;
    if (result != VisitResult::Complete) return result;
  }
  return VisitResult::Complete;
}

static VisitResult collectForeignAliasTargets(
    const uint8_t applet_owner, std::vector<Tag> &targets) {
  try {
    return visitForeignAliasTargetsToAppletOwner(
        applet_owner,
        [](const Tag &target, void *opaque) {
          static_cast<std::vector<Tag> *>(opaque)->push_back(target);
          return true;
        },
        &targets);
  } catch (...) {
    return VisitResult::IncompleteReachability;
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

bool isAppletInstanceRootTag(const Tag &tag,
                             const uint8_t expected_owner) noexcept {
  return (tag.len == 5 || tag.len == 6) && tag.value[0] == 0x03 &&
         tag.value[1] == expected_owner && tag.value[tag.len - 1] == 0xff;
}

bool decodeAppletRootTag(const Tag &tag, const uint8_t expected_owner,
                         AppletRootTag &root) noexcept {
  if (!isAppletInstanceRootTag(tag, expected_owner)) {
    return false;
  }
  if (tag.len == 5) {
    root = AppletRootTag{tag.value[1], tag.value[2], tag.value[3]};
    return true;
  }
  if (tag.len == 6) {
    root = AppletRootTag{
        tag.value[1], tag.value[2],
        static_cast<uint16_t>((static_cast<uint16_t>(tag.value[3]) << 8) |
                              tag.value[4])};
    return true;
  }
  return false;
}

VisitResult visitPackageStatics(const uint8_t package_id,
                                const TagVisitor visitor, void *context) {
  Tag prefix{};
  prefix.len = 2;
  prefix.value[0] = 0x02; // package static fields and their persistent graphs
  prefix.value[1] = package_id;
  return visitPrefix(prefix, visitor, context);
}

VisitResult visitPackageStaticReferenceSlots(
    const uint8_t package_id, const uint16_t image_size,
    const uint16_t reference_count, const TagVisitor visitor, void *context) {
  if (visitor == nullptr) return VisitResult::InvalidArgument;
  if (reference_count > image_size / 2u) {
    return VisitResult::InvalidStaticLayout;
  }

  for (uint32_t offset = 0; offset < static_cast<uint32_t>(reference_count) * 2;
       offset += 2) {
    Tag tag{};
    if (offset <= UINT8_MAX) {
      path_static(package_id, static_cast<uint8_t>(offset), &tag.value,
                  &tag.len);
    } else {
      path_static16(package_id, static_cast<uint16_t>(offset), &tag.value,
                    &tag.len);
    }
    if (tag.len == 0 || tag.len > kTagCapacity || tag.value[0] != 0x02 ||
        tag.value[1] != package_id) {
      return VisitResult::InvalidTag;
    }
    if (!visitor(tag, context)) {
      return VisitResult::Stopped;
    }
  }
  return VisitResult::Complete;
}

VisitResult visitPackageStaticRoots(
    const uint8_t package_id, const uint16_t image_size,
    const uint16_t reference_count, const StaticReferenceReader reader,
    const StaticRootVisitor visitor, void *context) {
  if (reader == nullptr || visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  if (reference_count > image_size / 2u) {
    return VisitResult::InvalidStaticLayout;
  }

  for (uint32_t offset = 0; offset < static_cast<uint32_t>(reference_count) * 2;
       offset += 2) {
    uint16_t reference = 0;
    if (!reader(package_id, static_cast<uint16_t>(offset), reference,
                 context)) {
      return VisitResult::ReadFailure;
    }
    if (reference != 0 &&
        !visitor(package_id, static_cast<uint16_t>(offset), reference,
                 context)) {
      return VisitResult::Stopped;
    }
  }
  return VisitResult::Complete;
}

static VisitResult visitUnreachableAppletTagsWithAliasTargets(
    const uint8_t applet_owner, Heap &heap,
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    const std::vector<Tag> &foreign_alias_targets,
    const TagVisitor candidate_visitor, void *candidate_context) {
  if (enumerate_roots == nullptr || candidate_visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  const Heap::ReachabilitySummary summary =
      heap.traceReachableFrom(enumerate_roots, root_context);
  if (!summary.complete) return VisitResult::IncompleteReachability;

  CandidateContext context{&heap, applet_owner, candidate_visitor,
                           candidate_context, &foreign_alias_targets};
  return visitApplet(applet_owner, visitUnreachableTag, &context);
}

VisitResult visitUnreachableAppletTags(
    const uint8_t applet_owner, Heap &heap,
    const std::vector<jref_t> &roots, const TagVisitor candidate_visitor,
    void *context) {
  VectorRoots root_context{&roots};
  return visitUnreachableAppletTags(applet_owner, heap, enumerateVectorRoots,
                                    &root_context, candidate_visitor, context);
}

VisitResult visitUnreachableAppletTags(
    const uint8_t applet_owner, Heap &heap,
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    const TagVisitor candidate_visitor, void *candidate_context) {
  std::vector<Tag> foreign_alias_targets;
  const VisitResult alias_scan =
      collectForeignAliasTargets(applet_owner, foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) return alias_scan;
  return visitUnreachableAppletTagsWithAliasTargets(
      applet_owner, heap, enumerate_roots, root_context, foreign_alias_targets,
      candidate_visitor, candidate_context);
}

namespace {
struct PackageAliasTargetSearch {
  uint8_t package_id;
  TagVisitor visitor;
  void *visitor_context;
  VisitResult failure = VisitResult::Complete;
};

bool collect_package_static_alias_target(const Tag &source_tag,
                                         void *opaque) {
  auto &search = *static_cast<PackageAliasTargetSearch *>(opaque);
  // Aliases rooted in the package being inventoried are accounted for by the
  // supplied package-static roots and heap trace. Only external namespaces
  // can add an additional lifetime root.
  if (source_tag.value[0] == 0x02 && source_tag.len >= 2 &&
      source_tag.value[1] == search.package_id) {
    return true;
  }

  const uint8_t *data = nullptr;
  uint32_t length = 0;
  if (fs_read_inplace(source_tag.value, source_tag.len, &data, &length) != 0 ||
      data == nullptr || length == 0) {
    search.failure = VisitResult::ReadFailure;
    return false;
  }

  if (data[0] == kPersistentForeignReference) {
    Tag target{};
    if (!decodeForeignStaticTarget(source_tag, data, length, target)) {
      search.failure = VisitResult::InvalidTag;
      return false;
    }
    return true;
  }

  for (uint8_t depth = 0;; ++depth) {
    if (data[0] != kPersistentObjectAlias &&
        data[0] != kPersistentArrayAlias) {
      return true;
    }
    if (depth >= kMaximumAliasDepth || length < 2 || data[1] == 0 ||
        data[1] > kTagCapacity || length != 2u + data[1]) {
      search.failure = VisitResult::InvalidTag;
      return false;
    }

    Tag target{};
    target.len = data[1];
    std::memcpy(target.value, data + 2, target.len);
    if (target.len >= 2 && target.value[0] == 0x02 &&
        target.value[1] == search.package_id) {
      if (!search.visitor(target, search.visitor_context)) {
        search.failure = VisitResult::Stopped;
        return false;
      }
      return true;
    }

    if (fs_read_inplace(target.value, target.len, &data, &length) != 0 ||
        data == nullptr || length == 0) {
      search.failure = VisitResult::ReadFailure;
      return false;
    }
  }
}

VisitResult visitForeignAliasTargetsToPackageStatics(
    const uint8_t package_id, const TagVisitor visitor, void *context) {
  if (visitor == nullptr) return VisitResult::InvalidArgument;
  PackageAliasTargetSearch search{package_id, visitor, context};
  const uint8_t namespaces[] = {0x02, 0x03};
  for (const uint8_t name_space : namespaces) {
    Tag prefix{};
    prefix.len = 1;
    prefix.value[0] = name_space;
    const VisitResult result =
        visitPrefix(prefix, collect_package_static_alias_target, &search);
    if (search.failure != VisitResult::Complete) return search.failure;
    if (result != VisitResult::Complete) return result;
  }
  return VisitResult::Complete;
}

struct PackageStaticCandidateContext {
  Heap *heap;
  TagVisitor visitor;
  void *visitor_context;
  const std::vector<Tag> *foreign_alias_targets;
};

bool visitUnreachablePackageStaticTag(const Tag &tag, void *opaque) {
  auto &context = *static_cast<PackageStaticCandidateContext *>(opaque);
  // Three- and four-byte records are direct package static slots. Preserve
  // them even when null or primitive; only their object graphs are candidates.
  if (tag.len <= 4) return true;
  fs::Tag heap_tag{};
  heap_tag.len = tag.len;
  std::memcpy(heap_tag.value, tag.value, tag.len);
  if (context.heap->hasReachablePersistentTagPrefix(heap_tag) ||
      hasForeignAliasRoot(tag, *context.foreign_alias_targets)) {
    return true;
  }
  return context.visitor(tag, context.visitor_context);
}

VisitResult visitUnreachablePackageStaticTagsWithAliasTargets(
    const uint8_t package_id, Heap &heap,
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    const std::vector<Tag> &foreign_alias_targets,
    const TagVisitor candidate_visitor, void *candidate_context) {
  if (enumerate_roots == nullptr || candidate_visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  const Heap::ReachabilitySummary summary =
      heap.traceReachableFrom(enumerate_roots, root_context);
  if (!summary.complete) return VisitResult::IncompleteReachability;

  PackageStaticCandidateContext context{
      &heap, candidate_visitor, candidate_context, &foreign_alias_targets};
  return visitPackageStatics(package_id, visitUnreachablePackageStaticTag,
                             &context);
}
} // namespace

static VisitResult collectForeignAliasTargetsToPackageStatics(
    const uint8_t package_id, std::vector<Tag> &targets) {
  try {
    return visitForeignAliasTargetsToPackageStatics(
        package_id,
        [](const Tag &target, void *opaque) {
          static_cast<std::vector<Tag> *>(opaque)->push_back(target);
          return true;
        },
        &targets);
  } catch (...) {
    return VisitResult::IncompleteReachability;
  }
}

VisitResult visitUnreachablePackageStaticTags(
    const uint8_t package_id, Heap &heap,
    const std::vector<jref_t> &roots, const TagVisitor candidate_visitor,
    void *context) {
  VectorRoots root_context{&roots};
  return visitUnreachablePackageStaticTags(
      package_id, heap, enumerateVectorRoots, &root_context,
      candidate_visitor, context);
}

VisitResult visitUnreachablePackageStaticTags(
    const uint8_t package_id, Heap &heap,
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    const TagVisitor candidate_visitor, void *candidate_context) {
  std::vector<Tag> foreign_alias_targets;
  const VisitResult alias_scan =
      collectForeignAliasTargetsToPackageStatics(package_id,
                                                foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) return alias_scan;
  return visitUnreachablePackageStaticTagsWithAliasTargets(
      package_id, heap, enumerate_roots, root_context, foreign_alias_targets,
      candidate_visitor, candidate_context);
}

} // namespace PersistentInventory

namespace PersistentReclaimer {
namespace {
constexpr uint8_t kTagBatchCapacity = 8;

struct CandidateBatch {
  std::array<PersistentInventory::Tag, kTagBatchCapacity> tags{};
  uint8_t count = 0;
};

bool acceptAllCandidates(const PersistentInventory::Tag &, void *) {
  return true;
}

bool collectCandidateBatch(const PersistentInventory::Tag &tag,
                           void *opaque) {
  auto &batch = *static_cast<CandidateBatch *>(opaque);
  if (batch.count >= batch.tags.size()) return false;
  batch.tags[batch.count++] = tag;
  return batch.count < batch.tags.size();
}
} // namespace

PersistentInventory::VisitResult reclaimUnreachableAppletTags(
    const uint8_t applet_owner, Heap &heap,
    const PersistentInventory::RootReferenceEnumerator enumerate_roots,
    void *root_context, uint32_t *erased_count,
    const uint32_t max_erased_count) {
  using PersistentInventory::VisitResult;
  if (erased_count != nullptr) *erased_count = 0;
  if (enumerate_roots == nullptr || max_erased_count == 0) {
    return VisitResult::InvalidArgument;
  }
  if (FlashMemory_Handler::persistentWriteTransactionActive()) {
    return VisitResult::TransactionActive;
  }

  std::vector<PersistentInventory::Tag> foreign_alias_targets;
  const VisitResult alias_scan =
      PersistentInventory::collectForeignAliasTargets(
          applet_owner, foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) return alias_scan;

  // Validate the complete tag cursor before any mutation. Later passes may
  // stop after a bounded batch. Alias roots are immutable during this call;
  // each pass still performs a fresh strict heap-root trace.
  const VisitResult preflight =
      PersistentInventory::visitUnreachableAppletTagsWithAliasTargets(
          applet_owner, heap, enumerate_roots, root_context,
          foreign_alias_targets, acceptAllCandidates, nullptr);
  if (preflight != VisitResult::Complete) return preflight;

  uint32_t erased = 0;
  while (true) {
    if (FlashMemory_Handler::persistentWriteTransactionActive()) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::TransactionActive;
    }

    CandidateBatch batch;
    const VisitResult scan =
        PersistentInventory::visitUnreachableAppletTagsWithAliasTargets(
            applet_owner, heap, enumerate_roots, root_context,
            foreign_alias_targets, collectCandidateBatch, &batch);
    if (scan != VisitResult::Complete && scan != VisitResult::Stopped) {
      if (erased_count != nullptr) *erased_count = erased;
      return scan;
    }
    if (scan == VisitResult::Stopped &&
        batch.count != kTagBatchCapacity) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::InvalidCursor;
    }
    if (batch.count == 0) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::Complete;
    }

    for (uint8_t index = 0; index < batch.count; ++index) {
      const auto &candidate = batch.tags[index];
      if (candidate.len < 2 || candidate.value[0] != 0x03 ||
          candidate.value[1] != applet_owner ||
          fs_exists(candidate.value, candidate.len) == 0) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::ReadFailure;
      }
      fs::Tag tag{};
      tag.len = candidate.len;
      std::memcpy(tag.value, candidate.value, candidate.len);
      if (!FlashMemory_Handler::erasePersistentTag(tag) ||
          fs_exists(candidate.value, candidate.len) != 0) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::EraseFailure;
      }
      ++erased;
      if (erased >= max_erased_count) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::Stopped;
      }
    }
  }
}

PersistentInventory::VisitResult reclaimUnreachablePackageStaticTags(
    const uint8_t package_id, Heap &heap,
    const PersistentInventory::RootReferenceEnumerator enumerate_roots,
    void *root_context, uint32_t *erased_count,
    const uint32_t max_erased_count) {
  using PersistentInventory::VisitResult;
  if (erased_count != nullptr) *erased_count = 0;
  if (enumerate_roots == nullptr || max_erased_count == 0) {
    return VisitResult::InvalidArgument;
  }
  if (FlashMemory_Handler::persistentWriteTransactionActive()) {
    return VisitResult::TransactionActive;
  }

  std::vector<PersistentInventory::Tag> foreign_alias_targets;
  const VisitResult alias_scan =
      PersistentInventory::collectForeignAliasTargetsToPackageStatics(
          package_id, foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) return alias_scan;

  const VisitResult preflight =
      PersistentInventory::visitUnreachablePackageStaticTagsWithAliasTargets(
          package_id, heap, enumerate_roots, root_context,
          foreign_alias_targets, acceptAllCandidates, nullptr);
  if (preflight != VisitResult::Complete) return preflight;

  uint32_t erased = 0;
  while (true) {
    if (FlashMemory_Handler::persistentWriteTransactionActive()) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::TransactionActive;
    }

    CandidateBatch batch;
    const VisitResult scan =
        PersistentInventory::visitUnreachablePackageStaticTagsWithAliasTargets(
            package_id, heap, enumerate_roots, root_context,
            foreign_alias_targets, collectCandidateBatch, &batch);
    if (scan != VisitResult::Complete && scan != VisitResult::Stopped) {
      if (erased_count != nullptr) *erased_count = erased;
      return scan;
    }
    if (scan == VisitResult::Stopped &&
        batch.count != kTagBatchCapacity) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::InvalidCursor;
    }
    if (batch.count == 0) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::Complete;
    }

    for (uint8_t index = 0; index < batch.count; ++index) {
      const auto &candidate = batch.tags[index];
      if (candidate.len <= 4 || candidate.value[0] != 0x02 ||
          candidate.value[1] != package_id ||
          fs_exists(candidate.value, candidate.len) == 0) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::ReadFailure;
      }
      fs::Tag tag{};
      tag.len = candidate.len;
      std::memcpy(tag.value, candidate.value, candidate.len);
      if (!FlashMemory_Handler::erasePersistentTag(tag) ||
          fs_exists(candidate.value, candidate.len) != 0) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::EraseFailure;
      }
      ++erased;
      if (erased >= max_erased_count) {
        if (erased_count != nullptr) *erased_count = erased;
        return VisitResult::Stopped;
      }
    }
  }
}
} // namespace PersistentReclaimer
} // namespace jcvm
