#include "persistent_inventory.hpp"
#include "long_operation_progress.hpp"

#ifndef PC_VERSION
// Weak fallbacks for host unit tests that build the inventory without the
// board layer; src/choupi_pico2.cpp provides the firmware definitions.
extern "C" __attribute__((weak)) uint32_t choupi_time_us_32(void) {
  return 0;
}
extern "C" __attribute__((weak)) void choupi_vm_cooperative_progress(void) {}
#endif

#include "heap.hpp"
#include "ffi.h"
#include "jc_handlers/flashmemory.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <utility>

extern "C" uint8_t fs_repair_overlay_index_if_needed(void)
    __attribute__((weak));

namespace jcvm {
namespace PersistentInventory {

namespace {
constexpr uint8_t kPersistentForeignReference = 8;
constexpr uint8_t kPersistentObjectAlias = 0xfe;
constexpr uint8_t kPersistentArrayAlias = 0xfd;
constexpr uint8_t kMaximumAliasDepth = 32;

// Alias-protection code only asks whether a candidate has one of these tag
// prefixes. Store tags as [length, bytes...] rather than reserving the full
// 129-byte Tag capacity for every target. Prefix-covered entries are
// redundant for the membership query and can be omitted safely.
class PackedTagPrefixes {
  std::vector<uint8_t> encoded_;

public:
  bool covers(const Tag &candidate) const noexcept {
    size_t offset = 0;
    while (offset < encoded_.size()) {
      const uint8_t length = encoded_[offset++];
      if (length == 0 || length > kTagCapacity ||
          length > encoded_.size() - offset) {
        return false;
      }
      if (candidate.len >= length &&
          std::memcmp(candidate.value, encoded_.data() + offset, length) == 0) {
        return true;
      }
      offset += length;
    }
    return false;
  }

  void add(const Tag &prefix) {
    if (prefix.len == 0 || prefix.len > kTagCapacity || covers(prefix)) return;
    encoded_.push_back(prefix.len);
    encoded_.insert(encoded_.end(), prefix.value,
                    prefix.value + prefix.len);
  }
};

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

struct CandidateScanSummary {
#ifdef PC_VERSION
  uint32_t scanned = 0;
  uint32_t applet_roots = 0;
  uint32_t heap_reachable = 0;
  uint32_t foreign_alias = 0;
  uint32_t reachable_alias = 0;
  uint32_t alias_body = 0;
  uint32_t candidates = 0;
  uint32_t reachable_alias_targets = 0;
  std::vector<uint32_t> reachable_alias_counts;
  std::vector<Tag> reachable_alias_roots;
#endif
};

struct CandidateContext {
  Heap *heap;
  uint8_t applet_owner;
  TagVisitor visitor;
  void *visitor_context;
  const PackedTagPrefixes *foreign_alias_targets;
  const std::vector<Tag> *reachable_alias_targets;
  CandidateScanSummary *summary;
  VisitResult failure = VisitResult::Complete;
};

// A reachable serialized field alias is itself a durable reference. The
// object walk normally marks its body target, but keep the body if that edge
// was missed by the in-memory graph (for example, a stale field layout or a
// partially restored object). This is deliberately conservative: once the
// alias field is cleared or its owner becomes unreachable, a later bounded
// pass can reclaim the body.
bool reachableAliasTargetsBody(const Tag &candidate, Heap &heap,
                               VisitResult &failure) {
  if (candidate.len < 7 ||
      candidate.value[candidate.len - 6] != 0xff ||
      (candidate.value[candidate.len - 5] != 0xfe &&
       candidate.value[candidate.len - 5] != 0xff &&
       candidate.value[candidate.len - 5] != 0xfd)) {
    return false;
  }

  Tag source{};
  source.len = static_cast<uint8_t>(candidate.len - 6);
  std::memcpy(source.value, candidate.value, source.len);
  fs::Tag heap_source{};
  heap_source.len = source.len;
  std::memcpy(heap_source.value, source.value, source.len);
  if (!heap.hasReachablePersistentTagPrefix(heap_source) ||
      fs_exists(source.value, source.len) == 0) {
    return false;
  }

  const uint8_t *data = nullptr;
  uint32_t length = 0;
  if (fs_read_inplace(source.value, source.len, &data, &length) != 0 ||
      data == nullptr || length == 0) {
    failure = VisitResult::ReadFailure;
    return false;
  }

  Tag current = source;
  for (uint8_t depth = 0; depth < kMaximumAliasDepth; ++depth) {
    if (data[0] != kPersistentObjectAlias &&
        data[0] != kPersistentArrayAlias) {
      return false;
    }
    if (length < 2 || data[1] == 0 || data[1] > kTagCapacity ||
        length != 2u + data[1]) {
      failure = VisitResult::InvalidTag;
      return false;
    }

    Tag target{};
    target.len = data[1];
    std::memcpy(target.value, data + 2, target.len);
    if (target.len == candidate.len &&
        std::memcmp(target.value, candidate.value, candidate.len) == 0) {
#if defined(CHOUPI_DEBUG_HEAP_GC) && !defined(PC_VERSION)
      std::printf("OBJECT_DELETION:CANDIDATE stage=alias-protect source=");
      for (uint8_t index = 0; index < source.len; ++index) {
        std::printf("%02X", source.value[index]);
      }
      std::printf(" target=");
      for (uint8_t index = 0; index < candidate.len; ++index) {
        std::printf("%02X", candidate.value[index]);
      }
      std::printf("\r\n");
      std::fflush(stdout);
#endif
      return true;
    }

    current = target;
    if (fs_exists(current.value, current.len) == 0) {
      failure = VisitResult::ReadFailure;
      return false;
    }
    if (fs_read_inplace(current.value, current.len, &data, &length) != 0 ||
        data == nullptr || length == 0) {
      failure = VisitResult::ReadFailure;
      return false;
    }
  }

  failure = VisitResult::InvalidTag;
  return false;
}

bool hasTagPrefixRoot(const Tag &candidate, const std::vector<Tag> &targets) {
  for (const Tag &target : targets) {
    if (candidate.len >= target.len &&
        std::memcmp(candidate.value, target.value, target.len) == 0) {
      return true;
    }
  }
  return false;
}

bool hasTagPrefixRoot(const Tag &candidate,
                      const PackedTagPrefixes &targets) {
  return targets.covers(candidate);
}

#ifdef PC_VERSION
void countReachableAliasRoot(const Tag &candidate,
                             const std::vector<Tag> &targets,
                             CandidateScanSummary *summary) {
  if (summary == nullptr) return;
  size_t best = targets.size();
  uint8_t best_length = 0;
  for (size_t index = 0; index < targets.size(); ++index) {
    const Tag &target = targets[index];
    if (target.len > best_length && candidate.len >= target.len &&
        std::memcmp(candidate.value, target.value, target.len) == 0) {
      best = index;
      best_length = target.len;
    }
  }
  if (best < summary->reachable_alias_counts.size()) {
    ++summary->reachable_alias_counts[best];
  }
}

#endif

#if defined(CHOUPI_DEBUG_HEAP_GC) && !defined(PC_VERSION)
bool isHeapPayloadTag(const Tag &tag) {
  for (uint8_t index = 0; index + 3 < tag.len; ++index) {
    if (tag.value[index] == 0xff &&
        (tag.value[index + 1] == 0xfd || tag.value[index + 1] == 0xfe ||
         tag.value[index + 1] == 0xff)) {
      return true;
    }
  }
  return false;
}

void logHeapPayloadDecision(const char *stage, const Tag &tag,
                            const bool reachable, const bool foreign_root) {
  if (!isHeapPayloadTag(tag)) return;
  std::printf("OBJECT_DELETION:CANDIDATE stage=%s reachable=%u foreign_root=%u tag=",
              stage, reachable ? 1u : 0u, foreign_root ? 1u : 0u);
  for (uint8_t index = 0; index < tag.len; ++index) {
    std::printf("%02X", tag.value[index]);
  }
  std::printf("\r\n");
  std::fflush(stdout);
}
#endif

bool visitUnreachableTag(const Tag &tag, void *opaque) {
  auto &context = *static_cast<CandidateContext *>(opaque);
#ifdef PC_VERSION
  if (context.summary != nullptr) ++context.summary->scanned;
#endif
  // Applet instance roots are registration records, not collectible object
  // payload. Their lifetime is controlled by applet uninstall/recovery.
  if (isAppletInstanceRootTag(tag, context.applet_owner)) {
#ifdef PC_VERSION
    if (context.summary != nullptr) ++context.summary->applet_roots;
#endif
    return true;
  }
  fs::Tag heap_tag{};
  heap_tag.len = tag.len;
  std::memcpy(heap_tag.value, tag.value, tag.len);
#if defined(CHOUPI_DEBUG_HEAP_GC) && !defined(PC_VERSION)
  const bool reachable = context.heap->hasReachablePersistentTagPrefix(heap_tag);
  const bool foreign_root =
      hasTagPrefixRoot(tag, *context.foreign_alias_targets);
  const bool reachable_alias_subtree =
      hasTagPrefixRoot(tag, *context.reachable_alias_targets);
  logHeapPayloadDecision("scan", tag, reachable, foreign_root);
#else
  const bool reachable = context.heap->hasReachablePersistentTagPrefix(heap_tag);
  const bool foreign_root =
      hasTagPrefixRoot(tag, *context.foreign_alias_targets);
  const bool reachable_alias_subtree =
      hasTagPrefixRoot(tag, *context.reachable_alias_targets);
#endif
  if (reachable) {
#ifdef PC_VERSION
    if (context.summary != nullptr) ++context.summary->heap_reachable;
#endif
    return true;
  }
  if (foreign_root) {
#ifdef PC_VERSION
    if (context.summary != nullptr) ++context.summary->foreign_alias;
#endif
    return true;
  }
  if (reachable_alias_subtree) {
#ifdef PC_VERSION
    if (context.summary != nullptr) ++context.summary->reachable_alias;
    countReachableAliasRoot(tag, *context.reachable_alias_targets,
                            context.summary);
#endif
    // Reachable aliases are resolved into heap handles and their descendants
    // are marked before this inventory pass. A prefix-wide keep here also
    // preserves detached historical descendants forever when the alias owner
    // remains live (for example, a persistent applet object with cleared key
    // cache fields).
  }
  if (reachableAliasTargetsBody(tag, *context.heap, context.failure)) {
#ifdef PC_VERSION
    if (context.summary != nullptr) ++context.summary->alias_body;
#endif
    return true;
  }
  if (context.failure != VisitResult::Complete) return false;
#ifdef PC_VERSION
  if (context.summary != nullptr) ++context.summary->candidates;
#endif
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

    longOperationProgress();
    Tag tag{};
    tag.len = tag_length;
    std::memcpy(tag.value, request.tag + 1, tag_length);
    if (!visitor(tag, context)) {
      return VisitResult::Stopped;
    }
  }
}

namespace {
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
    const uint8_t applet_owner, PackedTagPrefixes &targets) {
  try {
    return visitForeignAliasTargetsToAppletOwner(
        applet_owner,
        [](const Tag &target, void *opaque) {
          static_cast<PackedTagPrefixes *>(opaque)->add(target);
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

namespace {
struct ReachableAliasTargetContext {
  Heap *heap;
  std::vector<Tag> *targets;
  std::vector<jref_t> *references;
#ifdef PC_VERSION
  std::vector<Tag> *reference_tags;
#endif
  VisitResult failure = VisitResult::Complete;
};

bool collectReachableAliasTarget(const Tag &source_tag, void *opaque) {
  auto &context = *static_cast<ReachableAliasTargetContext *>(opaque);
  fs::Tag heap_tag{};
  heap_tag.len = source_tag.len;
  std::memcpy(heap_tag.value, source_tag.value, source_tag.len);
  if (!context.heap->hasReachablePersistentTagPrefix(heap_tag)) return true;

  const uint8_t *data = nullptr;
  uint32_t length = 0;
  if (fs_read_inplace(source_tag.value, source_tag.len, &data, &length) != 0 ||
      data == nullptr || length == 0) {
    context.failure = VisitResult::ReadFailure;
    return false;
  }

  if (data[0] != kPersistentObjectAlias &&
      data[0] != kPersistentArrayAlias) {
    return true;
  }
  Tag current = source_tag;
  for (uint8_t depth = 0; depth < kMaximumAliasDepth; ++depth) {
    if (data[0] != kPersistentObjectAlias &&
        data[0] != kPersistentArrayAlias) {
      fs::Tag leaf_tag{};
      leaf_tag.len = current.len;
      std::memcpy(leaf_tag.value, current.value, current.len);
      if (!context.heap->hasReachablePersistentTagPrefix(leaf_tag)) {
        fs::Tag fs_source{};
        fs_source.len = source_tag.len;
        std::memcpy(fs_source.value, source_tag.value, source_tag.len);
        try {
          const jref_t reference =
              FlashMemory_Handler::getPersistentField_ReferenceStrict(
                  fs_source, *context.heap);
          if (!reference.isNullPointer() && !reference.isGlobalArray() &&
              !reference.isGlobalInstance() &&
              std::find(context.references->begin(), context.references->end(),
                        reference) == context.references->end()) {
            context.references->push_back(reference);
#ifdef PC_VERSION
            context.reference_tags->push_back(current);
#endif
          }
        } catch (Exceptions) {
          context.failure = VisitResult::ReadFailure;
          return false;
        }
      }
      return true;
    }
    if (length < 2 || data[1] == 0 || data[1] > kTagCapacity ||
        length != 2u + data[1]) {
      context.failure = VisitResult::InvalidTag;
      return false;
    }

    Tag target{};
    target.len = data[1];
    std::memcpy(target.value, data + 2, target.len);
#ifdef PC_VERSION
    const char *scan_diagnostics = std::getenv("PICO2_GC_SCAN_DIAGNOSTICS");
    static std::vector<std::pair<Tag, Tag>> alias_diagnostic_samples;
    const bool sample_alias =
        scan_diagnostics != nullptr &&
        std::strcmp(scan_diagnostics, "1") == 0 &&
        alias_diagnostic_samples.size() < 256 &&
        std::none_of(alias_diagnostic_samples.begin(),
                     alias_diagnostic_samples.end(),
                     [&source_tag, &target](const auto &sample) {
                       return sample.first.len == source_tag.len &&
                              sample.second.len == target.len &&
                              std::memcmp(sample.first.value,
                                          source_tag.value,
                                          source_tag.len) == 0 &&
                              std::memcmp(sample.second.value,
                                          target.value, target.len) == 0;
                     });
    if (sample_alias) {
      std::fprintf(stderr, "OBJECT_DELETION:ALIAS_SAMPLE source=");
      for (uint8_t index = 0; index < source_tag.len; ++index) {
        std::fprintf(stderr, "%02X", source_tag.value[index]);
      }
      std::fprintf(stderr, " target=");
      for (uint8_t index = 0; index < target.len; ++index) {
        std::fprintf(stderr, "%02X", target.value[index]);
      }
      std::fprintf(stderr, "\n");
      alias_diagnostic_samples.emplace_back(source_tag, target);
    }
#endif
    // Keep the expanded target list only for PC/VPCD scan diagnostics. The
    // target list does not control tracing or candidate retention: the heap
    // mark check below prevents re-enqueueing already resolved bodies. On
    // RP2350, retaining one 129-byte Tag slot per alias can consume tens of
    // KiB during the same collection that is meant to recover memory.
    if (context.targets != nullptr &&
        !hasTagPrefixRoot(target, *context.targets)) {
      context.targets->push_back(target);
    }
    if (fs_read_inplace(target.value, target.len, &data, &length) != 0 ||
        data == nullptr || length == 0) {
      context.failure = VisitResult::ReadFailure;
      return false;
    }
    current = target;
  }
  context.failure = VisitResult::InvalidTag;
  return false;
}

using ReachableTagScanner = VisitResult (*)(uint8_t, TagVisitor, void *);

VisitResult collectReachableAliasTargets(const uint8_t owner, Heap &heap,
                                         std::vector<Tag> *targets,
                                         ReachableTagScanner scanner) {
  std::vector<jref_t> references;
#ifdef PC_VERSION
  std::vector<Tag> reference_tags;
#endif
  try {
    uint32_t passes = 0;
    for (;;) {
      references.clear();
#ifdef PC_VERSION
      reference_tags.clear();
      ReachableAliasTargetContext context{&heap, targets, &references,
                                          &reference_tags};
#else
      ReachableAliasTargetContext context{&heap, targets, &references};
#endif
      const VisitResult result = scanner(owner, collectReachableAliasTarget,
                                         &context);
      if (context.failure != VisitResult::Complete) return context.failure;
      if (result != VisitResult::Complete) return result;
      if (references.empty()) return VisitResult::Complete;
      ++passes;

#ifdef PC_VERSION
      if (std::getenv("PICO2_GC_SCAN_DIAGNOSTICS") != nullptr &&
          passes <= 32) {
        std::fprintf(stderr,
                     "OBJECT_DELETION:ALIAS_RETRACE pass=%u roots=%zu first=",
                     passes, references.size());
        if (!reference_tags.empty()) {
          for (uint8_t index = 0; index < reference_tags.front().len; ++index) {
            std::fprintf(stderr, "%02X", reference_tags.front().value[index]);
          }
        }
        std::fprintf(stderr, "\n");
      }
#endif
      if (passes > 32) return VisitResult::IncompleteReachability;

      VectorRoots root_context{&references};
      const Heap::ReachabilitySummary extended =
          heap.extendReachableFrom(enumerateVectorRoots, &root_context);
      if (!extended.complete) return VisitResult::IncompleteReachability;
    }
  } catch (...) {
    return VisitResult::IncompleteReachability;
  }
}
} // namespace

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
    const PackedTagPrefixes &foreign_alias_targets,
    const TagVisitor candidate_visitor, void *candidate_context,
    CandidateScanSummary *scan_summary = nullptr) {
  if (enumerate_roots == nullptr || candidate_visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  const Heap::ReachabilitySummary summary =
      heap.traceReachableFrom(enumerate_roots, root_context);
  if (!summary.complete) return VisitResult::IncompleteReachability;

  std::vector<Tag> reachable_alias_targets;
  if (summary.persistent_tagged_arrays != 0 ||
      summary.persistent_tagged_instances != 0) {
    std::vector<Tag> *diagnostic_targets =
        scan_summary == nullptr ? nullptr : &reachable_alias_targets;
    const VisitResult alias_scan = collectReachableAliasTargets(
        applet_owner, heap, diagnostic_targets, visitApplet);
    if (alias_scan != VisitResult::Complete) return alias_scan;
#ifdef PC_VERSION
  if (scan_summary != nullptr) {
    scan_summary->reachable_alias_targets = static_cast<uint32_t>(
        reachable_alias_targets.size());
    scan_summary->reachable_alias_counts.assign(reachable_alias_targets.size(),
                                                0);
    scan_summary->reachable_alias_roots = reachable_alias_targets;
  }
#endif
  }

  CandidateContext context{&heap, applet_owner, candidate_visitor,
                           candidate_context, &foreign_alias_targets,
                           &reachable_alias_targets, scan_summary};
  const VisitResult result =
      visitApplet(applet_owner, visitUnreachableTag, &context);
  return context.failure == VisitResult::Complete ? result : context.failure;
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
  PackedTagPrefixes foreign_alias_targets;
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
  const PackedTagPrefixes *foreign_alias_targets;
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
      hasTagPrefixRoot(tag, *context.foreign_alias_targets)) {
    return true;
  }
  // Reachable package-static aliases are resolved and traced before the
  // inventory pass. Do not preserve their entire historical tag prefix.
  return context.visitor(tag, context.visitor_context);
}

VisitResult visitUnreachablePackageStaticTagsWithAliasTargets(
    const uint8_t package_id, Heap &heap,
    const RootReferenceEnumerator enumerate_roots, void *root_context,
    const PackedTagPrefixes &foreign_alias_targets,
    const TagVisitor candidate_visitor, void *candidate_context) {
  if (enumerate_roots == nullptr || candidate_visitor == nullptr) {
    return VisitResult::InvalidArgument;
  }
  const Heap::ReachabilitySummary summary =
      heap.traceReachableFrom(enumerate_roots, root_context);
  if (!summary.complete) return VisitResult::IncompleteReachability;

  if (summary.persistent_tagged_arrays != 0 ||
      summary.persistent_tagged_instances != 0) {
    const VisitResult alias_scan = collectReachableAliasTargets(
        package_id, heap, nullptr, visitPackageStatics);
    if (alias_scan != VisitResult::Complete) return alias_scan;
  }

  PackageStaticCandidateContext context{
      &heap, candidate_visitor, candidate_context, &foreign_alias_targets};
  return visitPackageStatics(package_id, visitUnreachablePackageStaticTag,
                             &context);
}
} // namespace

static VisitResult collectForeignAliasTargetsToPackageStatics(
    const uint8_t package_id, PackedTagPrefixes &targets) {
  try {
    return visitForeignAliasTargetsToPackageStatics(
        package_id,
        [](const Tag &target, void *opaque) {
          static_cast<PackedTagPrefixes *>(opaque)->add(target);
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
  PackedTagPrefixes foreign_alias_targets;
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
  uint16_t skipped_unresolvable = 0;
  PersistentInventory::Tag first_unresolvable{};
};

void logReadFailure(const char *stage,
                    const PersistentInventory::VisitResult result) {
  if (result != PersistentInventory::VisitResult::ReadFailure) return;
  std::printf("OBJECT_DELETION:READ_FAILURE stage=%s\r\n", stage);
  std::fflush(stdout);
}

bool reportSkippedCandidates(const char *namespace_name,
                             const CandidateBatch &batch,
                             bool *repair_attempted,
                             bool *integrity_logged) {
  if (batch.skipped_unresolvable == 0) return false;
  const char *repair = "already_attempted";
  bool repaired = false;
  if (repair_attempted != nullptr && !*repair_attempted) {
    *repair_attempted = true;
    if (fs_repair_overlay_index_if_needed == nullptr) {
      repair = "unavailable";
    } else {
      const uint8_t status = fs_repair_overlay_index_if_needed();
      repair = status == 0 ? "not_needed"
                           : status == 1 ? "repaired" : "unrecoverable";
      repaired = status == 1;
    }
  }
  if (integrity_logged == nullptr || !*integrity_logged) {
    if (integrity_logged != nullptr) *integrity_logged = true;
    std::printf("OBJECT_DELETION:INTEGRITY stage=candidate_scan "
                "namespace=%s skipped=%u index_repair=%s tag=",
                namespace_name,
                static_cast<unsigned>(batch.skipped_unresolvable), repair);
    const uint8_t tag_length = std::min<uint8_t>(
        batch.first_unresolvable.len, PersistentInventory::kTagCapacity);
    for (uint8_t index = 0; index < tag_length; ++index) {
      std::printf("%02X", batch.first_unresolvable.value[index]);
    }
    std::printf("\r\n");
    std::fflush(stdout);
  }
  return repaired;
}

bool acceptAllCandidates(const PersistentInventory::Tag &, void *) {
  return true;
}

bool collectCandidateBatch(const PersistentInventory::Tag &tag,
                           void *opaque) {
  auto &batch = *static_cast<CandidateBatch *>(opaque);
  if (tag.len == 0 || tag.len > PersistentInventory::kTagCapacity ||
      (tag.value[0] != 0x02 && tag.value[0] != 0x03)) {
    if (batch.skipped_unresolvable == 0) batch.first_unresolvable = tag;
    ++batch.skipped_unresolvable;
    return true;
  }
  if (fs_exists(tag.value, tag.len) == 0) {
    if (batch.skipped_unresolvable == 0) batch.first_unresolvable = tag;
    ++batch.skipped_unresolvable;
    return true;
  }
  if (batch.count >= batch.tags.size()) return false;
  batch.tags[batch.count++] = tag;
#if defined(CHOUPI_DEBUG_HEAP_GC) && !defined(PC_VERSION)
  std::printf("OBJECT_DELETION:CANDIDATE stage=batch index=%u tag=",
              static_cast<unsigned>(batch.count - 1));
  for (uint8_t index = 0; index < tag.len; ++index) {
    std::printf("%02X", tag.value[index]);
  }
  std::printf("\r\n");
  std::fflush(stdout);
#endif
  return batch.count < batch.tags.size();
}

void sortCandidateBatch(CandidateBatch &batch) {
  const uint8_t count = batch.count < kTagBatchCapacity
                            ? batch.count
                            : kTagBatchCapacity;
  for (uint8_t index = 1; index < count; ++index) {
    const PersistentInventory::Tag candidate = batch.tags[index];
    uint8_t destination = index;
    while (destination != 0 &&
           batch.tags[destination - 1].len < candidate.len) {
      batch.tags[destination] = batch.tags[destination - 1];
      --destination;
    }
    batch.tags[destination] = candidate;
  }
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

  PersistentInventory::PackedTagPrefixes foreign_alias_targets;
  const VisitResult alias_scan =
      PersistentInventory::collectForeignAliasTargets(
          applet_owner, foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) {
    logReadFailure("alias_chain", alias_scan);
    return alias_scan;
  }

  // Validate the complete tag cursor before any mutation. Later passes may
  // stop after a bounded batch. Alias roots are immutable during this call;
  // each pass still performs a fresh strict heap-root trace.
#ifdef PC_VERSION
  PersistentInventory::CandidateScanSummary preflight_summary{};
  const char *diagnostics = std::getenv("PICO2_GC_SCAN_DIAGNOSTICS");
  const bool collect_diagnostics = diagnostics != nullptr &&
                                   std::strcmp(diagnostics, "1") == 0;
  PersistentInventory::CandidateScanSummary *scan_summary =
      collect_diagnostics ? &preflight_summary : nullptr;
#else
  PersistentInventory::CandidateScanSummary *scan_summary = nullptr;
#endif
  const VisitResult preflight =
      PersistentInventory::visitUnreachableAppletTagsWithAliasTargets(
          applet_owner, heap, enumerate_roots, root_context,
          foreign_alias_targets, acceptAllCandidates, nullptr,
          scan_summary);
  if (preflight != VisitResult::Complete) {
    logReadFailure("root_scan", preflight);
    return preflight;
  }
#ifdef PC_VERSION
  if (collect_diagnostics) {
    static uint32_t diagnostic_sequence = 0;
    ++diagnostic_sequence;
    if (diagnostic_sequence == 1 || diagnostic_sequence % 100 == 0) {
      std::fprintf(stderr,
                   "OBJECT_DELETION:SCAN seq=%u scanned=%u applet_roots=%u "
                   "heap_reachable=%u foreign_alias=%u reachable_alias=%u "
                   "alias_body=%u candidates=%u alias_targets=%u\n",
                   diagnostic_sequence, preflight_summary.scanned,
                   preflight_summary.applet_roots,
                   preflight_summary.heap_reachable,
                   preflight_summary.foreign_alias,
                   preflight_summary.reachable_alias,
                   preflight_summary.alias_body,
                   preflight_summary.candidates,
                   preflight_summary.reachable_alias_targets);
      std::vector<size_t> emitted;
      for (size_t rank = 0; rank < 8; ++rank) {
        size_t best = preflight_summary.reachable_alias_counts.size();
        uint32_t count = 0;
        for (size_t index = 0;
             index < preflight_summary.reachable_alias_counts.size();
             ++index) {
          if (preflight_summary.reachable_alias_counts[index] > count &&
              std::find(emitted.begin(), emitted.end(), index) ==
                  emitted.end()) {
            best = index;
            count = preflight_summary.reachable_alias_counts[index];
          }
        }
        if (best == preflight_summary.reachable_alias_counts.size() ||
            count == 0) {
          break;
        }
        std::fprintf(stderr, "OBJECT_DELETION:ALIAS_ROOT seq=%u count=%u tag=",
                     diagnostic_sequence, count);
        const PersistentInventory::Tag &root =
            preflight_summary.reachable_alias_roots[best];
        for (uint8_t index = 0; index < root.len; ++index) {
          std::fprintf(stderr, "%02X", root.value[index]);
        }
        std::fprintf(stderr, "\n");
        emitted.push_back(best);
      }
    }
  }
#endif

  uint32_t erased = 0;
  bool probe_repair_attempted = false;
  bool integrity_logged = false;
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
      logReadFailure("candidate_scan", scan);
      return scan;
    }
    if (scan == VisitResult::Stopped &&
        batch.count != kTagBatchCapacity) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::InvalidCursor;
    }
    if (batch.count == 0) {
      if (reportSkippedCandidates("applet", batch,
                                  &probe_repair_attempted,
                                  &integrity_logged)) {
        continue;
      }
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::Complete;
    }
    sortCandidateBatch(batch);
    if (reportSkippedCandidates("applet", batch,
                                &probe_repair_attempted,
                                &integrity_logged)) {
      continue;
    }
    batch.skipped_unresolvable = 0;

    for (uint8_t index = 0; index < batch.count; ++index) {
      const auto &candidate = batch.tags[index];
      if (candidate.len < 2 || candidate.value[0] != 0x03 ||
          candidate.value[1] != applet_owner ||
          fs_exists(candidate.value, candidate.len) == 0) {
        if (batch.skipped_unresolvable == 0)
          batch.first_unresolvable = candidate;
        ++batch.skipped_unresolvable;
        continue;
      }
      fs::Tag tag{};
      tag.len = candidate.len;
      std::memcpy(tag.value, candidate.value, candidate.len);
#if defined(CHOUPI_DEBUG_HEAP_GC) && !defined(PC_VERSION)
      std::printf("OBJECT_DELETION:CANDIDATE stage=erase index=%u tag=",
                  static_cast<unsigned>(index));
      for (uint8_t tag_index = 0; tag_index < candidate.len; ++tag_index) {
        std::printf("%02X", candidate.value[tag_index]);
      }
      std::printf("\r\n");
      std::fflush(stdout);
#endif
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
    if (reportSkippedCandidates("applet", batch,
                                &probe_repair_attempted,
                                &integrity_logged)) {
      continue;
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

  PersistentInventory::PackedTagPrefixes foreign_alias_targets;
  const VisitResult alias_scan =
      PersistentInventory::collectForeignAliasTargetsToPackageStatics(
          package_id, foreign_alias_targets);
  if (alias_scan != VisitResult::Complete) {
    logReadFailure("alias_chain", alias_scan);
    return alias_scan;
  }

  const VisitResult preflight =
      PersistentInventory::visitUnreachablePackageStaticTagsWithAliasTargets(
          package_id, heap, enumerate_roots, root_context,
          foreign_alias_targets, acceptAllCandidates, nullptr);
  if (preflight != VisitResult::Complete) {
    logReadFailure("root_scan", preflight);
    return preflight;
  }

  uint32_t erased = 0;
  bool probe_repair_attempted = false;
  bool integrity_logged = false;
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
      logReadFailure("candidate_scan", scan);
      return scan;
    }
    if (scan == VisitResult::Stopped &&
        batch.count != kTagBatchCapacity) {
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::InvalidCursor;
    }
    if (batch.count == 0) {
      if (reportSkippedCandidates("package", batch,
                                  &probe_repair_attempted,
                                  &integrity_logged)) {
        continue;
      }
      if (erased_count != nullptr) *erased_count = erased;
      return VisitResult::Complete;
    }
    sortCandidateBatch(batch);
    if (reportSkippedCandidates("package", batch,
                                &probe_repair_attempted,
                                &integrity_logged)) {
      continue;
    }
    batch.skipped_unresolvable = 0;

    for (uint8_t index = 0; index < batch.count; ++index) {
      const auto &candidate = batch.tags[index];
      if (candidate.len <= 4 || candidate.value[0] != 0x02 ||
          candidate.value[1] != package_id ||
          fs_exists(candidate.value, candidate.len) == 0) {
        if (batch.skipped_unresolvable == 0)
          batch.first_unresolvable = candidate;
        ++batch.skipped_unresolvable;
        continue;
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
    if (reportSkippedCandidates("package", batch,
                                &probe_repair_attempted,
                                &integrity_logged)) {
      continue;
    }
  }
}

ObjectDeletionBatchResult reclaimUnreachableObjectDeletionBatch(
    const uint8_t applet_owner, const uint8_t package_id, Heap &heap,
    const PersistentInventory::RootReferenceEnumerator enumerate_roots,
    void *root_context, const uint32_t max_erased_count) {
  // Each candidate record is checked against the marked heap; answer those
  // checks from one sorted tag copy instead of a walk over all live objects.
  const Heap::ReachableTagIndexScope tag_index_scope;
  // The applet and package scans below repeat the same root trace on an
  // unchanged live heap; trace once per batch.
  const Heap::TraceReuseScope trace_reuse_scope;
  ObjectDeletionBatchResult result;
  result.applet_result = reclaimUnreachableAppletTags(
      applet_owner, heap, enumerate_roots, root_context,
      &result.erased_applet, max_erased_count);
  if (result.applet_result != PersistentInventory::VisitResult::Complete) {
    result.package_result = result.applet_result;
    return result;
  }

  const uint32_t remaining_budget =
      max_erased_count - result.erased_applet;
  if (remaining_budget == 0) {
    // The applet namespace consumed the whole budget. Defer even an empty
    // package namespace check so this call has a strict upper bound.
    result.package_result = PersistentInventory::VisitResult::Stopped;
    return result;
  }
  result.package_result = reclaimUnreachablePackageStaticTags(
      package_id, heap, enumerate_roots, root_context,
      &result.erased_package, remaining_budget);
  return result;
}
} // namespace PersistentReclaimer
} // namespace jcvm
