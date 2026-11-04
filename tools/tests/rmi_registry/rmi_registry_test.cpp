#include "context.hpp"
#include "context_registry.hpp"
#include "applet_owner_allocator.hpp"
#include "jc_bytecodes/shareable_arguments.hpp"
#include "jc_types/jc_array.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace jcvm {
uint32_t FlashMemory_Handler::persistentWriteRollbackGeneration() noexcept {
  return 0;
}
} // namespace jcvm

namespace {
struct VisitedReferences {
  std::array<uint16_t, 40> values{};
  size_t count = 0;
};

void record_reference(const jcvm::jref_t reference, void *opaque) {
  auto &visited = *static_cast<VisitedReferences *>(opaque);
  assert(visited.count < visited.values.size());
  visited.values[visited.count++] = reference.compact();
}

jcvm::jref_t make_reference(const uint16_t offset) {
  jcvm::jref_t reference;
  reference.setAsInstance(true);
  reference.setOffset(offset);
  return reference;
}

bool contains(const VisitedReferences &visited, const jcvm::jref_t reference) {
  for (size_t index = 0; index < visited.count; ++index) {
    if (visited.values[index] == reference.compact()) return true;
  }
  return false;
}

void verify_sensitive_result_cross_context(jcvm::Context &producer,
                                           jcvm::Context &consumer) {
  const auto previous = jcvm::Context::setActiveContext(&producer);
  jcvm::Context::getActiveContext().setSensitiveResultBoolean(true);
  jcvm::Context::setActiveContext(&consumer);
  jcvm::Context::getActiveContext().assertSensitiveResultBoolean(true);

  jcvm::Context::getActiveContext().setSensitiveResultShort(-7);
  jcvm::Context::setActiveContext(&producer);
  jcvm::Context::getActiveContext().assertSensitiveResultShort(-7, 0);

  const auto object = make_reference(7);
  jcvm::Context::getActiveContext().setSensitiveResultObject(object);
  jcvm::Context::setActiveContext(&consumer);
  jcvm::Context::getActiveContext().assertSensitiveResultObject(object);
  VisitedReferences roots;
  jcvm::Context::getActiveContext().visitRootReferences(record_reference,
                                                        &roots);
  assert(contains(roots, object));

  jcvm::Context::getActiveContext().resetSensitiveResult();
  bool reset_rejected = false;
  try {
    jcvm::Context::getActiveContext().assertSensitiveResultObject(object);
  } catch (...) {
    reset_rejected = true;
  }
  assert(reset_rejected);
  jcvm::Context::setActiveContext(previous);
}

void verify_previous_context_aid() {
  jcvm::Context client(2, 1);
  jcvm::Context server(3, 1);
  constexpr uint8_t client_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x01};
  constexpr uint8_t server_aid[] = {0xa0, 0x00, 0x00, 0x01, 0x02};
  assert(client.setAppletAID(client_aid, sizeof(client_aid)));
  assert(server.setAppletAID(server_aid, sizeof(server_aid)));

  jcvm::Context::setActiveContext(&client);
  uint8_t previous_aid[16]{};
  uint8_t previous_aid_length = 0;
  assert(!client.copyPreviousContextAID(previous_aid, previous_aid_length));
  assert(previous_aid_length == 0);

  jcvm::Context::setActiveContext(&server);
  assert(server.copyPreviousContextAID(previous_aid, previous_aid_length));
  assert(previous_aid_length == sizeof(client_aid));
  assert(std::equal(client_aid, client_aid + sizeof(client_aid),
                    previous_aid));

  jcvm::Context::setActiveContext(&client);
  assert(client.copyPreviousContextAID(previous_aid, previous_aid_length));
  assert(previous_aid_length == sizeof(server_aid));
  assert(std::equal(server_aid, server_aid + sizeof(server_aid),
                    previous_aid));

  // Re-entering the same context is not a context switch and must preserve the
  // most recent caller AID.
  jcvm::Context::setActiveContext(&client);
  assert(client.copyPreviousContextAID(previous_aid, previous_aid_length));
  assert(previous_aid_length == sizeof(server_aid));
  assert(std::equal(server_aid, server_aid + sizeof(server_aid),
                    previous_aid));
  jcvm::Context::setActiveContext(nullptr);
}

void verify_retained_applet_context_lookup() {
  jcvm::Context selected(1, 1);
  jcvm::Context cached_a(2, 1);
  jcvm::Context cached_b(3, 1);
  constexpr uint8_t selected_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x01};
  constexpr uint8_t cached_a_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x02};
  constexpr uint8_t cached_b_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x03};
  assert(selected.setAppletAID(selected_aid, sizeof(selected_aid)));
  assert(cached_a.setAppletAID(cached_a_aid, sizeof(cached_a_aid)));
  assert(cached_b.setAppletAID(cached_b_aid, sizeof(cached_b_aid)));
  std::array<std::unique_ptr<jcvm::Context>, 2> inactive{};
  inactive[0] = std::make_unique<jcvm::Context>(std::move(cached_a));
  inactive[1] = std::make_unique<jcvm::Context>(std::move(cached_b));

  assert(jcvm::findAppletContext(&selected, inactive, 1, selected_aid,
                                 sizeof(selected_aid)) == &selected);
  assert(jcvm::findAppletContext(&selected, inactive, 2, cached_a_aid,
                                 sizeof(cached_a_aid)) == inactive[0].get());
  assert(jcvm::findAppletContext(&selected, inactive, 3, cached_b_aid,
                                 sizeof(cached_b_aid)) == inactive[1].get());
  assert(jcvm::findAppletContext(&selected, inactive, 9, cached_b_aid,
                                 sizeof(cached_b_aid)) == nullptr);
  assert(jcvm::findAppletContext(&selected, inactive, 3, cached_b_aid, 4) ==
         nullptr);
}

void verify_package_context_lookup() {
  jcvm::Context selected(1, 10);
  std::array<std::unique_ptr<jcvm::Context>, 3> inactive{};
  inactive[0] = std::make_unique<jcvm::Context>(2, 20);
  inactive[1] = std::make_unique<jcvm::Context>(3, 10);
  inactive[2] = std::make_unique<jcvm::Context>(4, 30);
  assert(jcvm::findPackageContext(&selected, inactive, 10) == &selected);
  assert(jcvm::findPackageContext(&selected, inactive, 20) == inactive[0].get());
  assert(jcvm::findPackageContext(&selected, inactive, 30) == inactive[2].get());
  assert(jcvm::findPackageContext(&selected, inactive, 99) == nullptr);
}

void verify_context_retains_sibling_instance_state() {
  jcvm::Context package_context(7, 4);
  constexpr uint8_t first_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x01};
  constexpr uint8_t second_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x02};
  assert(package_context.setAppletAID(first_aid, sizeof(first_aid)));
  const auto first_root = make_reference(30);
  package_context.setAppletAIDObjectRef(first_root);
  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  assert(package_context.getAppletID() == 8);
  uint8_t active_aid[16]{};
  uint8_t active_aid_length = 0;
  assert(package_context.copyAppletAID(active_aid, active_aid_length));
  assert(active_aid_length == sizeof(second_aid));
  assert(std::equal(second_aid, second_aid + sizeof(second_aid), active_aid));
  assert(!package_context.wasAppletRegistered());
  assert(package_context.getHeap().getOwner() == 8);
  const auto sibling_array_reference =
      package_context.getHeap().addArray(3, jcvm::JAVA_ARRAY_T_BYTE);
  const auto sibling_array =
      package_context.getHeap().getArray(sibling_array_reference);
  sibling_array->setByteEntry(0, 0x5a);
  package_context.getHeap().setStaticReferenceRoot(4, 3, make_reference(35));
  const auto second_root = make_reference(31);
  package_context.setAppletAIDObjectRef(second_root);
  package_context.markAppletRegistered(second_root);

  assert(package_context.hasAppletInstance(7, first_aid, sizeof(first_aid)));
  assert(package_context.activateAppletInstance(7, first_aid,
                                                 sizeof(first_aid)));
  assert(package_context.getAppletID() == 7);
  assert(package_context.copyAppletAID(active_aid, active_aid_length));
  assert(active_aid_length == sizeof(first_aid));
  assert(std::equal(first_aid, first_aid + sizeof(first_aid), active_aid));
  assert(!package_context.wasAppletRegistered());
  assert(package_context.getHeap().getOwner() == 7);
  const auto shared_sibling_array =
      package_context.getHeap().getArray(sibling_array_reference);
  assert(shared_sibling_array == sibling_array);
  assert(shared_sibling_array->getByteEntry(0) == 0x5a);
  assert(shared_sibling_array->getOwner().getOwner() == 8);
  jcvm::jref_t shared_static_reference;
  assert(package_context.getHeap().getStaticReferenceRoot(
      4, 3, shared_static_reference));
  assert(shared_static_reference == make_reference(35));

  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  assert(package_context.wasAppletRegistered());
  VisitedReferences roots;
  package_context.visitRootReferences(record_reference, &roots);
  assert(contains(roots, first_root));
  assert(contains(roots, second_root));
}

void verify_sibling_rmi_state_isolation_and_delete() {
  jcvm::Context package_context(7, 4);
  constexpr uint8_t first_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x11};
  constexpr uint8_t second_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x12};
  assert(package_context.setAppletAID(first_aid, sizeof(first_aid)));
  const auto first_server = make_reference(40);
  const auto first_proxy = make_reference(41);
  const uint16_t first_object_id =
      package_context.exportRemoteObject(first_server);
  assert(first_object_id != 0xffff);
  assert(package_context.registerShareableProxy(
      first_proxy, second_aid, sizeof(second_aid), first_server));
  assert(package_context.retainShareableObjectRoot(first_server));

  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  const auto second_client_proxy = make_reference(43);
  assert(package_context.registerShareableProxy(
      second_client_proxy, first_aid, sizeof(first_aid), first_server));
  assert(package_context.remoteObjectId(first_server) == 0xffff);
  jcvm::jref_t resolved;
  assert(!package_context.remoteObject(first_object_id, resolved));
  assert(!package_context.isShareableProxy(first_proxy));
  uint8_t server_aid[16]{};
  uint8_t server_aid_length = 0;
  jcvm::jref_t server_reference;
  assert(!package_context.getShareableProxy(first_proxy, server_aid,
                                            server_aid_length,
                                            server_reference));

  const auto second_server = make_reference(42);
  const uint16_t second_object_id =
      package_context.exportRemoteObject(second_server);
  assert(second_object_id == first_object_id);
  assert(package_context.activateAppletInstance(7, first_aid,
                                                 sizeof(first_aid)));
  assert(package_context.remoteObjectId(first_server) == first_object_id);
  assert(package_context.isShareableProxy(first_proxy));
  VisitedReferences roots;
  package_context.visitRootReferences(record_reference, &roots);
  assert(contains(roots, first_server));
  assert(contains(roots, first_proxy));

  assert(package_context.forgetAppletInstance(first_aid, sizeof(first_aid)));
  assert(!package_context.hasAppletInstance(7, first_aid, sizeof(first_aid)));
  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  assert(package_context.remoteObjectId(second_server) == second_object_id);
  assert(!package_context.isShareableProxy(second_client_proxy));
  roots.count = 0;
  package_context.visitRootReferences(record_reference, &roots);
  assert(!contains(roots, first_server));
  assert(!contains(roots, first_proxy));
}

void verify_deleted_owner_is_reserved_until_objects_are_collected() {
  jcvm::Context package_context(7, 4);
  constexpr uint8_t applet_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x21};
  assert(package_context.setAppletAID(applet_aid, sizeof(applet_aid)));
  const auto owned_array_ref =
      package_context.getHeap().addArray(2, jcvm::JAVA_ARRAY_T_BYTE);
  package_context.getHeap().setStaticReferenceRoot(4, 7, owned_array_ref);
  assert(package_context.getHeap().hasLiveObjects());
  assert(package_context.isAppletOwnerRetained(7));

  assert(package_context.forgetAppletInstance(applet_aid,
                                               sizeof(applet_aid)));
  std::vector<jcvm::jref_t> roots;
  package_context.appendRootReferences(roots);
  const auto retained =
      package_context.getHeap().collectUnreachableFrom(roots);
  assert(retained.complete);
  assert(package_context.getHeap().hasLiveObjects());
  assert(package_context.isAppletOwnerRetained(7));

  package_context.getHeap().setStaticReferenceRoot(4, 7, jcvm::jref_t{});
  roots.clear();
  package_context.appendRootReferences(roots);
  const auto reclaimed =
      package_context.getHeap().collectUnreachableFrom(roots);
  assert(reclaimed.complete);
  assert(reclaimed.arrays_reclaimed == 1);
  assert(!package_context.getHeap().hasLiveObjects());
  assert(!package_context.isAppletOwnerRetained(7));
}

void verify_owner_id_space_exhaustion_and_reclamation() {
  jcvm::Context package_context(0, 4);
  std::array<std::array<uint8_t, 5>, UINT8_MAX> aids{};
  bool persisted_owner_ids[256]{};
  uint8_t allocated_owner = 0xa5;
  const auto never_retained = [](uint8_t) noexcept { return false; };
  for (uint16_t owner = 1; owner <= UINT8_MAX; ++owner)
    persisted_owner_ids[owner] = true;
  assert(!jcvm::allocate_applet_owner_id(
      persisted_owner_ids, never_retained, allocated_owner));
  assert(allocated_owner == 0xa5);
  persisted_owner_ids[17] = false;
  assert(jcvm::allocate_applet_owner_id(
      persisted_owner_ids, never_retained, allocated_owner));
  assert(allocated_owner == 17);
  persisted_owner_ids[17] = true;

  for (uint16_t owner = 1; owner <= UINT8_MAX; ++owner) {
    auto &aid = aids[owner - 1];
    aid = {0xa0, 0x00, 0x00, 0x04, static_cast<uint8_t>(owner)};
    assert(package_context.activateAppletInstance(
        static_cast<uint8_t>(owner), aid.data(), aid.size()));
    const auto reference = package_context.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_BYTE);
    package_context.getHeap().setStaticReferenceRoot(
        4, static_cast<uint16_t>(owner), reference);
    assert(package_context.forgetAppletInstance(aid.data(), aid.size()));
    assert(package_context.isAppletOwnerRetained(
        static_cast<uint8_t>(owner)));
    persisted_owner_ids[owner] = true;
  }

  for (uint16_t owner = 1; owner <= UINT8_MAX; ++owner) {
    assert(package_context.isAppletOwnerRetained(
        static_cast<uint8_t>(owner)));
  }

  const auto is_retained = [&package_context](uint8_t owner) noexcept {
    return package_context.isAppletOwnerRetained(owner);
  };
  allocated_owner = 0xa5;
  assert(!jcvm::allocate_applet_owner_id(
      persisted_owner_ids, is_retained, allocated_owner));
  assert(allocated_owner == 0xa5);
  std::fill(std::begin(persisted_owner_ids), std::end(persisted_owner_ids),
            false);
  assert(!jcvm::allocate_applet_owner_id(
      persisted_owner_ids, is_retained, allocated_owner));
  assert(allocated_owner == 0xa5);

  for (uint16_t owner = 1; owner <= UINT8_MAX; ++owner) {
    package_context.getHeap().setStaticReferenceRoot(
        4, static_cast<uint16_t>(owner), jcvm::jref_t{});
  }
  std::vector<jcvm::jref_t> roots;
  package_context.appendRootReferences(roots);
  const auto reclaimed = package_context.getHeap().collectUnreachableFrom(roots);
  assert(reclaimed.complete);
  assert(reclaimed.arrays_reclaimed == UINT8_MAX);
  for (uint16_t owner = 1; owner <= UINT8_MAX; ++owner) {
    assert(!package_context.isAppletOwnerRetained(
        static_cast<uint8_t>(owner)));
  }
  assert(jcvm::allocate_applet_owner_id(
      persisted_owner_ids, is_retained, allocated_owner));
  assert(allocated_owner == 1);
}

void verify_heap_owner_facades_share_package_roots() {
  jcvm::Heap writer_heap(7);
  jcvm::Heap reader_heap(8, writer_heap);
  assert(writer_heap.getOwner() == 7);
  assert(reader_heap.getOwner() == 8);
  const auto static_reference = make_reference(34);
  writer_heap.setStaticReferenceRoot(4, 2, static_reference);
  jcvm::jref_t resolved;
  assert(reader_heap.getStaticReferenceRoot(4, 2, resolved));
  assert(resolved == static_reference);
}

void verify_global_object_array_package_ownership() {
  constexpr jcvm::jpackage_ID_t owner_package = 20;
  constexpr jcvm::jpackage_ID_t other_package = 21;
  jcvm::Context owner_context(7, owner_package);
  jcvm::Context other_context(8, other_package);
  const jcvm::jref_t local_array = owner_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  const jcvm::jref_t global_array = jcvm::Heap::addGlobalArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE);
  const auto object_array = jcvm::Heap::getGlobalArray(global_array);

  object_array->setReferenceEntry(0, local_array, owner_context);
  assert(object_array->getReferenceEntry(0) == local_array);
  assert(object_array->getReferenceEntryOwner(0) == owner_package);
  assert(object_array->isReferenceEntryAccessible(0, owner_package));
  assert(!object_array->isReferenceEntryAccessible(0, other_package));

  // Equal compact handles in another package must not alias this entry.
  const jcvm::jref_t colliding_array = other_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  assert(colliding_array == local_array);
  object_array->setReferenceEntry(0, local_array, other_context, owner_package);
  assert(object_array->getReferenceEntryOwner(0) == owner_package);
  assert(!object_array->isReferenceEntryAccessible(
      0, other_context.getCurrentPackageID()));

  jcvm::Stack foreign_reference_stack;
  foreign_reference_stack.push_Frame(0, 0, 2, nullptr);
  foreign_reference_stack.push_Reference(local_array, owner_package);
  bool foreign_access_rejected = false;
  try {
    (void)foreign_reference_stack.pop_ReferenceFromPackage(other_package);
  } catch (jcvm::Exceptions error) {
    foreign_access_rejected = error == jcvm::Exceptions::SecurityException;
  }
  assert(foreign_access_rejected);
  assert(foreign_reference_stack.pop_ReferenceFromPackage(owner_package) ==
         local_array);

  VisitedReferences owner_roots;
  owner_context.visitRootReferences(record_reference, &owner_roots);
  assert(contains(owner_roots, local_array));
  std::vector<jcvm::jref_t> owner_root_vector;
  owner_context.appendRootReferences(owner_root_vector);
  const auto owner_collection =
      owner_context.getHeap().collectUnreachableFrom(owner_root_vector);
  assert(owner_collection.complete);
  assert(owner_collection.arrays_reachable == 1);
  assert(owner_collection.arrays_reclaimed == 0);

  VisitedReferences other_roots;
  other_context.visitRootReferences(record_reference, &other_roots);
  assert(!contains(other_roots, local_array));
  std::vector<jcvm::jref_t> other_root_vector;
  other_context.appendRootReferences(other_root_vector);
  const auto other_collection =
      other_context.getHeap().collectUnreachableFrom(other_root_vector);
  assert(other_collection.complete);
  assert(other_collection.arrays_reclaimed == 1);

  const jcvm::jref_t retained_again = owner_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  object_array->setReferenceEntry(0, retained_again, owner_context);
  jcvm::Heap::clearGlobalArrays(jcvm::ClearEvent::CLEAR_ON_RESET);
  assert(object_array->getReferenceEntry(0).isNullPointer());
  owner_root_vector.clear();
  owner_context.appendRootReferences(owner_root_vector);
  const auto reset_collection =
      owner_context.getHeap().collectUnreachableFrom(owner_root_vector);
  assert(reset_collection.complete);
  assert(reset_collection.arrays_reclaimed == 2);

  const jcvm::jref_t package_deleted = owner_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  object_array->setReferenceEntry(0, package_deleted, owner_context);
  jcvm::Heap::clearGlobalArrayReferencesForPackage(owner_package);
  assert(object_array->getReferenceEntry(0).isNullPointer());
  VisitedReferences cleared_roots;
  owner_context.visitRootReferences(record_reference, &cleared_roots);
  assert(!contains(cleared_roots, local_array));
  jcvm::Heap::releaseGlobalArray(global_array);
}

void verify_local_array_rejects_foreign_references_before_owner_reuse() {
  constexpr jcvm::jpackage_ID_t owner_package = 20;
  constexpr jcvm::jpackage_ID_t receiver_package = 21;
  jcvm::Context owner_context(7, owner_package);
  jcvm::Context receiver_context(8, receiver_package);

  const jcvm::jref_t foreign_collision =
      owner_context.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  const jcvm::jref_t foreign_noncollision =
      owner_context.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  const jcvm::jref_t receiver_collision =
      receiver_context.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  const jcvm::jref_t receiver_array = receiver_context.getHeap().addArray(
      2, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto local_array = receiver_context.getHeap().getArray(receiver_array);
  assert(foreign_collision == receiver_collision);
  assert(foreign_noncollision != receiver_collision);

  for (uint16_t index = 0; index < 2; ++index) {
    const auto foreign = index == 0 ? foreign_collision : foreign_noncollision;
    bool rejected = false;
    try {
      local_array->setReferenceEntry(index, foreign, receiver_context,
                                     owner_package);
    } catch (jcvm::Exceptions error) {
      rejected = error == jcvm::Exceptions::SecurityException;
    }
    assert(rejected);
    assert(local_array->getReferenceEntry(index).isNullPointer());
    assert(local_array->getReferenceEntryOwner(index) == 0xff);
  }

  const std::vector<jcvm::jref_t> no_roots;
  const auto owner_gc =
      owner_context.getHeap().collectUnreachableFrom(no_roots);
  assert(owner_gc.complete);
  assert(owner_gc.arrays_reclaimed == 2);
  const auto reused_collision =
      owner_context.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  const auto reused_noncollision =
      owner_context.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  assert(reused_collision == foreign_collision);
  assert(reused_noncollision == foreign_noncollision);

  std::puts("RMI_REGISTRY:PASS local_foreign_array_store_rejected=2 "
            "colliding_handle=1 noncolliding_handle=1 owner_reclaimed=2 "
            "owner_slots_reused_without_alias=1");
}

void verify_shareable_proxy_registry() {
  jcvm::Context client(4, 1);
  jcvm::Context server(5, 1);
  constexpr uint8_t server_aid[] = {0xa0, 0x00, 0x00, 0x03, 0x01};
  const jcvm::jref_t proxy(0x0010);
  const jcvm::jref_t server_object(0x0020);
  assert(client.findShareableProxy(server_aid, sizeof(server_aid),
                                  server_object)
             .isNullPointer());
  assert(client.registerShareableProxy(proxy, server_aid, sizeof(server_aid),
                                      server_object));
  assert(client.findShareableProxy(server_aid, sizeof(server_aid),
                                  server_object) == proxy);
  assert(client.isShareableProxy(proxy));
  assert(!client.isShareableProxy(server_object));

  uint8_t returned_aid[16]{};
  uint8_t returned_aid_length = 0;
  jcvm::jref_t returned_server_object;
  assert(client.getShareableProxy(proxy, returned_aid, returned_aid_length,
                                  returned_server_object));
  assert(returned_aid_length == sizeof(server_aid));
  assert(std::equal(server_aid, server_aid + sizeof(server_aid),
                    returned_aid));
  assert(returned_server_object == server_object);
  assert(server.retainShareableObjectRoot(server_object));
  VisitedReferences server_roots;
  server.visitRootReferences(record_reference, &server_roots);
  assert(contains(server_roots, server_object));
  assert(!server.retainShareableObjectRoot(jcvm::jref_t{}));
  assert(!server.retainShareableObjectRoot(jcvm::jref_t(0x0031)));

  constexpr uint8_t other_aid[] = {0xa0, 0x00, 0x00, 0x03, 0x02};
  assert(client.findShareableProxy(other_aid, sizeof(other_aid), server_object)
             .isNullPointer());
  assert(!client.registerShareableProxy(proxy, other_aid, sizeof(other_aid),
                                       server_object));
  assert(!client.registerShareableProxy(jcvm::jref_t{}, server_aid,
                                       sizeof(server_aid), server_object));
  assert(!client.registerShareableProxy(jcvm::jref_t(0x0030), server_aid, 4,
                                       server_object));

  client.clearShareableSessionState();
  assert(!client.isShareableProxy(proxy));
  assert(client.findShareableProxy(server_aid, sizeof(server_aid),
                                  server_object)
             .isNullPointer());
  assert(!client.getShareableProxy(proxy, returned_aid, returned_aid_length,
                                   returned_server_object));
  server.clearShareableSessionState();
  server_roots.count = 0;
  server.visitRootReferences(record_reference, &server_roots);
  assert(!contains(server_roots, server_object));
}

void verify_shareable_cross_context_arguments() {
  const auto receiver = make_reference(22);
  std::array<jcvm::InterpretorInitialLocal, 255> locals{};
  uint8_t local_count = 0;

  {
    jcvm::jref_t global_instance;
    global_instance.setOffset(1);
    global_instance.setAsGlobalInstance();
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 8, nullptr);
    stack.push_Reference(receiver);
    stack.push_Byte(-2);
    stack.push_Int(static_cast<jcvm::jint_t>(0x12345678));
    stack.push_Reference(global_instance);
    assert(stack.get_Pushed_ValueKind(5) ==
           jcvm::Stack::ValueKind::Reference);
    assert(stack.get_Pushed_ValueKind(4) ==
           jcvm::Stack::ValueKind::Primitive);
    assert(stack.get_Pushed_ValueKind(3) ==
           jcvm::Stack::ValueKind::IntLowWord);
    assert(stack.get_Pushed_ValueKind(2) ==
           jcvm::Stack::ValueKind::IntHighWord);
    assert(stack.get_Pushed_ValueKind(1) ==
           jcvm::Stack::ValueKind::Reference);
    assert(jcvm::buildShareableTaggedInitialLocals(
        5, receiver, stack, locals, local_count));
    assert(local_count == 4);
    assert(locals[1].type == jcvm::InterpretorLocalType::Short);
    assert(static_cast<int32_t>(locals[1].value) == -2);
    assert(locals[2].type == jcvm::InterpretorLocalType::Int);
    assert(locals[2].value == 0x12345678);
    assert(locals[3].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[3].value == global_instance.compact());

    jcvm::Stack shuffled_stack;
    shuffled_stack.push_Frame(0, 0, 8, nullptr);
    shuffled_stack.push_Reference(receiver);
    shuffled_stack.push_Short(0x2468);
    shuffled_stack.dup(1, 0);
    assert(shuffled_stack.get_Pushed_ValueKind(1) ==
           jcvm::Stack::ValueKind::Primitive);
    assert(shuffled_stack.pop_Short() == 0x2468);
    assert(shuffled_stack.pop_Short() == 0x2468);
    assert(shuffled_stack.pop_Reference() == receiver);

    shuffled_stack.push_Reference(receiver);
    shuffled_stack.push_Short(0x1357);
    shuffled_stack.swap(1, 1);
    assert(shuffled_stack.get_Pushed_ValueKind(1) ==
           jcvm::Stack::ValueKind::Reference);
    assert(shuffled_stack.pop_Reference() == receiver);
    assert(shuffled_stack.pop_Short() == 0x1357);

    jcvm::Stack private_reference_stack;
    private_reference_stack.push_Frame(0, 0, 2, nullptr);
    private_reference_stack.push_Reference(receiver);
    private_reference_stack.push_Reference(make_reference(23));
    assert(!jcvm::buildShareableTaggedInitialLocals(
        2, receiver, private_reference_stack, locals, local_count));
    assert(jcvm::buildShareableTaggedInitialLocals(
        2, receiver, private_reference_stack, locals, local_count, 22));
    assert(locals[1].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[1].reference_owner == 22);

    jcvm::Stack reference_provenance_stack;
    reference_provenance_stack.push_Frame(0, 2, 8, nullptr);
    const auto foreign_reference = make_reference(24);
    reference_provenance_stack.writeLocal_Reference(0, foreign_reference, 17);
    const auto local_reference =
        reference_provenance_stack.readLocal_ReferenceValue(0);
    assert(local_reference.reference == foreign_reference);
    assert(local_reference.owner == 17);
    bool unqualified_local_read_rejected = false;
    try {
      (void)reference_provenance_stack.readLocal_Reference(0);
    } catch (jcvm::Exceptions error) {
      unqualified_local_read_rejected =
          error == jcvm::Exceptions::SecurityException;
    }
    assert(unqualified_local_read_rejected);
    const jcvm::Stack::ReferenceValue local_reference_value{
        foreign_reference, jcvm::Stack::kCurrentReferenceOwner};
    const jcvm::Stack::ReferenceValue returned_reference_value{
        foreign_reference, 17};
    const jcvm::Stack::ReferenceValue other_owner_reference_value{
        foreign_reference, 23};
    assert(jcvm::Stack::referencesEqual(local_reference_value,
                                        returned_reference_value, 17));
    assert(jcvm::Stack::referencesEqual(returned_reference_value,
                                        returned_reference_value, 23));
    assert(!jcvm::Stack::referencesEqual(returned_reference_value,
                                         other_owner_reference_value, 17));
    reference_provenance_stack.push_Reference(foreign_reference, 17);
    bool unqualified_stack_read_rejected = false;
    try {
      (void)reference_provenance_stack.get_Pushed_Element(1);
    } catch (jcvm::Exceptions error) {
      unqualified_stack_read_rejected =
          error == jcvm::Exceptions::SecurityException;
    }
    assert(unqualified_stack_read_rejected);
    bool typed_stack_read_rejected = false;
    try {
      (void)reference_provenance_stack.get_Pushed_Reference(1);
    } catch (jcvm::Exceptions error) {
      typed_stack_read_rejected =
          error == jcvm::Exceptions::SecurityException;
    }
    assert(typed_stack_read_rejected);
    bool unqualified_pop_rejected = false;
    try {
      (void)reference_provenance_stack.pop_Reference();
    } catch (jcvm::Exceptions error) {
      unqualified_pop_rejected =
          error == jcvm::Exceptions::SecurityException;
    }
    assert(unqualified_pop_rejected);
    assert(reference_provenance_stack.pop_ReferenceFromPackage(17) ==
           foreign_reference);
    reference_provenance_stack.push_Short(0x1234);
    bool primitive_receiver_rejected = false;
    try {
      (void)reference_provenance_stack.get_Pushed_Reference(1);
    } catch (jcvm::Exceptions error) {
      primitive_receiver_rejected =
          error == jcvm::Exceptions::SecurityException;
    }
    assert(primitive_receiver_rejected);
    assert(reference_provenance_stack.pop_Short() == 0x1234);
    reference_provenance_stack.push_Reference(foreign_reference, 17);
    reference_provenance_stack.dup(1, 0);
    assert(reference_provenance_stack.get_Pushed_ReferenceOwner(1) == 17);
    assert(reference_provenance_stack.get_Pushed_ReferenceOwner(2) == 17);
    const auto duplicated_reference =
        reference_provenance_stack.pop_ReferenceValue();
    assert(duplicated_reference.reference == foreign_reference);
    assert(duplicated_reference.owner == 17);
    (void)reference_provenance_stack.pop_ReferenceValue();
    reference_provenance_stack.push_Reference(foreign_reference, 17);
    reference_provenance_stack.push_Reference(make_reference(25), 23);
    reference_provenance_stack.swap(1, 1);
    assert(reference_provenance_stack.pop_ReferenceValue().owner == 17);
    assert(reference_provenance_stack.pop_ReferenceValue().owner == 23);
    reference_provenance_stack.push_Reference(global_instance);
    assert(reference_provenance_stack.get_Pushed_ReferenceOwner(1) ==
           jcvm::Stack::kGlobalReferenceOwner);
    assert(reference_provenance_stack.pop_ReferenceValue().owner ==
           jcvm::Stack::kGlobalReferenceOwner);

#ifdef JCVM_INT_SUPPORTED
    jcvm::Stack malformed_int_stack;
    malformed_int_stack.push_Frame(0, 0, 8, nullptr);
    malformed_int_stack.push_Reference(receiver);
    malformed_int_stack.push_Int(static_cast<jcvm::jint_t>(0x12345678));
    malformed_int_stack.push_Int(static_cast<jcvm::jint_t>(0x76543210));
    malformed_int_stack.swap(1, 1);
    assert(!jcvm::buildShareableTaggedInitialLocals(
        5, receiver, malformed_int_stack, locals, local_count));
#endif
  }

  assert(jcvm::buildShareableReceiverOnlyInitialLocals(
      1, receiver, locals, local_count));
  assert(local_count == 1);
  assert(locals[0].type == jcvm::InterpretorLocalType::Reference);
  assert(locals[0].value == receiver.compact());
  assert(!jcvm::buildShareableReceiverOnlyInitialLocals(
      2, receiver, locals, local_count));
  assert(!jcvm::buildShareableReceiverOnlyInitialLocals(
      1, jcvm::jref_t{}, locals, local_count));

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 3, nullptr);
    stack.push_Reference(receiver);
    stack.push_Byte(-2);
    stack.push_Short(0x7bcd);
    constexpr uint8_t signature[] = {0x03, 0x04, 0x04}; // (byte, short) short
    assert(jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 3, receiver, stack, locals,
        local_count));
    assert(local_count == 3);
    assert(locals[0].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[0].value == receiver.compact());
    assert(locals[1].type == jcvm::InterpretorLocalType::Byte);
    assert(static_cast<int32_t>(locals[1].value) == -2);
    assert(locals[2].type == jcvm::InterpretorLocalType::Short);
    assert(locals[2].value == 0x7bcd);
    // Conversion does not consume or rearrange caller words.
    assert(stack.pop_Short() == 0x7bcd);
    assert(stack.pop_Byte() == -2);
    assert(stack.pop_Reference() == receiver);
  }

#ifdef JCVM_INT_SUPPORTED
  {
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 3, nullptr);
    stack.push_Reference(receiver);
    stack.push_Int(static_cast<jcvm::jint_t>(0x12345678));
    constexpr uint8_t signature[] = {0x05, 0x04}; // (int) short
    assert(jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 3, receiver, stack, locals,
        local_count));
    assert(local_count == 2);
    assert(locals[1].type == jcvm::InterpretorLocalType::Int);
    assert(locals[1].value == 0x12345678);
  }
#endif

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 2, nullptr);
    stack.push_Reference(receiver);
    stack.push_Reference(jcvm::jref_t{});
    constexpr uint8_t signature[] = {
        0x06, 0x00, 0x00, 0x00, 0x00, 0x04}; // (Object) short
    assert(jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 2, receiver, stack, locals,
        local_count));
    assert(local_count == 2);
    assert(locals[1].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[1].value == 0);

    stack.pop_Reference();
    stack.pop_Reference();
    stack.push_Reference(receiver);
    stack.push_Reference(make_reference(23));
    assert(!jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 2, receiver, stack, locals,
        local_count));
    assert(jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 2, receiver, stack, locals,
        local_count, 23));
    assert(locals[1].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[1].reference_owner == 23);
  }

  {
    jcvm::jref_t global_instance;
    global_instance.setOffset(1);
    global_instance.setAsGlobalInstance();
    jcvm::jref_t global_array;
    global_array.setOffset(2);
    global_array.setAsGlobalArray();

    jcvm::Stack stack;
    stack.push_Frame(0, 0, 3, nullptr);
    stack.push_Reference(receiver);
    stack.push_Reference(global_instance);
    stack.push_Reference(global_array);
    constexpr uint8_t signature[] = {
        0x06, 0x00, 0x00, 0x00, 0x00, // Object
        0x0b,                         // byte[]
        0x04};                         // short return
    assert(jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 3, receiver, stack, locals,
        local_count));
    assert(local_count == 3);
    assert(locals[1].value == global_instance.compact());
    assert(locals[2].value == global_array.compact());
    assert(stack.pop_Reference() == global_array);
    assert(stack.pop_Reference() == global_instance);
    assert(stack.pop_Reference() == receiver);

    stack.push_Reference(receiver);
    stack.push_Reference(global_array);
    constexpr uint8_t object_signature[] = {
        0x06, 0x00, 0x00, 0x00, 0x00, 0x04};
    assert(jcvm::buildShareableInitialLocals(
        object_signature, sizeof(object_signature), 2, receiver, stack,
        locals, local_count));
    assert(locals[1].value == global_array.compact());
    stack.pop_Reference();
    stack.pop_Reference();

    stack.push_Reference(receiver);
    stack.push_Reference(global_instance);
    constexpr uint8_t array_signature[] = {0x0b, 0x04};
    assert(!jcvm::buildShareableInitialLocals(
        array_signature, sizeof(array_signature), 2, receiver, stack,
        locals, local_count));
    stack.pop_Reference();
    stack.pop_Reference();
  }

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 1, nullptr);
    stack.push_Reference(receiver);
    constexpr uint8_t signature[] = {0x03, 0x04}; // (byte) short
    assert(!jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 1, receiver, stack, locals,
        local_count));
  }
}

void verify_typed_local_access() {
  jcvm::Stack stack;
  stack.push_Frame(0, 6, 1, nullptr);
  stack.writeLocal_Short(0, -9);
  assert(stack.readLocal_Short(0) == -9);

  bool rejected = false;
  try {
    (void)stack.readLocal_Reference(0);
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);

#ifdef JCVM_INT_SUPPORTED
  constexpr jcvm::jint_t expected_int = 0x12345678;
  stack.writeLocal_Int(1, expected_int);
  assert(stack.readLocal_Int(1) == expected_int);
  rejected = false;
  try {
    (void)stack.readLocal_Int(0);
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
#endif

  const auto reference = make_reference(27);
  stack.writeLocal_Reference(3, reference);
  assert(stack.readLocal_Reference(3) == reference);
  rejected = false;
  try {
    (void)stack.readLocal_Short(3);
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);

  stack.writeLocal_ReturnAddress(4, 0x1234);
  assert(stack.readLocal_ReturnAddress(4) == 0x1234);
  rejected = false;
  try {
    (void)stack.readLocal_Reference(4);
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);

  rejected = false;
  try {
    (void)stack.readLocal_Short(5);
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
}

void verify_typed_operand_pops() {
  const auto reference = make_reference(28);
  bool rejected = false;

  jcvm::Stack primitive_stack;
  primitive_stack.push_Frame(0, 0, 2, nullptr);
  primitive_stack.push_Reference(reference);
  try {
    (void)primitive_stack.pop_Short();
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
  assert(primitive_stack.pop_Reference() == reference);

  jcvm::Stack reference_stack;
  reference_stack.push_Frame(0, 0, 2, nullptr);
  reference_stack.push_Short(0x2468);
  rejected = false;
  try {
    (void)reference_stack.pop_Reference();
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
  assert(reference_stack.pop_Short() == 0x2468);

  jcvm::Stack return_address_stack;
  return_address_stack.push_Frame(0, 0, 2, nullptr);
  return_address_stack.push_Short(0x1357);
  rejected = false;
  try {
    (void)return_address_stack.pop_ReturnAddress();
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
  assert(return_address_stack.pop_Short() == 0x1357);

#ifdef JCVM_INT_SUPPORTED
  jcvm::Stack int_stack;
  int_stack.push_Frame(0, 0, 4, nullptr);
  constexpr jcvm::jint_t expected = 0x12345678;
  int_stack.push_Int(expected);
  assert(int_stack.pop_Int() == expected);
  int_stack.push_Int(expected);
  int_stack.swap(1, 1);
  rejected = false;
  try {
    (void)int_stack.pop_Int();
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
#endif
}
} // namespace

int main() {
  jcvm::Stack argument_stack;
  argument_stack.push_Frame(0, 0, 3, nullptr);
  const auto proxy_receiver = make_reference(20);
  const auto target_receiver = make_reference(21);
  argument_stack.push_Reference(proxy_receiver);
  argument_stack.push_Short(0x2468);
  argument_stack.set_Pushed_Element(2, target_receiver);
  assert(argument_stack.get_Pushed_Element(2).compact() ==
         target_receiver.compact());
  assert(argument_stack.pop_Short() == 0x2468);
  assert(argument_stack.pop_Reference().compact() ==
         target_receiver.compact());

  jcvm::Context context(1, 1);
  jcvm::Context second_context(2, 1);
  verify_sensitive_result_cross_context(context, second_context);
  verify_previous_context_aid();
  verify_retained_applet_context_lookup();
  verify_package_context_lookup();
  verify_context_retains_sibling_instance_state();
  verify_sibling_rmi_state_isolation_and_delete();
  verify_deleted_owner_is_reserved_until_objects_are_collected();
  verify_owner_id_space_exhaustion_and_reclamation();
  verify_heap_owner_facades_share_package_roots();
  verify_global_object_array_package_ownership();
  verify_local_array_rejects_foreign_references_before_owner_reuse();
  verify_shareable_proxy_registry();
  verify_shareable_cross_context_arguments();
  verify_typed_local_access();
  verify_typed_operand_pops();

  const auto first = make_reference(1);
  const uint16_t first_id = context.exportRemoteObject(first);
  assert(first_id != 0xffff);
  assert(context.remoteObjectId(first) == first_id);

  jcvm::jref_t resolved;
  assert(context.remoteObject(first_id, resolved) && resolved == first);
  assert(context.exportRemoteObject(first) == first_id);

  context.unexportRemoteObject(first);
  assert(context.remoteObjectId(first) == 0xffff);
  assert(!context.remoteObject(first_id, resolved));

  const auto retained = make_reference(2);
  const uint16_t retained_id = context.exportRemoteObject(retained);
  context.setAppletActive(true);
  assert(context.retainRemoteObjectForSession(retained));
  context.unexportRemoteObject(retained);
  assert(context.remoteObject(retained_id, resolved) && resolved == retained);

  VisitedReferences roots;
  context.visitRootReferences(record_reference, &roots);
  assert(contains(roots, retained));

  context.setAppletActive(false);
  assert(!context.remoteObject(retained_id, resolved));
  assert(context.remoteObjectId(retained) == 0xffff);

  const auto applet_aid_object = make_reference(3);
  const auto registered_applet = make_reference(4);
  const auto apdu_buffer = make_reference(5);
  const auto apdu_object = make_reference(6);
  const auto root_return = make_reference(7);
  const auto static_reference = make_reference(8);
  context.setAppletAIDObjectRef(applet_aid_object);
  jcvm::jref_t global_aid_reference;
  global_aid_reference.setOffset(0x4001);
  global_aid_reference.setAsGlobalInstance();
  assert(global_aid_reference.isGlobalInstance());
  assert(global_aid_reference.isInstance());
  assert(global_aid_reference.getGlobalInstanceIndex() == 1);
  context.markAppletRegistered(registered_applet);
  context.setAPDUBufferRef(apdu_buffer, 0, 0, false);
  context.setAPDUObjectRef(apdu_object);
  context.getStack().setRootReferenceReturnValue(root_return);
  assert(context.getStack().getRootReturnKind() ==
         jcvm::Stack::ReturnKind::Reference);
  context.getHeap().reserveStaticReferenceRoot(2, 9);
  context.getHeap().setStaticReferenceRoot(2, 9, static_reference);

  VisitedReferences lifecycle_roots;
  context.visitRootReferences(record_reference, &lifecycle_roots);
  assert(contains(lifecycle_roots, applet_aid_object));
  assert(contains(lifecycle_roots, registered_applet));
  assert(contains(lifecycle_roots, apdu_buffer));
  assert(contains(lifecycle_roots, apdu_object));
  assert(contains(lifecycle_roots, root_return));
  assert(contains(lifecycle_roots, static_reference));

  const auto foreign_root_return = make_reference(9);
  context.getStack().setRootReferenceReturnValue(
      jcvm::Stack::ReferenceValue{foreign_root_return, 2});
  jcvm::Stack::ReferenceValue returned_value{};
  assert(context.getStack().getRootReferenceReturnValue(returned_value));
  assert(returned_value.reference == foreign_root_return);
  assert(returned_value.owner == 2);
  VisitedReferences foreign_return_roots;
  context.visitRootReferences(record_reference, &foreign_return_roots);
  assert(!contains(foreign_return_roots, foreign_root_return));

  context.getStack().setRootReferenceReturnValue(
      jcvm::Stack::ReferenceValue{root_return,
                                  jcvm::Stack::kCurrentReferenceOwner});
  VisitedReferences local_return_roots;
  context.visitRootReferences(record_reference, &local_return_roots);
  assert(contains(local_return_roots, root_return));

  context.getStack().setRootReturnValue(0x1234);
  assert(context.getStack().getRootReturnKind() ==
         jcvm::Stack::ReturnKind::Short);
  context.getStack().setRootReturnValue(0x12345678,
                                       jcvm::Stack::ReturnKind::Int);
  assert(context.getStack().getRootReturnKind() ==
         jcvm::Stack::ReturnKind::Int);
  context.getStack().setRootReturnValue(0, jcvm::Stack::ReturnKind::Void);
  assert(context.getStack().getRootReturnKind() ==
         jcvm::Stack::ReturnKind::Void);
  context.getStack().recordMethodReturn(jcvm::Stack::ReturnKind::Void);
  assert(context.getStack().getLastMethodReturnKind() ==
         jcvm::Stack::ReturnKind::Void);
  VisitedReferences primitive_return_roots;
  context.visitRootReferences(record_reference, &primitive_return_roots);
  assert(!contains(primitive_return_roots, root_return));

  std::array<uint16_t, 32> ids{};
  for (uint16_t index = 0; index < ids.size(); ++index) {
    ids[index] = context.exportRemoteObject(make_reference(index + 10));
    assert(ids[index] != 0xffff);
  }
  assert(context.exportRemoteObject(make_reference(100)) == 0xffff);

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 lifecycle_roots=6 session_cleanup=1 sensitive_result_cross_context=1 previous_context_aid=1 retained_context_lookup=1 package_context_lookup=1 sibling_instance_state=1 sibling_rmi_isolation=1 sibling_rmi_delete=1 deleted_owner_reservation=1 owner_id_exhaustion=1 package_heap_arrays=1 package_heap_roots=1 global_object_array_owner=1 shareable_proxy_registry=1 shareable_object_roots=1 shareable_reset=1 global_aid_handle=1 stack_receiver_replace=1 shareable_cross_context_arguments=1 typed_local_access=1 typed_operand_pop=1");
  return 0;
}
