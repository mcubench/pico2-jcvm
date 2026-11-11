/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#include "debug.hpp"
#include "interpretor.hpp"
#include "context.hpp"
#include "context_registry.hpp"
#include "jc_config.h"
#include "types.hpp"
#include "ffi.h"

#include "jc_handlers/jc_class.hpp"
#include "jc_handlers/jc_cp.hpp"
#include "jc_handlers/jc_export.hpp"
#include "jc_handlers/flashmemory.hpp"
#include "jc_handlers/jc_import.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"
#include "persistent_inventory.hpp"
#include "../../src/short_apdu.hpp"
#include "../../src/cap_store.hpp"
#include "../../src/vm_lifecycle.h"
#include "../../src/vm_applet_context.hpp"
#include "../../src/vm_diagnostics.h"

#include <cerrno>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <array>
#include <memory>
#include <utility>
#include <vector>

#ifdef PC_VERSION
extern int main_pc(int argc, char *argv[]);
#else
extern int main_arm(void);
#endif /* PC_VERSION */

int main
#ifdef PC_VERSION
    (int argc, char *argv[])
#else
    ()
#endif /* PC_VERSION */
{
#ifdef PC_VERSION
  return main_pc(argc, argv);
#else
  return main_arm();
#endif /* PC_VERSION */
}

namespace {
std::unique_ptr<jcvm::Context> selected_applet_context;
// The selected context plus this bounded inactive table cover the runtime's
// configured applet-instance limit. Keeping contexts alive preserves transient
// arrays across deselection; reset and deletion paths must explicitly clear
// or discard the affected entries.
std::array<std::unique_ptr<jcvm::Context>, JCVM_MAX_APPLETS - 1>
    inactive_applet_contexts{};
bool selected_applet_is_active = false;
bool selected_applet_reselection_pending = false;
bool selected_applet_is_reselecting = false;
bool selected_applet_is_selecting = false;
bool selected_applet_select_process_pending = false;
bool selected_applet_reselection_process_pending = false;
pico2_jcvm::ShortApduResponseChain pending_apdu_response;

extern "C" bool choupi_vm_applet_owner_in_use(const uint8_t owner_id) noexcept {
  if (selected_applet_context &&
      selected_applet_context->isAppletOwnerRetained(owner_id)) {
    return true;
  }
  for (auto &context : inactive_applet_contexts) {
    if (context && context->isAppletOwnerRetained(owner_id)) return true;
  }
  return false;
}

void collect_context_before_applet_delete(jcvm::Context &context) noexcept {
  try {
    std::vector<jcvm::jref_t> roots;
    context.appendRootReferences(roots);
    (void)context.getHeap().collectUnreachableFrom(roots);
  } catch (...) {
    // A failed root walk must not make deletion less conservative.
  }
}

extern "C" uint8_t choupi_vm_has_foreign_reference_to_applet(
    const uint8_t package_id, const uint8_t applet_owner) {
  // SIO can clear the last edge in an inactive context. Reclaim its now-dead
  // objects before scanning shared foreign-reference registries, which also
  // contain unrooted objects until their owning heap next runs collection.
  if (selected_applet_context) {
    collect_context_before_applet_delete(*selected_applet_context);
  }
  for (auto &context : inactive_applet_contexts) {
    if (context) collect_context_before_applet_delete(*context);
  }
  // Persistent aliases may belong to sibling applets that have not been
  // selected since boot, so no Context heap is available to inspect them.
  // Run the inventory scan before consulting materialized runtime heaps.
  bool persistent_alias_found = false;
  const auto alias_scan = jcvm::PersistentInventory::findAliasesToAppletOwner(
      applet_owner, persistent_alias_found);
  if (persistent_alias_found ||
      alias_scan != jcvm::PersistentInventory::VisitResult::Complete) {
    return 1;
  }
  for (auto &context : inactive_applet_contexts) {
    if (context && context->getAppletPackageID() == package_id &&
        context->isAppletOwnerRetained(applet_owner)) {
      if (context->hasForeignArrayReferenceToAppletOwner(applet_owner)) {
        return 1;
      }
    }
  }
  if (selected_applet_context &&
      selected_applet_context->getAppletPackageID() == package_id &&
      selected_applet_context->isAppletOwnerRetained(applet_owner)) {
    if (selected_applet_context->hasForeignArrayReferenceToAppletOwner(
            applet_owner)) {
      return 1;
    }
  }
  return 0;
}

bool context_matches_applet(const jcvm::Context &context,
                            const uint8_t owner,
                            const uint8_t *aid,
                            const uint8_t aid_length) noexcept {
  uint8_t context_aid[16]{};
  uint8_t context_aid_length = 0;
  return context.getAppletID() == owner &&
         context.copyAppletAID(context_aid, context_aid_length) &&
         context_aid_length == aid_length &&
         std::memcmp(context_aid, aid, aid_length) == 0;
}

bool is_select_for_applet(const uint8_t *command, const uint16_t length,
                          const jcvm::Context &context) noexcept {
  uint8_t aid[16]{};
  uint8_t aid_length = 0;
  if (command == nullptr || length < 5 || command[0] != 0x00 ||
      command[1] != 0xa4 || command[2] != 0x04 ||
      (command[3] & 0xef) != 0 ||
      !context.copyAppletAID(aid, aid_length) || command[4] != aid_length ||
      (length != static_cast<uint16_t>(5 + aid_length) &&
       length != static_cast<uint16_t>(6 + aid_length))) {
    return false;
  }
  return std::memcmp(command + 5, aid, aid_length) == 0;
}

class SelectingAppletProcessScope {
public:
  SelectingAppletProcessScope(const uint8_t *command, const uint16_t length,
                              const jcvm::Context *context) noexcept {
    const bool was_pending = selected_applet_select_process_pending;
    selected_applet_select_process_pending = false;
    selected_applet_is_selecting =
        was_pending && context != nullptr &&
        is_select_for_applet(command, length, *context);
    selected_applet_is_reselecting =
        selected_applet_is_selecting &&
        selected_applet_reselection_process_pending;
    selected_applet_reselection_process_pending = false;
  }

  ~SelectingAppletProcessScope() {
    selected_applet_is_selecting = false;
    selected_applet_is_reselecting = false;
  }

  SelectingAppletProcessScope(const SelectingAppletProcessScope &) = delete;
  SelectingAppletProcessScope &operator=(
      const SelectingAppletProcessScope &) = delete;
};

class APDUProcessScope {
public:
  explicit APDUProcessScope(jcvm::Context &context) noexcept
      : context_(context) {
    this->context_.setAPDUProcessActive(true);
  }

  ~APDUProcessScope() { this->context_.setAPDUProcessActive(false); }

  APDUProcessScope(const APDUProcessScope &) = delete;
  APDUProcessScope &operator=(const APDUProcessScope &) = delete;

private:
  jcvm::Context &context_;
};

struct RegisteredAppletRootContext {
  jcvm::Heap::ReferenceVisitor visitor;
  void *visitor_context;
  uint8_t owner;
  uint8_t package_id;
  uint16_t class_index;
  uint8_t root_count = 0;
  bool metadata_matches = true;
};

bool visit_registered_applet_root(uint8_t owner, uint8_t package_id,
                                  uint16_t class_index, uint16_t reference,
                                  void *opaque) {
  auto &expected = *static_cast<RegisteredAppletRootContext *>(opaque);
  if (owner != expected.owner || package_id != expected.package_id ||
      class_index != expected.class_index || expected.root_count != 0) {
    expected.metadata_matches = false;
    return false;
  }
  ++expected.root_count;
  expected.visitor(jcvm::jref_t(reference), expected.visitor_context);
  return true;
}

bool enumerate_applet_context_root(
    jcvm::Context &context, jcvm::Heap::ReferenceVisitor visitor,
    void *visitor_context) {
  if (visitor == nullptr || context.getRestoredAppletRef().isNullPointer()) {
    return false;
  }
  try {
    const auto instance = context.getHeap().getInstance(
        context.getRestoredAppletRef());
    RegisteredAppletRootContext root_context{
        visitor, visitor_context, context.getAppletID(),
        context.getAppletPackageID(), instance->getClassIndex()};
    const auto result = jcvm::PersistentInventory::visitAppletInstanceRoots(
        context.getAppletID(), context.getHeap(), visit_registered_applet_root,
        &root_context);
    return result == jcvm::PersistentInventory::VisitResult::Complete &&
           root_context.metadata_matches && root_context.root_count == 1;
  } catch (...) {
    return false;
  }
}

#ifndef PC_VERSION
struct SelectedAppletRootContext {
  jcvm::Context *context;
  jcvm::Heap::ReferenceVisitor visitor;
  void *visitor_context;
  unsigned static_root_count = 0;
  unsigned applet_root_count = 0;
  jcvm::PersistentInventory::VisitResult static_result =
      jcvm::PersistentInventory::VisitResult::InvalidArgument;
  jcvm::PersistentInventory::VisitResult applet_result =
      jcvm::PersistentInventory::VisitResult::InvalidArgument;
};

void visit_selected_applet_instance_root(const jcvm::jref_t reference,
                                         void *opaque) {
  auto &roots = *static_cast<SelectedAppletRootContext *>(opaque);
  ++roots.applet_root_count;
  roots.visitor(reference, roots.visitor_context);
}

bool enumerate_selected_applet_roots(
    jcvm::Heap::ReferenceVisitor visitor, void *visitor_context,
    void *opaque) {
  auto &roots = *static_cast<SelectedAppletRootContext *>(opaque);
  auto *selected_context = roots.context;
  roots.static_root_count = 0;
  roots.applet_root_count = 0;
  selected_context->visitRootReferences(visitor, visitor_context);
  roots.visitor = visitor;
  roots.visitor_context = visitor_context;
  roots.static_result = jcvm::PersistentInventory::VisitResult::Complete;
  const bool applet_root = enumerate_applet_context_root(
      *selected_context, visit_selected_applet_instance_root, &roots);
  roots.applet_result = applet_root
      ? jcvm::PersistentInventory::VisitResult::Complete
      : jcvm::PersistentInventory::VisitResult::ReadFailure;
  return applet_root;
}

#ifdef CHOUPI_DEBUG_HEAP_GC
struct PhysicalPersistentTagCount {
  unsigned count = 0;
};

bool count_physical_unreachable_tag(
    const jcvm::PersistentInventory::Tag &, void *opaque) {
  ++static_cast<PhysicalPersistentTagCount *>(opaque)->count;
  return true;
}
#endif
#endif

#ifdef PC_VERSION
struct PersistentInventoryCounts {
  unsigned static_roots = 0;
  unsigned applet_roots = 0;
  std::vector<jcvm::jref_t> references;
};

struct PersistentTagStats {
  unsigned tags = 0;
  std::uint64_t payload_bytes = 0;
  bool complete = true;
  bool dump_tags = false;
  const char *source = "unknown";
};

struct PersistentRootVector {
  std::vector<jcvm::jref_t> references;
};

unsigned long persistent_inventory_interval() {
  const char *setting =
      std::getenv("PICO2_PERSISTENT_INVENTORY_EVERY");
  if (setting == nullptr || *setting == '\0') return 0;
  for (const char *character = setting; *character != '\0'; ++character) {
    if (*character < '0' || *character > '9') return 0;
  }
  errno = 0;
  char *end = nullptr;
  const unsigned long interval = std::strtoul(setting, &end, 10);
  return errno == ERANGE || end == setting || *end != '\0' ? 0 : interval;
}

bool enumerate_persistent_root_vector(
    jcvm::Heap::ReferenceVisitor visitor, void *visitor_context,
    void *root_context) {
  if (visitor == nullptr || root_context == nullptr) return false;
  const auto &roots =
      static_cast<PersistentRootVector *>(root_context)->references;
  for (const auto &reference : roots) visitor(reference, visitor_context);
  return true;
}

void count_applet_inventory_root(const jcvm::jref_t reference,
                                 void *opaque) {
  auto &counts = *static_cast<PersistentInventoryCounts *>(opaque);
  ++counts.applet_roots;
  counts.references.emplace_back(reference);
}

bool count_package_static_inventory_root(const uint8_t, const uint16_t,
                                         const uint16_t reference,
                                         void *opaque) {
  auto &counts = *static_cast<PersistentInventoryCounts *>(opaque);
  ++counts.static_roots;
  counts.references.emplace_back(jcvm::jref_t(reference));
  return true;
}

bool accumulate_persistent_tag(
    const jcvm::PersistentInventory::Tag &tag, void *opaque) {
  auto &stats = *static_cast<PersistentTagStats *>(opaque);
  jcvm::fs::Tag fs_tag{};
  if (tag.len == 0 || tag.len > sizeof(fs_tag.value)) {
    stats.complete = false;
    return false;
  }
  fs_tag.len = tag.len;
  std::memcpy(fs_tag.value, tag.value, tag.len);
  try {
    const auto record =
        jcvm::FlashMemory_Handler::readPersistentRecord(fs_tag);
    ++stats.tags;
    stats.payload_bytes += record.first;
    if (stats.dump_tags) {
      std::fprintf(stderr, "PERSISTENT_TAG: source=%s len=%u payload=%u tag=",
                   stats.source, static_cast<unsigned>(tag.len),
                   static_cast<unsigned>(record.first));
      for (uint8_t i = 0; i < tag.len; ++i) {
        std::fprintf(stderr, "%02X", tag.value[i]);
      }
      std::fputc('\n', stderr);
    }
  } catch (...) {
    stats.complete = false;
    return false;
  }
  return true;
}

bool inventory_unreachable_tags_for_owner(
    const uint8_t owner, jcvm::Heap &heap,
    const std::vector<jcvm::jref_t> &roots, unsigned &owner_scans,
    PersistentTagStats &stats) {
  try {
    const auto result = jcvm::PersistentInventory::visitUnreachableAppletTags(
        owner, heap, roots, accumulate_persistent_tag, &stats);
    if (result != jcvm::PersistentInventory::VisitResult::Complete ||
        !stats.complete) {
      return false;
    }
    owner_scans = 1;
  } catch (...) {
    return false;
  }
  return true;
}

void inventory_persistent_roots_if_enabled(jcvm::Context &context) {
  const char *setting = std::getenv("PICO2_PERSISTENT_INVENTORY");
  const unsigned long interval = persistent_inventory_interval();
  const bool explicitly_enabled =
      setting != nullptr &&
      (std::strcmp(setting, "1") == 0 ||
       std::strcmp(setting, "once") == 0);
  if (!explicitly_enabled && interval == 0) {
    return;
  }

  PersistentInventoryCounts counts;
  // Load every static reference slot belonging to this applet package before
  // collecting roots. Walking unrelated packages into this heap would confuse
  // package-global handles with another package context's local handles.
  const auto static_result =
      jcvm::PersistentInventory::visitInstalledPackageStaticRoots(
          context.getAppletPackageID(), context.getHeap(),
          count_package_static_inventory_root, &counts);
  context.appendRootReferences(counts.references);
  const std::size_t context_roots = counts.references.size();
  const auto applet_result = enumerate_applet_context_root(
      context, count_applet_inventory_root, &counts)
      ? jcvm::PersistentInventory::VisitResult::Complete
      : jcvm::PersistentInventory::VisitResult::ReadFailure;
  jcvm::Heap::ReachabilitySummary reachability;
  unsigned owner_scans = 0;
  PersistentTagStats owner_records;
  PersistentTagStats unreachable_records;
  PersistentTagStats global_records;
  PersistentTagStats package_unreachable_records;
  owner_records.source = "owner";
  unreachable_records.source = "unreachable";
  global_records.source = "global";
  package_unreachable_records.source = "package-unreachable";
  const char *tag_dump_setting =
      std::getenv("PICO2_PERSISTENT_INVENTORY_TAGS");
  global_records.dump_tags = tag_dump_setting != nullptr &&
                             std::strcmp(tag_dump_setting, "1") == 0;
  const auto owner_inventory_result = jcvm::PersistentInventory::visitApplet(
      context.getAppletID(), accumulate_persistent_tag, &owner_records);
  const jcvm::PersistentInventory::Tag all_tags_prefix{};
  const auto global_inventory_result = jcvm::PersistentInventory::visitPrefix(
      all_tags_prefix, accumulate_persistent_tag, &global_records);
  const bool owner_records_complete =
      owner_inventory_result ==
          jcvm::PersistentInventory::VisitResult::Complete &&
      owner_records.complete;
  const bool global_records_complete =
      global_inventory_result ==
          jcvm::PersistentInventory::VisitResult::Complete &&
      global_records.complete;
  bool unreachable_complete = false;
  if (static_result == jcvm::PersistentInventory::VisitResult::Complete &&
      applet_result == jcvm::PersistentInventory::VisitResult::Complete &&
      owner_records_complete) {
    reachability = context.getHeap().traceReachableFrom(counts.references);
    if (reachability.complete) {
      unreachable_complete = inventory_unreachable_tags_for_owner(
          context.getAppletID(), context.getHeap(), counts.references,
          owner_scans, unreachable_records);
    }
  } else {
    reachability.complete = false;
    reachability.root_enumeration_failures = 1;
  }
  auto package_unreachable_result =
      jcvm::PersistentInventory::VisitResult::IncompleteReachability;
  if (reachability.complete) {
    package_unreachable_result =
        jcvm::PersistentInventory::visitUnreachablePackageStaticTags(
            context.getAppletPackageID(), context.getHeap(), counts.references,
            accumulate_persistent_tag, &package_unreachable_records);
  }
  static unsigned long inventory_sample_sequence = 0;
  ++inventory_sample_sequence;
  std::fprintf(stderr,
               "PERSISTENT_INVENTORY: owner=%u "
               "static_result=%u static_roots=%u "
               "applet_result=%u applet_roots=%u trace_complete=%u "
               "owner_scan=%u owner_result=%u applet_records_complete=%u "
               "applet_records=%u applet_payload_bytes=%llu "
               "global_records_complete=%u global_records=%u "
               "global_payload_bytes=%llu "
               "unreachable_complete=%u "
               "unreachable_tags=%u unreachable_payload_bytes=%llu "
               "reachable_arrays=%u/%u reachable_instances=%u/%u "
               "invalid_refs=%u edge_failures=%u root_failures=%u "
               "context_roots=%u package_unreachable_result=%u "
               "package_unreachable_complete=%u package_unreachable_tags=%u "
               "package_unreachable_payload_bytes=%llu sample=%lu\n",
               static_cast<unsigned>(context.getAppletID()),
               static_cast<unsigned>(static_result), counts.static_roots,
               static_cast<unsigned>(applet_result), counts.applet_roots,
               reachability.complete ? 1u : 0u,
               owner_scans, static_cast<unsigned>(owner_inventory_result),
               owner_records_complete ? 1u : 0u, owner_records.tags,
               static_cast<unsigned long long>(owner_records.payload_bytes),
               global_records_complete ? 1u : 0u, global_records.tags,
               static_cast<unsigned long long>(global_records.payload_bytes),
               unreachable_complete ? 1u : 0u, unreachable_records.tags,
               static_cast<unsigned long long>(
                   unreachable_records.payload_bytes),
               reachability.arrays_reachable, reachability.arrays_total,
               reachability.instances_reachable,
               reachability.instances_total, reachability.invalid_references,
               reachability.edge_load_failures,
               reachability.root_enumeration_failures,
               static_cast<unsigned>(context_roots),
               static_cast<unsigned>(package_unreachable_result),
               package_unreachable_result ==
                       jcvm::PersistentInventory::VisitResult::Complete &&
                       package_unreachable_records.complete
                   ? 1u
                   : 0u,
               package_unreachable_records.tags,
               static_cast<unsigned long long>(
                   package_unreachable_records.payload_bytes),
               inventory_sample_sequence);
}

void service_object_deletion_request_if_enabled(jcvm::Context &context) {
  const char *setting = std::getenv("PICO2_TEST_OBJECT_DELETION");
  if (setting == nullptr || std::strcmp(setting, "1") != 0 ||
      !context.isObjectDeletionRequested()) {
    return;
  }

  try {
    PersistentInventoryCounts roots;
    // Each package must have one retained heap/handle namespace. Peer package
    // contexts may cache cross-package static references or retain server
    // objects through shareable proxies; include those roots before tracing
    // this package's persistent records.
    if (!context.appendRetainedRootsForPackage(
            context.getAppletPackageID(), roots.references)) {
      std::fprintf(stderr,
                   "OBJECT_DELETION:SKIP package=%u owner=%u "
                   "reason=ambiguous_package_context request_retained=1\n",
                   static_cast<unsigned>(context.getAppletPackageID()),
                   static_cast<unsigned>(context.getAppletID()));
      return;
    }
    const auto static_result =
        jcvm::PersistentInventory::visitInstalledPackageStaticRoots(
            context.getAppletPackageID(), context.getHeap(),
            count_package_static_inventory_root, &roots);
    context.appendRootReferences(roots.references);
    const bool applet_root = enumerate_applet_context_root(
        context, count_applet_inventory_root, &roots);
    if (static_result != jcvm::PersistentInventory::VisitResult::Complete ||
        !applet_root) {
      std::fprintf(stderr,
                   "OBJECT_DELETION:SKIP package=%u static_result=%u "
                   "applet_roots_complete=%u\n",
                   static_cast<unsigned>(context.getAppletPackageID()),
                   static_cast<unsigned>(static_result),
                   applet_root ? 1u : 0u);
      return;
    }

    PersistentRootVector root_vector{std::move(roots.references)};
    uint32_t erased_applet = 0;
    uint32_t erased_package = 0;
    const auto applet_result =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            static_cast<uint8_t>(context.getAppletID()), context.getHeap(),
            enumerate_persistent_root_vector, &root_vector, &erased_applet);
    const auto package_result =
        jcvm::PersistentReclaimer::reclaimUnreachablePackageStaticTags(
            context.getAppletPackageID(), context.getHeap(),
            enumerate_persistent_root_vector, &root_vector, &erased_package);
    const bool complete =
        applet_result == jcvm::PersistentInventory::VisitResult::Complete &&
        package_result == jcvm::PersistentInventory::VisitResult::Complete;
    std::fprintf(stderr,
                 "OBJECT_DELETION: package=%u owner=%u applet_result=%u "
                 "package_result=%u erased_applet=%u erased_package=%u "
                 "static_roots=%u applet_roots=%u\n",
                 static_cast<unsigned>(context.getAppletPackageID()),
                 static_cast<unsigned>(context.getAppletID()),
                 static_cast<unsigned>(applet_result),
                 static_cast<unsigned>(package_result),
                 static_cast<unsigned>(erased_applet),
                 static_cast<unsigned>(erased_package),
                 roots.static_roots, roots.applet_roots);
    if (complete) context.clearObjectDeletionRequest();
  } catch (...) {
    std::fprintf(stderr,
                 "OBJECT_DELETION:ERROR package=%u owner=%u request_retained=1\n",
                 static_cast<unsigned>(context.getAppletPackageID()),
                 static_cast<unsigned>(context.getAppletID()));
  }
}

void collect_heap_after_apdu() {
  const char *trace_setting = std::getenv("PICO2_HEAP_TRACE");
  const char *collect_setting = std::getenv("PICO2_HEAP_GC");
  const char *inventory_setting = std::getenv("PICO2_PERSISTENT_INVENTORY");
  // Match the Pico runtime: collect the selected applet's RAM heap after every
  // APDU, rotate one inactive heap every 32 APDUs, and reclaim transient global
  // arrays each time. PICO2_HEAP_GC retains its diagnostic role (extra
  // collection of all retained contexts plus telemetry), not the switch that
  // turns the normal PC VM collector on.
  const bool collect_all_contexts = collect_setting != nullptr &&
                                    std::strcmp(collect_setting, "1") == 0;
  const bool trace_enabled = trace_setting != nullptr;
  const bool inventory_always_enabled =
      inventory_setting != nullptr &&
      (std::strcmp(inventory_setting, "1") == 0 ||
       std::strcmp(inventory_setting, "once") == 0);
  const unsigned long inventory_interval = persistent_inventory_interval();
  static unsigned long inventory_apdu_sequence = 0;
  const bool interval_sample =
      inventory_interval != 0 &&
      (++inventory_apdu_sequence % inventory_interval) == 0;
  const bool inventory_enabled =
      inventory_always_enabled || interval_sample;
  // The "once" mode is for integration tests: preserve one inventory/heap
  // snapshot per owner ID without rescanning every retained context per APDU.
  const bool inventory_once = inventory_setting != nullptr &&
                              std::strcmp(inventory_setting, "once") == 0;
  static std::array<bool, 256> inventory_reported{};
  if (!selected_applet_context) return;

  unsigned long repeat_count = 1;
  if (trace_enabled) {
    char *end = nullptr;
    repeat_count = std::strtoul(trace_setting, &end, 10);
    if (end == trace_setting || *end != '\0' || repeat_count == 0) {
      repeat_count = 1;
    }
  }
  if (repeat_count > 4) {
    repeat_count = 4;
  }

  const auto process_context = [&](jcvm::Context &context,
                                   const bool collect_heap) {
    const unsigned owner = static_cast<unsigned>(context.getAppletID());
    if (!context.getStack().empty()) {
      if (trace_enabled || collect_all_contexts) {
        std::fprintf(stderr,
                     "HEAP_TRACE:SKIP owner=%u reason=active_java_frames\n",
                     static_cast<unsigned>(context.getAppletID()));
      }
      return;
    }
    const bool already_reported =
        inventory_once && owner < inventory_reported.size() &&
        inventory_reported[owner];
    if (inventory_enabled && !already_reported) {
      try {
        inventory_persistent_roots_if_enabled(context);
      } catch (...) {
        std::fprintf(stderr,
                     "PERSISTENT_INVENTORY:ERROR owner=%u root_walk_failed\n",
                     static_cast<unsigned>(context.getAppletID()));
      }
      if (inventory_once && owner < inventory_reported.size()) {
        inventory_reported[owner] = true;
      }
    }
#ifdef PC_VERSION
    service_object_deletion_request_if_enabled(context);
#endif

    const unsigned long passes = trace_enabled ? repeat_count : 1;
    for (unsigned long pass = 1; pass <= passes; ++pass) {
      try {
        std::vector<jcvm::jref_t> roots;
        context.appendRootReferences(roots);
        const bool collect_this_pass = collect_heap && pass == 1;
        const auto summary = collect_this_pass
                                 ? context.getHeap().collectUnreachableFrom(roots)
                                 : context.getHeap().traceReachableFrom(roots);
        if (!summary.complete) {
          std::fprintf(stderr,
                       "HEAP_GC_SKIP owner=%u invalid=%u edge_failures=%u "
                       "root_failures=%u\n",
                       static_cast<unsigned>(context.getAppletID()),
                       static_cast<unsigned>(summary.invalid_references),
                       static_cast<unsigned>(summary.edge_load_failures),
                       static_cast<unsigned>(summary.root_enumeration_failures));
        }
        if (collect_all_contexts || trace_enabled) {
          std::fprintf(stderr,
                       "HEAP_TRACE: owner=%u pass=%lu roots=%u arrays=%u/%u "
                       "instances=%u/%u reclaimed_arrays=%u "
                       "reclaimed_instances=%u invalid=%u edge_failures=%u "
                       "complete=%u\n",
                       static_cast<unsigned>(context.getAppletID()), pass,
                       static_cast<unsigned>(roots.size()),
                       static_cast<unsigned>(summary.arrays_reachable),
                       static_cast<unsigned>(summary.arrays_total),
                       static_cast<unsigned>(summary.instances_reachable),
                       static_cast<unsigned>(summary.instances_total),
                       static_cast<unsigned>(summary.arrays_reclaimed),
                       static_cast<unsigned>(summary.instances_reclaimed),
                       static_cast<unsigned>(summary.invalid_references),
                       static_cast<unsigned>(summary.edge_load_failures),
                       summary.complete ? 1u : 0u);
        }
      } catch (...) {
        std::fprintf(stderr,
                     "HEAP_GC_SKIP owner=%u pass=%lu "
                     "reason=root_enumeration_failed\n",
                     static_cast<unsigned>(context.getAppletID()), pass);
        break;
      }
    }
  };

  process_context(*selected_applet_context, true);

  static uint32_t inactive_gc_sequence = 0;
  static std::size_t inactive_gc_cursor = 0;
  const bool collect_one_inactive = ++inactive_gc_sequence % 32 == 0;
  std::size_t scheduled_inactive = inactive_applet_contexts.size();
  if (collect_all_contexts || collect_one_inactive) {
    for (std::size_t visited = 0; visited < inactive_applet_contexts.size();
         ++visited) {
      const std::size_t index = inactive_gc_cursor++ %
                                inactive_applet_contexts.size();
      auto &context = inactive_applet_contexts[index];
      if (!context || !context->getStack().empty()) continue;
      scheduled_inactive = index;
      process_context(*context, true);
      if (!collect_all_contexts) break;
    }
  }
  if (trace_enabled || inventory_enabled) {
    for (std::size_t index = 0; index < inactive_applet_contexts.size();
         ++index) {
      auto &context = inactive_applet_contexts[index];
      if (context && index != scheduled_inactive) {
        process_context(*context, false);
      }
    }
  }

  const uint32_t reclaimed = jcvm::Heap::reclaimTransientGlobalArrays();
  if (collect_all_contexts) {
    std::fprintf(stderr, "HEAP_TRACE_GLOBAL_ARRAY_GC reclaimed=%u\n",
                 static_cast<unsigned>(reclaimed));
  }
}
#else
void collect_heap_after_apdu() {
  if (!selected_applet_context) {
    return;
  }

#ifdef CHOUPI_DEBUG_HEAP_GC
  static uint32_t collection_sequence = 0;
#endif

  jcvm::Context &context = *selected_applet_context;
  if (!context.getStack().empty()) {
    std::printf("CHOUPI:HEAP_GC_SKIP reason=active_java_frames\r\n");
    return;
  }

  SelectedAppletRootContext roots{&context, nullptr, nullptr};
  try {
#ifdef CHOUPI_DEBUG_PERSISTENT_GC
    struct PersistentGcCounts {
      uint32_t count = 0;
      jcvm::Heap *heap = nullptr;
      bool report_tags = false;
    } owner_tag_count{0, &context.getHeap(), true},
      candidate_tag_count{0, &context.getHeap(), false};
    const auto count_tag = [](const jcvm::PersistentInventory::Tag &tag,
                              void *opaque) {
      auto &counts = *static_cast<PersistentGcCounts *>(opaque);
      ++counts.count;
      if (counts.report_tags) {
        std::printf("CHOUPI:PERSISTENT_TAG owner_tag=");
        for (uint8_t index = 0; index < tag.len; ++index) {
          std::printf("%02X", tag.value[index]);
        }
        jcvm::fs::Tag heap_tag{};
        heap_tag.len = tag.len;
        std::memcpy(heap_tag.value, tag.value, tag.len);
        std::printf(" reachable=%u\r\n",
                    counts.heap->hasReachablePersistentTagPrefix(heap_tag)
                        ? 1u
                        : 0u);
      }
      return true;
    };
    const uint8_t applet_owner =
        static_cast<uint8_t>(context.getAppletID());
    const auto owner_inventory_result =
        jcvm::PersistentInventory::visitApplet(applet_owner, count_tag,
                                                &owner_tag_count);
    const auto candidate_inventory_result =
        jcvm::PersistentInventory::visitUnreachableAppletTags(
            applet_owner, context.getHeap(), enumerate_selected_applet_roots,
            &roots, count_tag, &candidate_tag_count);
    uint32_t erased_persistent_tags = 0;
    const auto persistent_gc_result =
        jcvm::PersistentReclaimer::reclaimUnreachableAppletTags(
            applet_owner, context.getHeap(),
            enumerate_selected_applet_roots, &roots,
            &erased_persistent_tags);
    std::printf(
        "CHOUPI:PERSISTENT_GC owner=%u result=%u erased=%lu "
        "tags=%lu owner_scan=%u candidates=%lu candidate_scan=%u\r\n",
        static_cast<unsigned>(applet_owner),
        static_cast<unsigned>(persistent_gc_result),
        static_cast<unsigned long>(erased_persistent_tags),
        static_cast<unsigned long>(owner_tag_count.count),
        static_cast<unsigned>(owner_inventory_result),
        static_cast<unsigned long>(candidate_tag_count.count),
        static_cast<unsigned>(candidate_inventory_result));
    std::fflush(stdout);
#endif
    const auto summary = context.getHeap().collectUnreachableFrom(
        enumerate_selected_applet_roots, &roots);
#ifdef CHOUPI_DEBUG_HEAP_GC
    ++collection_sequence;
    if ((collection_sequence % 32) == 0 || !summary.complete) {
      std::printf(
          "CHOUPI:HEAP_GC summary seq=%lu arrays=%lu/%lu reclaimed=%lu "
          "instances=%lu/%lu reclaimed=%lu roots_static=%lu "
          "roots_runtime=%lu tagged_arrays=%lu tagged_instances=%lu "
          "invalid=%lu edge_failures=%lu root_failures=%lu\r\n",
          static_cast<unsigned long>(collection_sequence),
          static_cast<unsigned long>(summary.arrays_reachable),
          static_cast<unsigned long>(summary.arrays_total),
          static_cast<unsigned long>(summary.arrays_reclaimed),
          static_cast<unsigned long>(summary.instances_reachable),
          static_cast<unsigned long>(summary.instances_total),
          static_cast<unsigned long>(summary.instances_reclaimed),
          static_cast<unsigned long>(summary.static_roots),
          static_cast<unsigned long>(summary.runtime_roots),
          static_cast<unsigned long>(summary.persistent_tagged_arrays),
          static_cast<unsigned long>(summary.persistent_tagged_instances),
          static_cast<unsigned long>(summary.invalid_references),
          static_cast<unsigned long>(summary.edge_load_failures),
          static_cast<unsigned long>(summary.root_enumeration_failures));
      std::fflush(stdout);
    }

    static bool persistent_inventory_reported = false;
    if (!persistent_inventory_reported && summary.complete) {
      PhysicalPersistentTagCount count;
      const auto inventory_result =
          jcvm::PersistentInventory::visitUnreachableAppletTags(
              static_cast<uint8_t>(context.getAppletID()), context.getHeap(),
              enumerate_selected_applet_roots, &roots,
              count_physical_unreachable_tag, &count);
      std::printf(
          "CHOUPI:PERSISTENT_INVENTORY owner=%u result=%u candidate_tags=%u "
          "static_roots=%u applet_roots=%u static_result=%u "
          "applet_result=%u\r\n",
          static_cast<unsigned>(context.getAppletID()),
          static_cast<unsigned>(inventory_result), count.count,
          roots.static_root_count, roots.applet_root_count,
          static_cast<unsigned>(roots.static_result),
          static_cast<unsigned>(roots.applet_result));
      std::fflush(stdout);
      persistent_inventory_reported =
          inventory_result ==
          jcvm::PersistentInventory::VisitResult::Complete;
    }
#endif
    if (!summary.complete) {
      std::printf(
          "CHOUPI:HEAP_GC_SKIP invalid=%u edge_failures=%u root_failures=%u "
          "static_result=%u applet_result=%u\r\n",
          static_cast<unsigned>(summary.invalid_references),
          static_cast<unsigned>(summary.edge_load_failures),
          static_cast<unsigned>(summary.root_enumeration_failures),
          static_cast<unsigned>(roots.static_result),
          static_cast<unsigned>(roots.applet_result));
    }
  } catch (...) {
    std::printf("CHOUPI:HEAP_GC_SKIP reason=root_enumeration_failed\r\n");
  }

  // Inactive contexts need collection too, but scanning every registered root
  // set for up to 39 heaps on each APDU is excessive. Visit one cached heap
  // every 32 APDUs, rotating through the bounded table.
  static uint32_t inactive_gc_sequence = 0;
  static std::size_t inactive_gc_cursor = 0;
  if (++inactive_gc_sequence % 32 == 0) {
    for (std::size_t visited = 0; visited < inactive_applet_contexts.size();
         ++visited) {
      const std::size_t index = inactive_gc_cursor++ %
                                inactive_applet_contexts.size();
      auto &inactive_context = inactive_applet_contexts[index];
      if (!inactive_context || !inactive_context->getStack().empty()) continue;
      SelectedAppletRootContext inactive_roots{
          inactive_context.get(), nullptr, nullptr};
      try {
#ifdef CHOUPI_DEBUG_HEAP_GC
        const auto inactive_summary =
            inactive_context->getHeap().collectUnreachableFrom(
                enumerate_selected_applet_roots, &inactive_roots);
        if (!inactive_summary.complete) {
          std::printf(
              "CHOUPI:HEAP_GC_SKIP owner=%u invalid=%u edge_failures=%u "
              "root_failures=%u\r\n",
              static_cast<unsigned>(inactive_context->getAppletID()),
              static_cast<unsigned>(inactive_summary.invalid_references),
              static_cast<unsigned>(inactive_summary.edge_load_failures),
              static_cast<unsigned>(inactive_summary.root_enumeration_failures));
        }
#else
        (void)inactive_context->getHeap().collectUnreachableFrom(
            enumerate_selected_applet_roots, &inactive_roots);
#endif
      } catch (...) {
        std::printf(
            "CHOUPI:HEAP_GC_SKIP owner=%u reason=root_enumeration_failed\r\n",
            static_cast<unsigned>(inactive_context->getAppletID()));
      }
      break;
    }
  }
  (void)jcvm::Heap::reclaimTransientGlobalArrays();
}
#endif

struct RuntimeDiagnostics {
  uint16_t status = 0x6f00;
  uint16_t exception_id = 0;
  uint16_t stack_depth = 0;
  uint8_t opcode = 0;
  uint32_t pc = 0;
  uint32_t opcode_history = 0;
  uint32_t depth_history = 0;
  bool field_bounds_failure = false;
  uint16_t field_index = 0;
  uint16_t field_count = 0;
  uint16_t field_package = 0;
  uint16_t field_class = 0;
  uint16_t field_value_ref = 0;
  bool field_cp_lookup_recorded = false;
  bool field_cp_lookup_succeeded = false;
  uint16_t field_cp_index = 0;
  uint8_t field_cp_package = 0;
  uint8_t field_cp_token = 0;
  uint8_t field_cp_exception = 0;
  bool field_heap_ref_recorded = false;
  uint16_t field_heap_ref = 0;
  uint16_t field_heap_offset = 0;
  bool field_heap_is_array = false;
  bool persistent_restore_failure = false;
  uint16_t persistent_restore_index = 0;
  uint16_t persistent_restore_package = 0;
  uint16_t persistent_restore_class = 0;
  uint8_t persistent_restore_type = 0;
  uint8_t persistent_restore_stage = 0;
  uint8_t persistent_restore_exception = 0;
  uint8_t putfield_stage = 0;
  uint16_t putfield_stage_object = 0;
  uint16_t putfield_stage_value = 0;
  uint8_t putfield_stage_exception = 0;
  uint8_t field_write_stage = 0;
  uint16_t field_write_package = 0;
  uint16_t field_write_class = 0;
  uint16_t field_write_index = 0;
  uint16_t field_write_value = 0;
  uint16_t field_write_count = 0;
  uint8_t field_write_flags = 0;
  uint8_t field_write_exception = 0;
  bool array_write_failure = false;
  uint8_t array_write_stage = 0;
  uint16_t array_write_index = 0;
  uint16_t array_write_size = 0;
  uint16_t array_write_ref = 0;
  uint8_t array_write_type = 0;
  uint8_t array_write_exception_id = 0;
};
RuntimeDiagnostics runtime_diagnostics;
}

#ifdef __cplusplus
extern "C" {
#endif

#ifdef PC_VERSION
uint8_t choupi_pc_initialize_storage(void);
#endif

uint16_t choupi_vm_last_native_method_index();

void choupi_ccid_task(void);

void choupi_vm_runtime_debug_state(uint16_t *status, uint16_t *exception_id,
                                   uint8_t *opcode, uint32_t *pc,
                                   uint16_t *stack_depth,
                                   uint32_t *opcode_history,
                                   uint32_t *depth_history,
                                   uint8_t *field_bounds_failure,
                                   uint16_t *field_index,
                                   uint16_t *field_count,
                                   uint16_t *field_package,
                                   uint16_t *field_class,
                                   uint16_t *field_value_ref) {
  if (status != nullptr) *status = runtime_diagnostics.status;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.exception_id;
  if (opcode != nullptr) *opcode = runtime_diagnostics.opcode;
  if (pc != nullptr) *pc = runtime_diagnostics.pc;
  if (stack_depth != nullptr) *stack_depth = runtime_diagnostics.stack_depth;
  if (opcode_history != nullptr)
    *opcode_history = runtime_diagnostics.opcode_history;
  if (depth_history != nullptr)
    *depth_history = runtime_diagnostics.depth_history;
  if (field_bounds_failure != nullptr)
    *field_bounds_failure = runtime_diagnostics.field_bounds_failure ? 1 : 0;
  if (field_index != nullptr) *field_index = runtime_diagnostics.field_index;
  if (field_count != nullptr) *field_count = runtime_diagnostics.field_count;
  if (field_package != nullptr)
    *field_package = runtime_diagnostics.field_package;
  if (field_class != nullptr) *field_class = runtime_diagnostics.field_class;
  if (field_value_ref != nullptr)
    *field_value_ref = runtime_diagnostics.field_value_ref;
}

void choupi_vm_record_field_bounds(uint16_t field_index, uint16_t field_count,
                                  uint16_t field_package,
                                  uint16_t field_class,
                                  uint16_t field_value_ref) {
  runtime_diagnostics.field_bounds_failure = true;
  runtime_diagnostics.field_index = field_index;
  runtime_diagnostics.field_count = field_count;
  runtime_diagnostics.field_package = field_package;
  runtime_diagnostics.field_class = field_class;
  runtime_diagnostics.field_value_ref = field_value_ref;
}

void choupi_vm_record_field_cp_lookup(uint8_t package_id, uint16_t cp_index,
                                      uint8_t succeeded, uint8_t token,
                                      uint8_t exception_id) {
  runtime_diagnostics.field_cp_lookup_recorded = true;
  runtime_diagnostics.field_cp_lookup_succeeded = succeeded != 0;
  runtime_diagnostics.field_cp_package = package_id;
  runtime_diagnostics.field_cp_index = cp_index;
  runtime_diagnostics.field_cp_token = token;
  runtime_diagnostics.field_cp_exception = exception_id;
}

void choupi_vm_field_cp_state(uint8_t *recorded, uint8_t *succeeded,
                              uint8_t *package_id, uint16_t *cp_index,
                              uint8_t *token, uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.field_cp_lookup_recorded ? 1 : 0;
  if (succeeded != nullptr)
    *succeeded = runtime_diagnostics.field_cp_lookup_succeeded ? 1 : 0;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.field_cp_package;
  if (cp_index != nullptr) *cp_index = runtime_diagnostics.field_cp_index;
  if (token != nullptr) *token = runtime_diagnostics.field_cp_token;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.field_cp_exception;
}

void choupi_vm_record_field_heap_ref(uint16_t objectref, uint16_t offset,
                                     uint8_t is_array) {
  runtime_diagnostics.field_heap_ref_recorded = true;
  runtime_diagnostics.field_heap_ref = objectref;
  runtime_diagnostics.field_heap_offset = offset;
  runtime_diagnostics.field_heap_is_array = is_array != 0;
}

void choupi_vm_field_heap_ref_state(uint8_t *recorded, uint16_t *objectref,
                                    uint16_t *offset, uint8_t *is_array) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.field_heap_ref_recorded ? 1 : 0;
  if (objectref != nullptr) *objectref = runtime_diagnostics.field_heap_ref;
  if (offset != nullptr) *offset = runtime_diagnostics.field_heap_offset;
  if (is_array != nullptr)
    *is_array = runtime_diagnostics.field_heap_is_array ? 1 : 0;
}

void choupi_vm_record_persistent_restore_failure(uint16_t package_id,
                                                uint16_t class_index,
                                                uint16_t field_index,
                                                uint8_t field_type,
                                                uint8_t stage,
                                                uint8_t exception_id) {
  runtime_diagnostics.persistent_restore_failure = true;
  runtime_diagnostics.persistent_restore_package = package_id;
  runtime_diagnostics.persistent_restore_class = class_index;
  runtime_diagnostics.persistent_restore_index = field_index;
  runtime_diagnostics.persistent_restore_type = field_type;
  runtime_diagnostics.persistent_restore_stage = stage;
  runtime_diagnostics.persistent_restore_exception = exception_id;
}

void choupi_vm_persistent_restore_failure_state(
    uint8_t *recorded, uint16_t *package_id, uint16_t *class_index,
    uint16_t *field_index, uint8_t *field_type, uint8_t *stage,
    uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.persistent_restore_failure ? 1 : 0;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.persistent_restore_package;
  if (class_index != nullptr)
    *class_index = runtime_diagnostics.persistent_restore_class;
  if (field_index != nullptr)
    *field_index = runtime_diagnostics.persistent_restore_index;
  if (field_type != nullptr)
    *field_type = runtime_diagnostics.persistent_restore_type;
  if (stage != nullptr) *stage = runtime_diagnostics.persistent_restore_stage;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.persistent_restore_exception;
}

void choupi_vm_record_putfield_stage(uint8_t stage, uint16_t objectref,
                                     uint16_t value, uint8_t exception_id) {
  runtime_diagnostics.putfield_stage = stage;
  runtime_diagnostics.putfield_stage_object = objectref;
  runtime_diagnostics.putfield_stage_value = value;
  runtime_diagnostics.putfield_stage_exception = exception_id;
}

void choupi_vm_putfield_stage_state(uint8_t *stage, uint16_t *objectref,
                                    uint16_t *value, uint8_t *exception_id) {
  if (stage != nullptr) *stage = runtime_diagnostics.putfield_stage;
  if (objectref != nullptr)
    *objectref = runtime_diagnostics.putfield_stage_object;
  if (value != nullptr) *value = runtime_diagnostics.putfield_stage_value;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.putfield_stage_exception;
}

void choupi_vm_record_field_write(uint8_t stage, uint16_t package_id,
                                  uint16_t class_index, uint16_t field_index,
                                  uint16_t value, uint16_t field_count,
                                  uint8_t flags, uint8_t exception_id) {
  runtime_diagnostics.field_write_stage = stage;
  runtime_diagnostics.field_write_package = package_id;
  runtime_diagnostics.field_write_class = class_index;
  runtime_diagnostics.field_write_index = field_index;
  runtime_diagnostics.field_write_value = value;
  runtime_diagnostics.field_write_count = field_count;
  runtime_diagnostics.field_write_flags = flags;
  runtime_diagnostics.field_write_exception = exception_id;
}

void choupi_vm_field_write_state(uint8_t *stage, uint16_t *package_id,
                                 uint16_t *class_index, uint16_t *field_index,
                                 uint16_t *value, uint16_t *field_count,
                                 uint8_t *flags, uint8_t *exception_id) {
  if (stage != nullptr) *stage = runtime_diagnostics.field_write_stage;
  if (package_id != nullptr)
    *package_id = runtime_diagnostics.field_write_package;
  if (class_index != nullptr)
    *class_index = runtime_diagnostics.field_write_class;
  if (field_index != nullptr)
    *field_index = runtime_diagnostics.field_write_index;
  if (value != nullptr) *value = runtime_diagnostics.field_write_value;
  if (field_count != nullptr)
    *field_count = runtime_diagnostics.field_write_count;
  if (flags != nullptr) *flags = runtime_diagnostics.field_write_flags;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.field_write_exception;
}

void choupi_vm_record_array_write_failure(uint8_t stage, uint16_t index,
                                          uint16_t size, uint16_t ref,
                                          uint8_t type,
                                          uint8_t exception_id) {
  if (runtime_diagnostics.array_write_failure) return;
  runtime_diagnostics.array_write_failure = true;
  runtime_diagnostics.array_write_stage = stage;
  runtime_diagnostics.array_write_index = index;
  runtime_diagnostics.array_write_size = size;
  runtime_diagnostics.array_write_ref = ref;
  runtime_diagnostics.array_write_type = type;
  runtime_diagnostics.array_write_exception_id = exception_id;
}

void choupi_vm_array_write_failure_state(uint8_t *recorded, uint8_t *stage,
                                         uint16_t *index, uint16_t *size,
                                         uint16_t *ref, uint8_t *type,
                                         uint8_t *exception_id) {
  if (recorded != nullptr)
    *recorded = runtime_diagnostics.array_write_failure ? 1 : 0;
  if (stage != nullptr) *stage = runtime_diagnostics.array_write_stage;
  if (index != nullptr) *index = runtime_diagnostics.array_write_index;
  if (size != nullptr) *size = runtime_diagnostics.array_write_size;
  if (ref != nullptr) *ref = runtime_diagnostics.array_write_ref;
  if (type != nullptr) *type = runtime_diagnostics.array_write_type;
  if (exception_id != nullptr)
    *exception_id = runtime_diagnostics.array_write_exception_id;
}

uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                 uint16_t class_index, const uint8_t *aid,
                                 uint8_t aid_length) {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return 1;
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  try {
    uint8_t applet_owner = 0;
    if (!cap_store_applet_owner(aid, aid_length, applet_owner)) return 1;

    const auto restore_instance = [&](jcvm::Context &context) -> uint8_t {
      if (!context.activateAppletInstance(applet_owner, aid, aid_length)) {
        return 2;
      }
      if (!context.hasRestoredAppletInstance() &&
          (!context.restoreAppletInstance(
               static_cast<jcvm::jpackage_ID_t>(package_id),
               static_cast<jcvm::jclass_index_t>(class_index)) ||
           !context.hasRestoredAppletInstance())) {
        return 1;
      }
      try {
        const auto applet = context.getHeap().getInstance(
            context.getRestoredAppletRef());
        if (applet->getPackageID() != package_id ||
            applet->getClassIndex() != class_index) {
          return 1;
        }
      } catch (...) {
        return 1;
      }
      return 0;
    };

    jcvm::Context *package_context = jcvm::findPackageContext(
        selected_applet_context.get(), inactive_applet_contexts,
        static_cast<jcvm::jpackage_ID_t>(package_id));
    if (package_context != nullptr) {
      if (package_context == selected_applet_context.get() &&
          context_matches_applet(*package_context, applet_owner, aid,
                                 aid_length) &&
          package_context->hasRestoredAppletInstance()) {
        selected_applet_is_active = false;
        return 0;
      }

      std::size_t inactive_index = inactive_applet_contexts.size();
      if (package_context != selected_applet_context.get()) {
        const auto found = std::find_if(
            inactive_applet_contexts.begin(), inactive_applet_contexts.end(),
            [&](const auto &context) { return context.get() == package_context; });
        if (found == inactive_applet_contexts.end()) return 1;
        inactive_index = static_cast<std::size_t>(
            std::distance(inactive_applet_contexts.begin(), found));
        if (selected_applet_context) {
          selected_applet_context->setAppletActive(false);
        }
        std::swap(selected_applet_context,
                  inactive_applet_contexts[inactive_index]);
      } else {
        uint8_t previous_aid[16]{};
        uint8_t previous_aid_length = 0;
        const uint8_t previous_owner =
            static_cast<uint8_t>(package_context->getAppletID());
        const bool have_previous = package_context->copyAppletAID(
            previous_aid, previous_aid_length);
        package_context->setAppletActive(false);
        const uint8_t status = restore_instance(*package_context);
        if (status != 0) {
          if (have_previous) {
            (void)package_context->activateAppletInstance(
                previous_owner, previous_aid, previous_aid_length);
          }
          return status;
        }
        selected_applet_is_active = false;
        return 0;
      }

      const uint8_t status = restore_instance(*selected_applet_context);
      if (status != 0) {
        std::swap(selected_applet_context,
                  inactive_applet_contexts[inactive_index]);
        return status;
      }
      selected_applet_is_active = false;
      return 0;
    }

    const auto free_slot = std::find_if(
        inactive_applet_contexts.begin(), inactive_applet_contexts.end(),
        [](const auto &context) { return !context; });
    const bool cache_active = selected_applet_context != nullptr;
    if (cache_active && free_slot == inactive_applet_contexts.end()) {
      return 2;
    }

    auto context = std::make_unique<jcvm::Context>(
        applet_owner, static_cast<jcvm::jpackage_ID_t>(package_id));
    if (!context->setAppletAID(aid, aid_length) ||
        !context->restoreAppletInstance(
            static_cast<jcvm::jpackage_ID_t>(package_id),
            static_cast<jcvm::jclass_index_t>(class_index)) ||
        !context->hasRestoredAppletInstance()) {
      return 1;
    }
    const auto applet = context->getHeap().getInstance(
        context->getRestoredAppletRef());
    if (applet->getPackageID() != package_id ||
        applet->getClassIndex() != class_index) {
      return 1;
    }
    if (cache_active) {
      selected_applet_context->setAppletActive(false);
      *free_slot = std::move(selected_applet_context);
    }
    selected_applet_context = std::move(context);
    selected_applet_is_active = false;
    return 0;
  } catch (const std::bad_alloc &) {
    // Preserve the prior context on memory exhaustion.
    return 2;
  } catch (...) {
    // A malformed persistent object or other restore error is not capacity.
    return 1;
  }
}
void choupi_vm_clear_selected_applet(void) {
  // Reader reset implicitly deselects applets without calling deselect().
  // Only abort applet-owned work here; runtime-owned INSTALL transactions are
  // controlled by CapLoadSession and must not be consumed by this guard.
  if (!jcvm::FlashMemory_Handler::abortAppletWriteTransaction()) {
    TRACE_JCVM_ERR("Failed to abort applet transaction during card reset");
  }
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_reselection_pending = false;
  selected_applet_is_reselecting = false;
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  if (selected_applet_context) {
    selected_applet_context->setAppletActive(false);
    selected_applet_context->clearShareableSessionState();
    selected_applet_context->getHeap().clearTransientArrays(
        jcvm::ClearEvent::CLEAR_ON_RESET);
    selected_applet_context->getHeap().clearTransientArrays(
        jcvm::ClearEvent::CLEAR_ON_DESELECT);
  }
  for (auto &context : inactive_applet_contexts) {
    if (context) {
      context->setAppletActive(false);
      context->clearShareableSessionState();
      context->getHeap().clearTransientArrays(
          jcvm::ClearEvent::CLEAR_ON_RESET);
      context->getHeap().clearTransientArrays(
          jcvm::ClearEvent::CLEAR_ON_DESELECT);
    }
  }
  selected_applet_is_active = false;
}

uint8_t choupi_vm_is_applet_selected(void) {
  return selected_applet_context != nullptr && selected_applet_is_active ? 1
                                                                        : 0;
}

void choupi_vm_forget_deleted_applet_contexts(
    const uint8_t *aid, const uint8_t aid_length,
    const uint8_t package_id, const uint8_t deleting_package) {
  pending_apdu_response.queue(nullptr, 0);
  if (deleting_package != 0) {
    jcvm::Heap::clearGlobalArrayReferencesForPackage(package_id);
    jcvm::Heap::clearForeignArrayReferencesForPackage(package_id);
    jcvm::Heap::clearForeignInstanceReferencesForPackage(package_id);
    if (selected_applet_context) {
      selected_applet_context->forgetShareableProxiesForPackage(package_id);
    }
    for (auto &context : inactive_applet_contexts) {
      if (context) context->forgetShareableProxiesForPackage(package_id);
    }
  }
  selected_applet_reselection_pending = false;
  selected_applet_is_reselecting = false;
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  const auto collect_context = [](jcvm::Context &context) {
    try {
      std::vector<jcvm::jref_t> roots;
      context.appendRootReferences(roots);
      (void)context.getHeap().collectUnreachableFrom(roots);
    } catch (...) {
      // Keep owner facades and heap objects intact if root enumeration fails.
    }
  };
  if (selected_applet_context) {
    if (deleting_package != 0 &&
        selected_applet_context->getAppletPackageID() == package_id) {
      selected_applet_context->setAppletActive(false);
      selected_applet_context.reset();
    } else if (deleting_package == 0 && aid != nullptr &&
               aid_length >= 5 && aid_length <= 16 &&
               selected_applet_context->forgetAppletInstance(aid, aid_length)) {
      selected_applet_context->setAppletActive(false);
      collect_context(*selected_applet_context);
    }
  }
  for (auto &context : inactive_applet_contexts) {
    if (!context) continue;
    if (deleting_package != 0 &&
        context->getAppletPackageID() == package_id) {
      context.reset();
    } else if (deleting_package == 0 && aid != nullptr && aid_length >= 5 &&
               aid_length <= 16 &&
               context->forgetAppletInstance(aid, aid_length)) {
      context->setAppletActive(false);
      collect_context(*context);
    }
  }
  // Canonical AID objects live in the process-global heap and may still be
  // referenced by another applet context. Keep them alive across DELETE;
  // freeing one here can leave an unrelated context with a dangling handle.
  selected_applet_is_active = false;
}

uint16_t choupi_vm_invoke_applet_method(uint16_t method_offset,
                                       uint8_t method_package,
                                       const jcvm::jref_t *initial_references,
                                       uint8_t initial_reference_count) {
  if (!selected_applet_context || initial_references == nullptr ||
      initial_reference_count == 0) {
    return 0x6f00;
  }
  jcvm::Interpretor interpretor(*selected_applet_context, method_offset,
                                static_cast<jcvm::jpackage_ID_t>(method_package),
                                initial_references,
                                initial_reference_count);
  return interpretor.run();
}

jcvm::Context *choupi_vm_find_or_restore_applet_context(
    const uint8_t *aid, const uint8_t aid_length) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return nullptr;

  uint8_t owner = 0;
  uint8_t package_id = 0;
  uint8_t applet_aid[16]{};
  uint8_t applet_aid_length = 0;
  uint16_t class_index = 0;
  if (!cap_store_find_instance(aid, aid_length, package_id, applet_aid,
                               applet_aid_length, class_index) ||
      !cap_store_applet_owner(aid, aid_length, owner) ||
      !jcvm::FlashMemory_Handler::isPackageExist(package_id)) {
    return nullptr;
  }

  if (auto *package_context = jcvm::findPackageContext(
          selected_applet_context.get(), inactive_applet_contexts,
          static_cast<jcvm::jpackage_ID_t>(package_id))) {
    uint8_t previous_aid[16]{};
    uint8_t previous_aid_length = 0;
    const uint8_t previous_owner =
        static_cast<uint8_t>(package_context->getAppletID());
    const bool have_previous =
        package_context->copyAppletAID(previous_aid, previous_aid_length);
    if (!package_context->activateAppletInstance(owner, applet_aid,
                                                  applet_aid_length)) {
      return nullptr;
    }
    if (!package_context->hasRestoredAppletInstance() &&
        (!package_context->restoreAppletInstance(
             static_cast<jcvm::jpackage_ID_t>(package_id),
             static_cast<jcvm::jclass_index_t>(class_index)) ||
         !package_context->hasRestoredAppletInstance())) {
      if (have_previous) {
        (void)package_context->activateAppletInstance(
            previous_owner, previous_aid, previous_aid_length);
      }
      return nullptr;
    }
    try {
      const auto applet = package_context->getHeap().getInstance(
          package_context->getRestoredAppletRef());
      if (applet->getPackageID() != package_id ||
          applet->getClassIndex() != class_index) {
        if (have_previous) {
          (void)package_context->activateAppletInstance(
              previous_owner, previous_aid, previous_aid_length);
        }
        return nullptr;
      }
    } catch (...) {
      if (have_previous) {
        (void)package_context->activateAppletInstance(
            previous_owner, previous_aid, previous_aid_length);
      }
      return nullptr;
    }
    return package_context;
  }

  auto free_slot = std::find_if(
      inactive_applet_contexts.begin(), inactive_applet_contexts.end(),
      [](const auto &context) { return !context; });
  if (free_slot == inactive_applet_contexts.end()) return nullptr;

  try {
    auto context = std::make_unique<jcvm::Context>(
        owner, static_cast<jcvm::jpackage_ID_t>(package_id));
    if (!context->setAppletAID(applet_aid, applet_aid_length) ||
        !context->restoreAppletInstance(
            static_cast<jcvm::jpackage_ID_t>(package_id),
            static_cast<jcvm::jclass_index_t>(class_index)) ||
        !context->hasRestoredAppletInstance()) {
      return nullptr;
    }
    *free_slot = std::move(context);
    return free_slot->get();
  } catch (...) {
    return nullptr;
  }
}
jcvm::jref_t choupi_vm_get_applet_shareable_interface_object(
    jcvm::Context &client_context, const jcvm::jref_t server_aid_reference,
    const int8_t parameter) {
  uint8_t server_aid[16]{};
  uint8_t server_aid_length = 0;
  uint8_t aid_package_id = 0;
  uint16_t aid_class_index = 0;
  if (!jcvm::Heap::describeGlobalAIDObject(
          server_aid_reference, server_aid, server_aid_length, aid_package_id,
          aid_class_index)) {
    TRACE_JCVM_DEBUG("SIO_LOOKUP invalid server AID reference");
    return jcvm::jref_t{};
  }

  // The API argument identifies the service applet. Its override receives
  // the identity of the requesting applet, which must come from the active
  // client Context rather than from that server-AID argument.
  uint8_t client_aid[16]{};
  uint8_t client_aid_length = 0;
  if (!client_context.copyAppletAID(client_aid, client_aid_length)) {
    TRACE_JCVM_DEBUG("SIO_LOOKUP missing client AID context=%u",
                     static_cast<unsigned>(client_context.getAppletID()));
    return jcvm::jref_t{};
  }
  const uint8_t client_owner =
      static_cast<uint8_t>(client_context.getAppletID());
  bool client_state_restored = false;
  const auto restore_client_state = [&]() {
    if (!client_state_restored) {
      client_state_restored = true;
      (void)client_context.activateAppletInstance(
          client_owner, client_aid, client_aid_length);
    }
  };
  struct ClientStateRestore {
    decltype(restore_client_state) &restore;
    ~ClientStateRestore() { this->restore(); }
  } client_state_restore{restore_client_state};
  TRACE_JCVM_DEBUG("SIO_LOOKUP client=%02X%02X%02X%02X%02X server=%02X%02X%02X%02X%02X",
                   client_aid[0], client_aid[1], client_aid[2], client_aid[3],
                   client_aid[4], server_aid[0], server_aid[1], server_aid[2],
                   server_aid[3], server_aid[4]);
  jcvm::jref_t client_aid_reference = jcvm::Heap::findGlobalAIDObject(
      client_aid, client_aid_length);
  if (client_aid_reference.isNullPointer()) {
    client_aid_reference = jcvm::Heap::addGlobalAIDObject(
        client_aid, client_aid_length,
        static_cast<jcvm::jpackage_ID_t>(aid_package_id),
        static_cast<jcvm::jclass_index_t>(aid_class_index));
  }
  client_context.setAppletAIDObjectRef(client_aid_reference);

  jcvm::Context *server_context =
      choupi_vm_find_or_restore_applet_context(server_aid, server_aid_length);
  if (server_context == nullptr) return jcvm::jref_t{};
  const jcvm::jref_t applet_reference = server_context->getRestoredAppletRef();
  if (applet_reference.isNullPointer() || applet_reference.isArray()) {
    return jcvm::jref_t{};
  }

  const auto applet = server_context->getHeap().getInstance(applet_reference);
  jcvm::Package applet_package(applet->getPackageID());
  jcvm::Import_Handler imports(applet_package);
  const auto *import_component = applet_package.getCap().getImport();
  if (import_component == nullptr) return jcvm::jref_t{};

  constexpr uint8_t framework_aid[] = {0xa0, 0x00, 0x00, 0x00,
                                       0x62, 0x01, 0x01};
  uint8_t framework_import_token = 0xff;
  uint8_t applet_class_token = 0xff;
  for (uint8_t import_index = 0; import_index < import_component->count;
       ++import_index) {
    const auto *package_info = imports.getPackageAID(import_index);
    if (package_info == nullptr || package_info->AID_length !=
                                       sizeof(framework_aid) ||
        std::memcmp(package_info->AID, framework_aid,
                    sizeof(framework_aid)) != 0) {
      continue;
    }
    for (uint8_t class_token = 0; class_token < 0x80; ++class_token) {
      if (jcvm::remap_imported_class_token(*package_info, class_token) == 13) {
        framework_import_token = import_index;
        applet_class_token = class_token;
        break;
      }
    }
    break;
  }
  if (framework_import_token == 0xff || applet_class_token == 0xff) {
    return jcvm::jref_t{};
  }

  jcvm::jc_cap_virtual_method_ref_info method_ref{};
  method_ref.class_ref.external_classref.package_token =
      static_cast<uint8_t>(framework_import_token | 0x80u);
  method_ref.class_ref.external_classref.class_token = applet_class_token;
  // This callback originates in the embedded ROM. The concrete applet's
  // method table uses the imported CAP token, so request the embedded Applet
  // export token and ask Class_Handler to reverse-map it for the override.
  method_ref.token = 6;
  const auto method = jcvm::Class_Handler(applet_package).getMethodOffset(
      method_ref, applet->getPackageID(), applet->getClassIndex(), true);

  const jcvm::InterpretorInitialLocal initial_locals[] = {
      {jcvm::InterpretorLocalType::Reference, applet_reference.compact()},
      {jcvm::InterpretorLocalType::Reference,
       client_aid_reference.compact()},
      {jcvm::InterpretorLocalType::Byte,
       static_cast<uint8_t>(parameter)}};
  jcvm::Interpretor interpretor(
      *server_context, method.second, method.first.getPackageID(),
      initial_locals, sizeof(initial_locals) / sizeof(initial_locals[0]), true);
  const uint16_t lookup_status = interpretor.run();
  TRACE_JCVM_DEBUG("SIO_LOOKUP override status=%04X",
                   static_cast<unsigned>(lookup_status));
  if (lookup_status != 0x9000) return jcvm::jref_t{};

  jcvm::jref_t server_reference;
  if (server_context == &client_context) {
    server_reference = server_context->getStack().pop_Reference();
  } else if (!server_context->getStack().getRootReferenceReturnValue(
                 server_reference)) {
    return jcvm::jref_t{};
  }
  if (server_reference.isNullPointer()) {
    TRACE_JCVM_DEBUG("SIO_LOOKUP override returned null");
    return jcvm::jref_t{};
  }
  if (server_reference.isArray() || server_reference.isGlobalInstance()) {
    throw jcvm::Exceptions::SecurityException;
  }
  const auto server_object =
      server_context->getHeap().getInstance(server_reference);
  const uint8_t server_owner =
      static_cast<uint8_t>(server_context->getAppletID());
  restore_client_state();
  const jcvm::jref_t existing = client_context.findShareableProxy(
      server_aid, server_aid_length, server_reference);
  if (!existing.isNullPointer()) return existing;

  if (server_context == &client_context &&
      !server_context->activateAppletInstance(server_owner, server_aid,
                                               server_aid_length)) {
    throw jcvm::Exceptions::SecurityException;
  }
  jcvm::ShareableObjectRootLease server_root(*server_context,
                                              server_reference);
  if (!server_root.retain()) {
    restore_client_state();
    throw jcvm::Exceptions::SecurityException;
  }
  restore_client_state();
  const jcvm::jref_t proxy = client_context.getHeap().addInstance(
      server_object->getPackageID(), server_object->getClassIndex());
  if (!client_context.registerShareableProxy(
          proxy, server_aid, server_aid_length, server_reference,
          server_object->getPackageID())) {
    throw jcvm::Exceptions::SecurityException;
  }
  server_root.commit();
  TRACE_JCVM_DEBUG("SIO_LOOKUP created proxy=%04X server_ref=%04X",
                   static_cast<unsigned>(proxy.compact()),
                   static_cast<unsigned>(server_reference.compact()));
  return proxy;
}

namespace {
uint16_t invoke_selected_applet_method(const uint8_t method_token,
                                       uint32_t *return_value) {
  if (!selected_applet_context) {
    return 0x6999;
  }
  selected_applet_context->resetSensitiveResult();
  try {
    jcvm::Context &context = *selected_applet_context;
    const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
    if (applet_ref.isNullPointer() || applet_ref.isArray()) {
      return 0x6f00;
    }
    const auto applet = context.getHeap().getInstance(applet_ref);
    jcvm::Package package(applet->getPackageID());
    jcvm::ConstantPool_Handler cp_handler(package);
    jcvm::jc_cap_virtual_method_ref_info method_ref;
    method_ref.class_ref = cp_handler.getClassRefFromClassIndex(
        applet->getClassIndex());
    method_ref.token = method_token;
    const auto method = jcvm::Class_Handler(package).getMethodOffset(method_ref);
    const jcvm::jref_t initial_references[] = {applet_ref};
    jcvm::Interpretor interpretor(
        context, method.second, method.first.getPackageID(),
        initial_references, 1);
    const uint16_t status = interpretor.run();
    if (status == 0x9000 && return_value != nullptr) {
      const jcvm::Stack &stack = context.getStack();
      if (!stack.hasRootReturnValue()) {
        return 0x6f00;
      }
      *return_value = stack.getRootReturnValue();
    }
    return status;
  } catch (...) {
    return 0x6f00;
  }
}
} // namespace

uint16_t choupi_vm_select_applet(void) {
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_select_process_pending = false;
  uint32_t selected = 0;
  selected_applet_reselection_process_pending =
      selected_applet_reselection_pending;
  selected_applet_is_reselecting =
      selected_applet_reselection_process_pending;
  selected_applet_reselection_pending = false;
  selected_applet_is_selecting = true;
  selected_applet_context->setAppletActive(true);
  uint16_t status = invoke_selected_applet_method(6, &selected);
  const bool transaction_left_open =
      jcvm::FlashMemory_Handler::appletWriteTransactionActive();
  if (transaction_left_open &&
      !jcvm::FlashMemory_Handler::abortAppletWriteTransaction()) {
    status = 0x6f00;
  } else if (transaction_left_open && status == 0x9000) {
    status = 0x6999;
  }
  selected_applet_is_selecting = false;
  selected_applet_is_reselecting = false;
  if (status != 0x9000) {
    selected_applet_context->setAppletActive(false);
    selected_applet_is_active = false;
    selected_applet_reselection_process_pending = false;
    return status;
  }
  if (selected == 0) {
    selected_applet_context->setAppletActive(false);
    selected_applet_is_active = false;
    selected_applet_reselection_process_pending = false;
    return 0x6999;
  }
  selected_applet_is_active = true;
  selected_applet_select_process_pending = true;
  return 0x9000;
}

extern "C" uint16_t choupi_vm_deselect_applet(
    const uint8_t selecting_same_applet) {
  pending_apdu_response.queue(nullptr, 0);
  selected_applet_reselection_pending = false;
  selected_applet_is_reselecting = false;
  selected_applet_is_selecting = false;
  selected_applet_select_process_pending = false;
  selected_applet_reselection_process_pending = false;
  if (!selected_applet_context || !selected_applet_is_active) {
    selected_applet_is_active = false;
    return 0x9000;
  }
  uint16_t status = invoke_selected_applet_method(4, nullptr);
  const bool transaction_left_open =
      jcvm::FlashMemory_Handler::appletWriteTransactionActive();
  if (transaction_left_open &&
      !jcvm::FlashMemory_Handler::abortAppletWriteTransaction()) {
    status = 0x6f00;
  } else if (transaction_left_open && status == 0x9000) {
    status = 0x6f00;
  }
  selected_applet_context->setAppletActive(false);
  selected_applet_context->getHeap().clearTransientArrays(
      jcvm::ClearEvent::CLEAR_ON_DESELECT);
  selected_applet_is_active = false;
  selected_applet_reselection_pending = selecting_same_applet != 0 &&
                                         status == 0x9000;
  return status;
}

extern "C" uint8_t choupi_vm_is_reselecting_applet(void) {
  return selected_applet_is_reselecting ? 1 : 0;
}

extern "C" uint8_t choupi_vm_is_selecting_applet(void) {
  return selected_applet_is_selecting ? 1 : 0;
}

uint16_t choupi_vm_process_applet(const uint8_t *command,
                                 uint16_t command_length,
                                 uint8_t *response,
                                 uint16_t response_capacity,
                                 uint16_t *response_length) {
#ifdef PC_VERSION
  const char *apdu_trace_setting = std::getenv("CHOUPI_VM_TRACE_APDU");
  const bool apdu_trace = apdu_trace_setting != nullptr &&
                          std::strcmp(apdu_trace_setting, "1") == 0;
  if (apdu_trace) {
    std::fprintf(stderr,
                 "CHOUPI:APDU_PROCESS stage=begin len=%u ins=%02X\n",
                 static_cast<unsigned>(command_length),
                 command != nullptr && command_length > 1 ? command[1] : 0);
  }
#endif
#ifndef PC_VERSION
  std::printf("CHOUPI:APDU_PROCESS stage=begin\r\n");
  std::fflush(stdout);
#endif
  SelectingAppletProcessScope selection_scope(
      command, command_length,
      selected_applet_context ? selected_applet_context.get() : nullptr);
  if (selected_applet_context) {
    selected_applet_context->resetSensitiveResult();
  }
  if (response_length != nullptr) {
    *response_length = 0;
  }
  if (response == nullptr || response_length == nullptr ||
      response_capacity < 2) {
    return 0x6f00;
  }

  uint16_t status = 0x6f00;
  uint16_t data_length = 0;
  try {
    if (!selected_applet_context || !selected_applet_is_active) {
#ifndef PC_VERSION
      std::printf("CHOUPI:APDU_PROCESS stage=no_active_applet\r\n");
      std::fflush(stdout);
#endif
      return 0xffff; // CCID maps the no-selection sentinel to its default SW.
    }
    if (command == nullptr || command_length < 4) {
      status = 0x6700;
      throw status;
    }

    jcvm::Context &context = *selected_applet_context;
    jcvm::Heap &heap = context.getHeap();
    pico2_jcvm::ShortApduCommand parsed_command;
    if (!pico2_jcvm::parse_short_apdu(command, command_length,
                                      parsed_command)) {
      pending_apdu_response.queue(nullptr, 0);
      status = 0x6700;
      throw status;
    }

    const bool get_response = command[0] == 0x00 && command[1] == 0xc0;
    if (get_response) {
      if (command[2] != 0 || command[3] != 0) {
        status = 0x6a86;
      } else if (!parsed_command.has_expected_response_length) {
        status = 0x6700;
      } else if (!pending_apdu_response.has_pending()) {
        status = 0x6a86;
      } else {
        data_length = pending_apdu_response.take(
            parsed_command.expected_response_length, response,
            static_cast<uint16_t>(response_capacity - 2), status);
      }
      response[data_length] = static_cast<uint8_t>(status >> 8);
      response[data_length + 1] = static_cast<uint8_t>(status);
      *response_length = static_cast<uint16_t>(data_length + 2);
      return status;
    }
    pending_apdu_response.queue(nullptr, 0);

    const uint16_t lc = parsed_command.incoming_length;

    jcvm::jref_t buffer_ref = context.getAPDUBufferRef();
    auto buffer = context.getAPDUJavaBuffer();
    if (!buffer && buffer_ref.isNullPointer()) {
      buffer_ref = heap.addArray(260, jcvm::JAVA_ARRAY_T_BYTE);
    }
    if (!buffer) {
      buffer = heap.getArray(buffer_ref);
    }
    if (!buffer || buffer->getType() != jcvm::JAVA_ARRAY_T_BYTE ||
        buffer->size() < static_cast<uint16_t>(5 + lc)) {
      status = 0x6f00;
      throw status;
    }
    for (uint16_t index = 0; index < buffer->size(); ++index) {
      buffer->setByteEntry(index, 0);
    }
    for (uint16_t index = 0; index < 4; ++index) {
      buffer->setByteEntry(index, static_cast<jcvm::jbyte_t>(command[index]));
    }
    // For case 2S P3 is Le; for case 3S/4S it is Lc.
    buffer->setByteEntry(
        4, static_cast<jcvm::jbyte_t>(command_length > 4 ? command[4] : 0));
    for (uint16_t index = 0; index < lc; ++index) {
      buffer->setByteEntry(static_cast<uint16_t>(5 + index),
                           static_cast<jcvm::jbyte_t>(command[5 + index]));
    }
    context.setAPDUBufferRef(buffer_ref, lc,
                             parsed_command.expected_response_length,
                             parsed_command.has_expected_response_length);
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=buffer_ready\n");
    }
#endif

    jcvm::jref_t apdu_ref = context.getAPDUObjectRef();
    if (apdu_ref.isNullPointer()) {
      const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
      if (applet_ref.isNullPointer() || applet_ref.isArray()) {
        status = 0x6f00;
        throw status;
      }
      const auto applet = heap.getInstance(applet_ref);
      jcvm::Package applet_package(applet->getPackageID());
      const auto applet_cap = applet_package.getCap();
      const auto *applet_package_info =
          applet_cap.getHeader()->package.AID_length
              ? &applet_cap.getHeader()->package
              : nullptr;
      if (applet_package_info == nullptr) {
        status = 0x6f00;
        throw status;
      }

      constexpr uint8_t framework_aid[] = {0xa0, 0x00, 0x00, 0x00,
                                           0x62, 0x01, 0x01};
      jcvm::Import_Handler import_handler(applet_package);
      const auto *imports = applet_package.getCap().getImport();
      const jcvm::jc_cap_package_info *framework_info = nullptr;
      for (uint8_t index = 0; index < imports->count; ++index) {
        const auto *candidate = import_handler.getPackageAID(index);
        if (candidate->AID_length == sizeof(framework_aid) &&
            std::memcmp(candidate->AID, framework_aid,
                        sizeof(framework_aid)) == 0) {
          framework_info = candidate;
          break;
        }
      }
      if (framework_info == nullptr) {
        status = 0x6f00;
        throw status;
      }
      const auto framework_package_id =
          import_handler.getPackageIndex(framework_info);
      jcvm::Package framework_package(framework_package_id);
      const uint8_t apdu_export_token =
          jcvm::remap_imported_class_token(*framework_info, 10);
      const uint16_t apdu_class_index = HTONS(
          jcvm::Export_Handler(framework_package)
              .getExportedClassOffset(apdu_export_token));
      apdu_ref = heap.addInstance(framework_package_id, apdu_class_index);
      context.setAPDUObjectRef(apdu_ref);
    }

#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=method_lookup_done\n");
    }
#endif
    const jcvm::jref_t applet_ref = context.getRestoredAppletRef();
    const auto applet = heap.getInstance(applet_ref);
    jcvm::Package applet_package(applet->getPackageID());
    jcvm::ConstantPool_Handler cp_handler(applet_package);
    jcvm::jc_cap_virtual_method_ref_info method_ref;
    method_ref.class_ref = cp_handler.getClassRefFromClassIndex(
        applet->getClassIndex());
    method_ref.token = 7; // Applet.process(APDU)
    const auto method = jcvm::Class_Handler(applet_package).getMethodOffset(
        method_ref, applet->getPackageID(), applet->getClassIndex());
    const jcvm::jref_t initial_references[] = {applet_ref, apdu_ref};
    jcvm::Interpretor interpretor(context, method.second,
                                  method.first.getPackageID(),
                                  initial_references, 2);
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr,
                   "CHOUPI:APDU_PROCESS stage=interpreter_begin package=%u "
                   "method=%u\n",
                   static_cast<unsigned>(method.first.getPackageID()),
                   static_cast<unsigned>(method.second));
    }
#else
    std::printf("CHOUPI:APDU_PROCESS stage=interpreter_begin package=%u method=%u\r\n",
                static_cast<unsigned>(method.first.getPackageID()),
                static_cast<unsigned>(method.second));
    std::fflush(stdout);
#endif
    {
      APDUProcessScope process_scope(context);
      status = interpretor.run();
    }
#ifndef PC_VERSION
    std::printf("CHOUPI:APDU_PROCESS stage=interpreter_end sw=%04X\r\n",
                static_cast<unsigned>(status));
    std::fflush(stdout);
#endif
#ifdef PC_VERSION
    if (apdu_trace) {
      std::fprintf(stderr, "CHOUPI:APDU_PROCESS stage=interpreter_end sw=%04X\n",
                   static_cast<unsigned>(status));
    }
#endif
    if (status != 0x9000) {
      const uint8_t exception_opcode = interpretor.lastExceptionOpcode();
      const uint16_t exception_operand = exception_opcode == 0xFE
          ? choupi_vm_last_native_method_index()
          : interpretor.lastExceptionOperand();
      fprintf(stderr,
                   "VPCD:APPLET_PROCESS status=%04X exception=%u opcode=%02X "
                   "operand=%u package=%u pc=%08X stack=%u "
                   "op_history=%08X depth_history=%08X\n",
                   status, static_cast<unsigned>(interpretor.lastExceptionId()),
                   static_cast<unsigned>(exception_opcode),
                   static_cast<unsigned>(exception_operand),
                   static_cast<unsigned>(context.getCurrentPackageID()),
                   static_cast<unsigned>(interpretor.lastExceptionPc()),
                   static_cast<unsigned>(interpretor.lastExceptionStackDepth()),
                   static_cast<unsigned>(interpretor.lastExceptionOpcodeHistory()),
                   static_cast<unsigned>(interpretor.lastExceptionDepthHistory()));
    }
    if (status == 0x9000) {
      data_length = context.getAPDUOutgoingLength();
      if (static_cast<uint32_t>(data_length) + 2 > response_capacity ||
          context.getAPDUState() == 3 || context.getAPDUState() == 4 ||
          context.getAPDUState() == 5) {
        status = 0x6f00;
        data_length = 0;
      } else {
        for (uint16_t index = 0; index < data_length; ++index) {
          response[index] = context.getAPDUOutgoingByte(index);
        }
      }
    }
    if (status == 0x9000 && parsed_command.has_expected_response_length &&
        data_length > parsed_command.expected_response_length) {
      const uint16_t first_chunk = parsed_command.expected_response_length;
      const uint16_t remaining =
          static_cast<uint16_t>(data_length - first_chunk);
      if (!pending_apdu_response.queue(response + first_chunk, remaining)) {
        status = 0x6f00;
        data_length = 0;
      } else {
        data_length = first_chunk;
        const uint16_t sw2 = remaining >= 256 ? 0 : remaining;
        status = static_cast<uint16_t>(0x6100 | sw2);
      }
    }
  } catch (uint16_t caught_status) {
    status = caught_status;
    data_length = 0;
  } catch (...) {
    status = 0x6f00;
    data_length = 0;
  }

  const bool transaction_left_open =
      jcvm::FlashMemory_Handler::appletWriteTransactionActive();
  if (transaction_left_open) {
    const bool aborted =
        jcvm::FlashMemory_Handler::abortAppletWriteTransaction();
    pending_apdu_response.queue(nullptr, 0);
    data_length = 0;
    if (!aborted || status == 0x9000) {
      // A normal process return with an open transaction is treated as an
      // uncaught exception by Java Card RE 3.0.5.
      status = 0x6f00;
    }
  }

#ifdef PC_VERSION
  // The try scope above owns the interpreter and temporary native references.
  // Run diagnostics only after those objects have been destroyed.
  collect_heap_after_apdu();
#else
  // Keep Java handles stable by collecting only after interpreter/native call
  // scopes have returned and the complete typed root walk has succeeded.
  std::printf("CHOUPI:APDU_PROCESS stage=gc_begin\r\n");
  std::fflush(stdout);
  collect_heap_after_apdu();
  std::printf("CHOUPI:APDU_PROCESS stage=gc_end\r\n");
  std::fflush(stdout);
#endif

  if (static_cast<uint32_t>(data_length) + 2 > response_capacity) {
    status = 0x6f00;
    data_length = 0;
  }
  response[data_length] = static_cast<uint8_t>(status >> 8);
  response[data_length + 1] = static_cast<uint8_t>(status);
  *response_length = static_cast<uint16_t>(data_length + 2);
#ifndef PC_VERSION
  std::printf("CHOUPI:APDU_PROCESS stage=end sw=%04X len=%u\r\n",
              static_cast<unsigned>(status),
              static_cast<unsigned>(*response_length));
  std::fflush(stdout);
#endif
  return status;
}

uint16_t runtime(uint8_t id_package, uint8_t id_class, uint8_t id_method) {
  TRACE_JCVM_DEBUG("Starting JCVM");
#ifdef PC_VERSION
  if (choupi_pc_initialize_storage() != 0) return 0x6f00;
#endif

  jcvm::japplet_ID_t id_applet = 0;

  // TODO: is a static method?
  jcvm::Interpretor interpretor(id_applet,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                id_class, id_method, true);
  const uint16_t status = interpretor.run();
  runtime_diagnostics.status = status;
  runtime_diagnostics.exception_id = interpretor.lastExceptionId();
  runtime_diagnostics.opcode = interpretor.lastExceptionOpcode();
  runtime_diagnostics.pc = interpretor.lastExceptionPc();
  runtime_diagnostics.stack_depth = interpretor.lastExceptionStackDepth();
  runtime_diagnostics.opcode_history =
      interpretor.lastExceptionOpcodeHistory();
  runtime_diagnostics.depth_history =
      interpretor.lastExceptionDepthHistory();
#ifndef PC_VERSION
  if (status != 0x9000) {
    std::printf(
        "CHOUPI:VM_RUNTIME_FAILURE sw=%04X exception=%u opcode=%02X "
        "pc=%08lX stack=%u op_history=%08lX depth_history=%08lX\r\n",
        static_cast<unsigned>(status),
        static_cast<unsigned>(interpretor.lastExceptionId()),
        static_cast<unsigned>(interpretor.lastExceptionOpcode()),
        static_cast<unsigned long>(interpretor.lastExceptionPc()),
        static_cast<unsigned>(interpretor.lastExceptionStackDepth()),
        static_cast<unsigned long>(interpretor.lastExceptionOpcodeHistory()),
        static_cast<unsigned long>(interpretor.lastExceptionDepthHistory()));
    std::fflush(stdout);
  }
#endif
  return status;
}

uint16_t runtime_install_method(uint8_t id_package, uint8_t applet_owner,
                                uint16_t method_offset,
                                const uint8_t *install_data,
                                uint8_t install_length,
                                const uint8_t *instance_aid,
                                uint8_t instance_aid_length,
                                uint8_t *applet_registered,
                                uint16_t *applet_class_index,
                                uint8_t *exception_id,
                                uint8_t *exception_opcode,
                                uint32_t *exception_pc,
                                uint32_t *exception_prefix,
                                uint16_t *exception_operand,
                                uint8_t *exception_stack_depth,
                                uint32_t *exception_opcode_history,
                                uint32_t *exception_depth_history,
                                uint8_t *install_debug_stage,
                                uint32_t *install_debug_context) {
  TRACE_JCVM_DEBUG("Starting CAP install method at offset %u",
                   static_cast<unsigned>(method_offset));
#ifndef PC_VERSION
  choupi_install_stage(0);
  choupi_install_context(0);
#endif
  jcvm::Interpretor interpretor(applet_owner,
                                static_cast<jcvm::jpackage_ID_t>(id_package),
                                method_offset, install_data, install_length);
  interpretor.getCurrentContext().resetSensitiveResult();
  if (!interpretor.getCurrentContext().setAppletAID(instance_aid,
                                                    instance_aid_length)) {
    return 0x6a80;
  }
  const uint16_t status = interpretor.run();
  bool applet_persisted = false;
  uint16_t registered_class_index = 0xffff;
  if (status == 0x9000 && interpretor.installAppletRegistered()) {
    try {
      const jcvm::jref_t applet_ref = interpretor.installAppletReference();
      jcvm::Context &context = interpretor.getCurrentContext();
      const auto applet = context.getHeap().getInstance(applet_ref);
      registered_class_index = applet->getClassIndex();
      const jcvm::fs::Tag instance_tag =
          jcvm::FlashMemory_Handler::getAppletInstanceTag(
              context.getAppletID(), applet->getPackageID(),
              applet->getClassIndex());
      jcvm::FlashMemory_Handler::setPersistentField_Instance(
          instance_tag, *applet, context.getHeap());
      applet_persisted = true;
    } catch (...) {
      TRACE_JCVM_ERR("Failed to persist registered applet instance");
    }
  }
  if (applet_registered != nullptr) {
    *applet_registered = applet_persisted ? 1 : 0;
  }
  if (applet_class_index != nullptr) {
    *applet_class_index = applet_persisted ? registered_class_index : 0xffff;
  }
  if (exception_id != nullptr) {
    *exception_id = interpretor.lastExceptionId();
  }
  if (exception_opcode != nullptr) {
    *exception_opcode = interpretor.lastExceptionOpcode();
  }
  if (exception_pc != nullptr) {
    *exception_pc = interpretor.lastExceptionPc();
  }
  if (exception_prefix != nullptr) {
    *exception_prefix = interpretor.lastExceptionPrefix();
  }
  if (exception_operand != nullptr) {
    *exception_operand = interpretor.lastExceptionOperand();
    if (interpretor.lastExceptionOpcode() == 0xFE) {
      *exception_operand = choupi_vm_last_native_method_index();
    }
  }
  if (exception_stack_depth != nullptr) {
    *exception_stack_depth = interpretor.lastExceptionStackDepth();
  }
  if (exception_opcode_history != nullptr) {
    *exception_opcode_history = interpretor.lastExceptionOpcodeHistory();
  }
  if (exception_depth_history != nullptr) {
    *exception_depth_history = interpretor.lastExceptionDepthHistory();
  }
#ifndef PC_VERSION
  if (install_debug_stage != nullptr) {
    *install_debug_stage = choupi_install_debug_stage();
  }
  if (install_debug_context != nullptr) {
    *install_debug_context = choupi_install_debug_context();
  }
#else
  if (install_debug_stage != nullptr) *install_debug_stage = 0;
  if (install_debug_context != nullptr) *install_debug_context = 0;
#endif
  return applet_persisted || !interpretor.installAppletRegistered()
             ? status
             : 0x6f00;
}

#ifdef __cplusplus
}
#endif
