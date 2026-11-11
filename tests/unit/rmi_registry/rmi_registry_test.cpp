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

namespace {
void push_test_frame(jcvm::Stack &stack, const uint8_t nargs,
                     const uint8_t max_locals,
                     const uint8_t max_operand_stack) {
  static const uint8_t method_code[] = {0};
  stack.push_Frame(nargs, max_locals, max_operand_stack, method_code,
                   method_code, method_code + sizeof(method_code));
}

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

void verify_dup_x_positive_depth() {
  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 3);
    stack.push_Short(10);
    stack.push_Short(20);
    stack.dup(1, 2);
    assert(stack.pop_Short() == 20);
    assert(stack.pop_Short() == 10);
    assert(stack.pop_Short() == 20);
  }

  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 5);
    stack.push_Short(10);
    stack.push_Short(20);
    stack.push_Short(30);
    stack.dup(2, 3);
    assert(stack.pop_Short() == 30);
    assert(stack.pop_Short() == 20);
    assert(stack.pop_Short() == 10);
    assert(stack.pop_Short() == 30);
    assert(stack.pop_Short() == 20);
  }

  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 3);
    const auto lower = make_reference(7);
    const auto top = make_reference(8);
    stack.push_Reference(lower, 17);
    stack.push_Reference(top, 23);
    stack.dup(1, 2);
    const auto original_top = stack.pop_ReferenceValue();
    assert(original_top.reference == top && original_top.owner == 23);
    const auto middle = stack.pop_ReferenceValue();
    assert(middle.reference == lower && middle.owner == 17);
    const auto inserted = stack.pop_ReferenceValue();
    assert(inserted.reference == top && inserted.owner == 23);
  }

  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 2);
    stack.push_Short(10);
    bool underflow = false;
    try {
      stack.dup(1, 2);
    } catch (jcvm::Exceptions error) {
      underflow = error == jcvm::Exceptions::StackUnderflowException;
    }
    assert(underflow);
    assert(stack.pop_Short() == 10);
  }

  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 2);
    stack.push_Short(10);
    stack.push_Short(20);
    bool overflow = false;
    try {
      stack.dup(1, 2);
    } catch (jcvm::Exceptions error) {
      overflow = error == jcvm::Exceptions::StackOverflowException;
    }
    assert(overflow);
    assert(stack.pop_Short() == 20);
    assert(stack.pop_Short() == 10);
  }

#ifdef JCVM_INT_SUPPORTED
  constexpr uint8_t maximum_m = 4;
  constexpr uint8_t maximum_n = maximum_m + 4;
#else
  constexpr uint8_t maximum_m = 2;
  constexpr uint8_t maximum_n = maximum_m + 2;
#endif
  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, maximum_m + maximum_n);
    for (uint8_t value = 1; value <= maximum_n; ++value) {
      stack.push_Short(value);
    }
    stack.dup(maximum_m, maximum_n);
    for (uint8_t value = maximum_n; value > 0; --value) {
      assert(stack.pop_Short() == value);
    }
    for (uint8_t value = maximum_n; value > maximum_n - maximum_m; --value) {
      assert(stack.pop_Short() == value);
    }
  }

  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 1);
    bool invalid_range = false;
    try {
      stack.dup(maximum_m, maximum_n + 1);
    } catch (jcvm::Exceptions error) {
      invalid_range = error == jcvm::Exceptions::RuntimeException;
    }
    assert(invalid_range);
  }

#ifdef JCVM_INT_SUPPORTED
  constexpr uint8_t maximum_extra_n = 4;
#else
  constexpr uint8_t maximum_extra_n = 2;
#endif
  for (uint8_t m = 1; m <= maximum_m; ++m) {
    for (uint8_t form = 0; form <= maximum_extra_n + 1; ++form) {
      const uint8_t n = form == 0 ? 0 : static_cast<uint8_t>(m + form - 1);
      const uint8_t input_words = n == 0 ? m : n;
      jcvm::Stack stack;
      push_test_frame(stack, 0, 0, input_words + m);
      for (uint8_t value = 1; value <= input_words; ++value) {
        stack.push_Short(value);
      }
      stack.dup(m, n);
      for (uint8_t value = input_words; value > 0; --value) {
        assert(stack.pop_Short() == value);
      }
      const uint8_t first_copy = n == 0 ? m : n;
      const uint8_t last_copy = n == 0 ? 1 : static_cast<uint8_t>(n - m + 1);
      for (uint8_t value = first_copy; value >= last_copy; --value) {
        assert(stack.pop_Short() == value);
        if (value == last_copy) {
          break;
        }
      }
    }
  }

  for (uint8_t m = 0; m < 16; ++m) {
    for (uint8_t n = 0; n < 16; ++n) {
      const bool valid_m = m >= 1 && m <= maximum_m;
      const bool valid_n = n == 0 ||
          (n >= m && n <= static_cast<uint8_t>(m + maximum_extra_n));
      if (valid_m && valid_n) {
        continue;
      }
      jcvm::Stack stack;
      push_test_frame(stack, 0, 0, 1);
      stack.push_Short(42);
      bool rejected = false;
      try {
        stack.dup(m, n);
      } catch (jcvm::Exceptions error) {
        rejected = error == jcvm::Exceptions::RuntimeException;
      }
      assert(rejected);
      assert(stack.pop_Short() == 42);
    }
  }

#ifdef JCVM_INT_SUPPORTED
  {
    jcvm::Stack stack;
    push_test_frame(stack, 0, 0, 6);
    stack.push_Int(static_cast<jcvm::jint_t>(0x12345678));
    stack.push_Int(static_cast<jcvm::jint_t>(0x23456789));
    stack.dup(2, 4);
    assert(stack.pop_Int() == static_cast<jcvm::jint_t>(0x23456789));
    assert(stack.pop_Int() == static_cast<jcvm::jint_t>(0x12345678));
    assert(stack.pop_Int() == static_cast<jcvm::jint_t>(0x23456789));
  }
#endif
}

void verify_unwind_to_depth_preserves_callers() {
  const uint8_t root_code[] = {0x00};
  const uint8_t caller_code[] = {0x00};
  const uint8_t callee_code[] = {0x00};
  jcvm::Stack stack;
  stack.push_Frame(0, 1, 2, root_code, root_code,
                   root_code + sizeof(root_code));
  stack.writeLocal_Short(0, 0x1234);

  stack.push_Frame(0, 1, 2, caller_code, caller_code,
                   caller_code + sizeof(caller_code));
  stack.writeLocal_Short(0, 0x2345);
  stack.push_Frame(0, 1, 2, callee_code, callee_code,
                   callee_code + sizeof(callee_code));

  assert(stack.frameDepth() == 3);
  stack.unwindToDepth(2);
  assert(stack.frameDepth() == 2);
  assert(stack.getPC().getValue() == caller_code);
  assert(stack.readLocal_Short(0) == 0x2345);

  stack.unwindToDepth(1);
  assert(stack.frameDepth() == 1);
  assert(stack.getPC().getValue() == root_code);
  assert(stack.readLocal_Short(0) == 0x1234);

  stack.unwindToDepth(0);
  assert(stack.empty());
}

template <typename Operation>
bool throws_stack_overflow(Operation operation) {
  try {
    operation();
  } catch (jcvm::Exceptions exception) {
    return exception == jcvm::Exceptions::StackOverflowException;
  }
  return false;
}

void verify_local_bounds_before_metadata_access() {
  const uint8_t method_code[] = {0x00};

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 1, 1, method_code, method_code,
                     method_code + sizeof(method_code));
    stack.writeLocal_Short(0, 0x1234);
#ifdef JCVM_INT_SUPPORTED
    assert(throws_stack_overflow([&] { stack.writeLocal_Int(0, 0x12345678); }));
#endif
    assert(stack.readLocal_Short(0) == 0x1234);
  }

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 255, 0, method_code, method_code,
                     method_code + sizeof(method_code));
    stack.push_Frame(0, 1, 0, method_code, method_code,
                     method_code + sizeof(method_code));
    assert(throws_stack_overflow([&] { stack.readLocal_Short(255); }));
    assert(throws_stack_overflow(
        [&] { (void)stack.readLocal_ReferenceValue(254); }));
    assert(throws_stack_overflow(
        [&] { (void)stack.readLocal_ReturnAddress(254); }));
#ifdef JCVM_INT_SUPPORTED
    assert(throws_stack_overflow([&] { (void)stack.readLocal_Int(254); }));
#endif
    assert(throws_stack_overflow(
        [&] { stack.writeLocal_Reference(255, jcvm::jref_t()); }));
    assert(throws_stack_overflow([&] { stack.writeLocal_ReturnAddress(255, 1); }));
  }

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 255, 0, method_code, method_code,
                     method_code + sizeof(method_code));
#ifdef JCVM_INT_SUPPORTED
    assert(throws_stack_overflow([&] { stack.writeLocal_Int(255, 0x12345678); }));
#endif
  }

  {
    jcvm::Stack stack;
    stack.push_Frame(0, 0, 1, method_code, method_code,
                     method_code + sizeof(method_code));
    stack.push_Short(0x4567);
    assert(throws_stack_overflow([&] {
      stack.push_Frame(1, 255, 1, method_code, method_code,
                       method_code + sizeof(method_code));
    }));
    assert(stack.pop_Short() == 0x4567);
  }
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

void verify_sensitive_result_owner_provenance() {
  constexpr jcvm::jpackage_ID_t producer_package = 25;
  constexpr jcvm::jpackage_ID_t consumer_package = 26;
  jcvm::Context producer(9, producer_package);
  jcvm::Context consumer(10, consumer_package);
  const auto collision = make_reference(7);
  assert(collision.compact() == make_reference(7).compact());

  const auto previous = jcvm::Context::setActiveContext(&producer);
  producer.setSensitiveResultObject(collision);
  VisitedReferences producer_roots;
  producer.visitRootReferences(record_reference, &producer_roots);
  assert(contains(producer_roots, collision));

  jcvm::Context::setActiveContext(&consumer);
  bool foreign_collision_rejected = false;
  try {
    consumer.assertSensitiveResultObject(collision);
  } catch (...) {
    foreign_collision_rejected = true;
  }
  assert(foreign_collision_rejected);
  VisitedReferences consumer_roots;
  consumer.visitRootReferences(record_reference, &consumer_roots);
  assert(!contains(consumer_roots, collision));

  producer.resetSensitiveResult();
  producer_roots = {};
  producer.visitRootReferences(record_reference, &producer_roots);
  assert(!contains(producer_roots, collision));
  jcvm::Context::setActiveContext(previous);
}

void verify_sensitive_result_roots_heap_object_until_reset() {
  constexpr jcvm::jpackage_ID_t owner_package = 27;
  jcvm::Context owner(11, owner_package);
  const auto previous = jcvm::Context::setActiveContext(&owner);
  owner.resetSensitiveResult();

  // This local handle is deliberately not supplied to the collector. After
  // the interpreted caller's frame is gone, SensitiveResult must be the root.
  const auto local_only_object =
      owner.getHeap().addArray(1, jcvm::JAVA_ARRAY_T_BYTE);
  owner.setSensitiveResultObject(local_only_object);

  std::vector<jcvm::jref_t> roots;
  owner.appendRootReferences(roots);
  const auto retained = owner.getHeap().collectUnreachableFrom(roots);
  assert(retained.complete);
  assert(retained.arrays_reachable == 1);
  assert(retained.arrays_reclaimed == 0);
  assert(owner.getHeap().getArray(local_only_object) != nullptr);

  owner.resetSensitiveResult();
  roots.clear();
  owner.appendRootReferences(roots);
  const auto reclaimed = owner.getHeap().collectUnreachableFrom(roots);
  assert(reclaimed.complete);
  assert(reclaimed.arrays_reclaimed == 1);
  bool stale_reference_rejected = false;
  try {
    (void)owner.getHeap().getArray(local_only_object);
  } catch (...) {
    stale_reference_rejected = true;
  }
  assert(stale_reference_rejected);
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
  auto cached_a = std::make_unique<jcvm::Context>(2, 1);
  auto cached_b = std::make_unique<jcvm::Context>(3, 1);
  constexpr uint8_t selected_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x01};
  constexpr uint8_t cached_a_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x02};
  constexpr uint8_t cached_b_aid[] = {0xa0, 0x00, 0x00, 0x02, 0x03};
  assert(selected.setAppletAID(selected_aid, sizeof(selected_aid)));
  assert(cached_a->setAppletAID(cached_a_aid, sizeof(cached_a_aid)));
  assert(cached_b->setAppletAID(cached_b_aid, sizeof(cached_b_aid)));
  std::array<std::unique_ptr<jcvm::Context>, 2> inactive{};
  inactive[0] = std::move(cached_a);
  inactive[1] = std::move(cached_b);

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
  jcvm::Context::setActiveContext(&selected);
  assert(jcvm::Context::findRetainedPackageContext(10) == &selected);
  assert(jcvm::Context::findRetainedPackageContext(20) == inactive[0].get());
  assert(jcvm::Context::findRetainedPackageContext(30) == inactive[2].get());
  jcvm::Context::setActiveContext(nullptr);
  assert(jcvm::Context::findRetainedPackageContext(10) == nullptr);
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
      first_proxy, second_aid, sizeof(second_aid), first_server, 9));
  assert(package_context.retainShareableObjectRoot(first_server));

  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  const auto second_client_proxy = make_reference(43);
  assert(package_context.registerShareableProxy(
      second_client_proxy, first_aid, sizeof(first_aid), first_server, 9));
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
  push_test_frame(foreign_reference_stack, 0, 0, 2);
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

void verify_local_array_routes_foreign_references_through_gc() {
  constexpr jcvm::jpackage_ID_t target_package = 20;
  constexpr jcvm::jpackage_ID_t source_package = 21;
  jcvm::Context target(7, target_package);
  jcvm::Context source(8, source_package);

  const jcvm::jref_t target_collision = target.getHeap().addInstance(
      target_package, static_cast<jcvm::jclass_index_t>(1));
  const jcvm::jref_t target_noncollision = target.getHeap().addInstance(
      target_package, static_cast<jcvm::jclass_index_t>(1));
  const jcvm::jref_t source_collision = source.getHeap().addInstance(
      source_package, static_cast<jcvm::jclass_index_t>(1));
  assert(target_collision == source_collision);

  const jcvm::jref_t source_array_ref = source.getHeap().addArray(
      2, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto source_array = source.getHeap().getArray(source_array_ref);
  source_array->setReferenceEntry(0, target_collision, source,
                                  target_package);
  source_array->setReferenceEntry(1, target_noncollision, source,
                                  target_package);
  assert(source_array->getReferenceEntryOwner(0) == target_package);
  assert(source_array->getReferenceEntryOwner(1) == target_package);
  source.getHeap().setStaticReferenceRoot(source_package, 0, source_array_ref);

  std::vector<jcvm::jref_t> source_roots;
  source.appendRootReferences(source_roots);
  const auto source_gc = source.getHeap().collectUnreachableFrom(source_roots);
  assert(source_gc.complete);
  assert(source_gc.instances_reclaimed == 1);
  assert(source_gc.arrays_reclaimed == 0);

  std::vector<jcvm::jref_t> target_roots;
  target.appendRootReferences(target_roots);
  const auto target_gc = target.getHeap().collectUnreachableFrom(target_roots);
  assert(target_gc.complete);
  assert(target_gc.instances_reclaimed == 0);
  assert(target.getHeap().getInstance(target_collision));
  assert(target.getHeap().getInstance(target_noncollision));

  source_array->setReferenceEntry(0, jcvm::jref_t{}, source);
  target_roots.clear();
  target.appendRootReferences(target_roots);
  const auto one_removed_gc =
      target.getHeap().collectUnreachableFrom(target_roots);
  assert(one_removed_gc.complete);
  assert(one_removed_gc.instances_reclaimed == 1);
  assert(target.getHeap().getInstance(target_noncollision));

  jcvm::Heap::clearForeignArrayReferencesForPackage(target_package);
  assert(source_array->getReferenceEntry(1).isNullPointer());
  assert(source_array->getReferenceEntryOwner(1) == 0xff);
  target_roots.clear();
  target.appendRootReferences(target_roots);
  const auto deleted_owner_gc =
      target.getHeap().collectUnreachableFrom(target_roots);
  assert(deleted_owner_gc.complete);
  assert(deleted_owner_gc.instances_reclaimed == 1);
  const jcvm::jref_t reused_target_slot = target.getHeap().addInstance(
      target_package, static_cast<jcvm::jclass_index_t>(1));
  assert(reused_target_slot == target_collision ||
         reused_target_slot == target_noncollision);
  assert(source_array->getReferenceEntry(1).isNullPointer());

  std::puts("RMI_REGISTRY:PASS local_foreign_array_roots_routed=2 "
            "equal_handle_alias_not_marked=1 overwritten_root_released=1 "
            "package_delete_root_cleared=1 owner_slot_reuse_safe=1");
}

void verify_unreachable_foreign_array_cycle_collected() {
  constexpr jcvm::jpackage_ID_t package_a = 24;
  constexpr jcvm::jpackage_ID_t package_b = 25;
  jcvm::Context context_a(18, package_a);
  jcvm::Context context_b(19, package_b);

  const jcvm::jref_t array_a_ref = context_a.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const jcvm::jref_t array_b_ref = context_b.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_a = context_a.getHeap().getArray(array_a_ref);
  const auto array_b = context_b.getHeap().getArray(array_b_ref);
  array_a->setReferenceEntry(0, array_b_ref, context_a, package_b);
  array_b->setReferenceEntry(0, array_a_ref, context_b, package_a);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, array_a_ref);
  jcvm::Context *cycle_contexts[] = {&context_a, &context_b};
  const auto rooted_cycle = jcvm::Context::collectHeaps(cycle_contexts, 2);
  assert(rooted_cycle.complete);
  assert(rooted_cycle.arrays_reclaimed == 0);
  assert(context_a.getHeap().getArray(array_a_ref));
  assert(context_b.getHeap().getArray(array_b_ref));

  context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
  const auto rootless_cycle = jcvm::Context::collectHeaps(cycle_contexts, 2);
  assert(rootless_cycle.complete);
  assert(rootless_cycle.arrays_reclaimed == 2);
  bool stale_array_a_rejected = false;
  bool stale_array_b_rejected = false;
  try {
    (void)context_a.getHeap().getArray(array_a_ref);
  } catch (...) {
    stale_array_a_rejected = true;
  }
  try {
    (void)context_b.getHeap().getArray(array_b_ref);
  } catch (...) {
    stale_array_b_rejected = true;
  }
  assert(stale_array_a_rejected && stale_array_b_rejected);
  std::puts("RMI_REGISTRY:PASS rooted_foreign_array_cycle=2 "
            "rootless_cycle_reclaimed=2");
}

void verify_retained_context_foreign_cycle_collected() {
  constexpr jcvm::jpackage_ID_t package_a = 26;
  constexpr jcvm::jpackage_ID_t package_b = 27;
  jcvm::Context context_a(20, package_a);
  jcvm::Context context_b(21, package_b);

  const auto array_a_ref = context_a.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_b_ref = context_b.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  context_a.getHeap().getArray(array_a_ref)->setReferenceEntry(
      0, array_b_ref, context_a, package_b);
  context_b.getHeap().getArray(array_b_ref)->setReferenceEntry(
      0, array_a_ref, context_b, package_a);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, array_a_ref);

  const auto rooted = jcvm::Context::collectRetainedHeaps();
  assert(rooted.complete);
  assert(rooted.arrays_reclaimed == 0);
  assert(context_a.getHeap().getArray(array_a_ref));
  assert(context_b.getHeap().getArray(array_b_ref));

  context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
  const auto rootless = jcvm::Context::collectRetainedHeaps();
  assert(rootless.complete);
  assert(rootless.arrays_reclaimed == 2);
  bool stale_array_a_rejected = false;
  bool stale_array_b_rejected = false;
  try {
    (void)context_a.getHeap().getArray(array_a_ref);
  } catch (...) {
    stale_array_a_rejected = true;
  }
  try {
    (void)context_b.getHeap().getArray(array_b_ref);
  } catch (...) {
    stale_array_b_rejected = true;
  }
  assert(stale_array_a_rejected && stale_array_b_rejected);

  const auto peer_static_root = context_a.getHeap().addInstance(
      package_a, static_cast<jcvm::jclass_index_t>(1));
  const auto peer_proxy_root = context_a.getHeap().addInstance(
      package_a, static_cast<jcvm::jclass_index_t>(1));
  const auto unrooted_instance = context_a.getHeap().addInstance(
      package_a, static_cast<jcvm::jclass_index_t>(1));
  constexpr uint8_t peer_aid[] = {0xa0, 0x00, 0x00, 0x1b, 0x01};
  constexpr uint8_t server_aid[] = {0xa0, 0x00, 0x00, 0x1a, 0x01};
  assert(context_b.setAppletAID(peer_aid, sizeof(peer_aid)));
  const auto proxy_instance = context_b.getHeap().addInstance(
      package_b, static_cast<jcvm::jclass_index_t>(1));
  context_b.getHeap().setStaticReferenceRoot(
      package_b, 0, peer_static_root, package_a);
  assert(context_b.registerShareableProxy(
      proxy_instance, server_aid, sizeof(server_aid), peer_proxy_root,
      package_a));

  const auto peer_roots = jcvm::Context::collectRetainedHeaps();
  assert(peer_roots.complete);
  assert(peer_roots.instances_reclaimed == 1);
  assert(context_a.getHeap().getInstance(peer_static_root));
  assert(context_a.getHeap().getInstance(peer_proxy_root));
  bool orphan_rejected = false;
  try {
    (void)context_a.getHeap().getInstance(unrooted_instance);
  } catch (...) {
    orphan_rejected = true;
  }
  assert(orphan_rejected);
  std::puts("RMI_REGISTRY:PASS retained_context_foreign_cycle=2");
  std::puts("RMI_REGISTRY:PASS retained_peer_static_proxy_roots=2");
}

void verify_retained_context_repeated_diamond_reclamation() {
  constexpr jcvm::jpackage_ID_t package_a = 34;
  constexpr jcvm::jpackage_ID_t package_b = 35;
  constexpr jcvm::jpackage_ID_t package_c = 36;
  constexpr jcvm::jpackage_ID_t package_d = 37;
  jcvm::Context context_a(28, package_a);
  jcvm::Context context_b(29, package_b);
  jcvm::Context context_c(30, package_c);
  jcvm::Context context_d(31, package_d);
  constexpr uint32_t stable_slots = 4;

  for (uint8_t iteration = 0; iteration < 8; ++iteration) {
    const auto array_a = context_a.getHeap().addArray(
        2, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    const auto array_b = context_b.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    const auto array_c = context_c.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    const auto array_d = context_d.getHeap().addArray(
        0, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    context_a.getHeap().getArray(array_a)->setReferenceEntry(
        0, array_b, context_a, package_b);
    context_a.getHeap().getArray(array_a)->setReferenceEntry(
        1, array_c, context_a, package_c);
    context_b.getHeap().getArray(array_b)->setReferenceEntry(
        0, array_d, context_b, package_d);
    context_c.getHeap().getArray(array_c)->setReferenceEntry(
        0, array_d, context_c, package_d);
    context_a.getHeap().setStaticReferenceRoot(package_a, 0, array_a);

    const auto rooted = jcvm::Context::collectRetainedHeaps();
    assert(rooted.complete && rooted.arrays_reclaimed == 0);
    assert(rooted.arrays_reachable == stable_slots);
    assert(rooted.arrays_total == stable_slots);

    context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
    const auto rootless = jcvm::Context::collectRetainedHeaps();
    assert(rootless.complete && rootless.arrays_reclaimed == stable_slots);
    assert(rootless.arrays_total == stable_slots);
  }
  std::puts("RMI_REGISTRY:PASS retained_context_diamond_cycles=8 slots=4");
}

void verify_three_heap_foreign_cycle_and_repeated_reclamation() {
  constexpr jcvm::jpackage_ID_t package_a = 34;
  constexpr jcvm::jpackage_ID_t package_b = 35;
  constexpr jcvm::jpackage_ID_t package_c = 36;
  jcvm::Context context_a(28, package_a);
  jcvm::Context context_b(29, package_b);
  jcvm::Context context_c(30, package_c);
  jcvm::Context *reverse_contexts[] = {&context_c, &context_a, &context_b};
  jcvm::Context *forward_contexts[] = {&context_a, &context_b, &context_c};

  const auto array_a_ref = context_a.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_b_ref = context_b.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_c_ref = context_c.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  context_a.getHeap().getArray(array_a_ref)->setReferenceEntry(
      0, array_b_ref, context_a, package_b);
  context_b.getHeap().getArray(array_b_ref)->setReferenceEntry(
      0, array_c_ref, context_b, package_c);
  context_c.getHeap().getArray(array_c_ref)->setReferenceEntry(
      0, array_a_ref, context_c, package_a);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, array_a_ref);

  auto rooted = jcvm::Context::collectHeaps(reverse_contexts, 3);
  assert(rooted.complete && rooted.arrays_reclaimed == 0);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
  auto rootless = jcvm::Context::collectHeaps(forward_contexts, 3);
  assert(rootless.complete && rootless.arrays_reclaimed == 3);

  const uint32_t stable_array_slots = rootless.arrays_total;
  assert(stable_array_slots == 3);
  for (uint8_t iteration = 0; iteration < 8; ++iteration) {
    const auto repeated_a = context_a.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    const auto repeated_b = context_b.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    const auto repeated_c = context_c.getHeap().addArray(
        1, jcvm::JAVA_ARRAY_T_REFERENCE,
        static_cast<jcvm::jc_cp_offset_t>(0xffff));
    context_a.getHeap().getArray(repeated_a)->setReferenceEntry(
        0, repeated_b, context_a, package_b);
    context_b.getHeap().getArray(repeated_b)->setReferenceEntry(
        0, repeated_c, context_b, package_c);
    context_c.getHeap().getArray(repeated_c)->setReferenceEntry(
        0, repeated_a, context_c, package_a);
    context_a.getHeap().setStaticReferenceRoot(package_a, 0, repeated_a);
    const auto repeated_rooted =
        jcvm::Context::collectHeaps(reverse_contexts, 3);
    assert(repeated_rooted.complete &&
           repeated_rooted.arrays_reclaimed == 0 &&
           repeated_rooted.arrays_total == stable_array_slots);
    context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
    const auto repeated_rootless =
        jcvm::Context::collectHeaps(forward_contexts, 3);
    assert(repeated_rootless.complete &&
           repeated_rootless.arrays_reclaimed == 3 &&
           repeated_rootless.arrays_total == stable_array_slots);
  }
  std::puts("RMI_REGISTRY:PASS three_heap_cycle=1 collection_order=2 "
            "repeated_cycles=8 slots=3");
}

void verify_foreign_array_diamond_reclamation() {
  constexpr jcvm::jpackage_ID_t package_a = 40;
  constexpr jcvm::jpackage_ID_t package_b = 41;
  constexpr jcvm::jpackage_ID_t package_c = 42;
  constexpr jcvm::jpackage_ID_t package_d = 43;
  jcvm::Context context_a(32, package_a);
  jcvm::Context context_b(33, package_b);
  jcvm::Context context_c(34, package_c);
  jcvm::Context context_d(35, package_d);
  jcvm::Context *contexts[] = {&context_a, &context_b, &context_c, &context_d};

  const auto array_a_ref = context_a.getHeap().addArray(
      2, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_b_ref = context_b.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_c_ref = context_c.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto array_d_ref = context_d.getHeap().addArray(
      0, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  context_a.getHeap().getArray(array_a_ref)->setReferenceEntry(
      0, array_b_ref, context_a, package_b);
  context_a.getHeap().getArray(array_a_ref)->setReferenceEntry(
      1, array_c_ref, context_a, package_c);
  context_b.getHeap().getArray(array_b_ref)->setReferenceEntry(
      0, array_d_ref, context_b, package_d);
  context_c.getHeap().getArray(array_c_ref)->setReferenceEntry(
      0, array_d_ref, context_c, package_d);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, array_a_ref);

  const auto rooted = jcvm::Context::collectHeaps(contexts, 4);
  assert(rooted.complete && rooted.arrays_reclaimed == 0 &&
         rooted.arrays_reachable == 4);
  context_a.getHeap().setStaticReferenceRoot(package_a, 0, jcvm::jref_t{});
  const auto rootless = jcvm::Context::collectHeaps(contexts, 4);
  assert(rootless.complete && rootless.arrays_reclaimed == 4);
  std::puts("RMI_REGISTRY:PASS rooted_foreign_array_diamond=4 "
            "rootless_diamond_reclaimed=4");
}

void verify_coordinated_gc_fails_closed_for_ambiguous_package() {
  jcvm::Context source(36, 45);
  jcvm::Context target_a(37, 46);
  jcvm::Context target_b(38, 46);
  const auto source_ref = source.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto target_ref = target_a.getHeap().addArray(
      0, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  source.getHeap().getArray(source_ref)->setReferenceEntry(
      0, target_ref, source, 46);
  source.getHeap().setStaticReferenceRoot(45, 0, source_ref);
  jcvm::Context *contexts[] = {&source, &target_a, &target_b};

  const auto result = jcvm::Context::collectHeaps(contexts, 3);
  assert(!result.complete);
  assert(source.getHeap().getArray(source_ref));
  assert(target_a.getHeap().getArray(target_ref));
  std::puts("RMI_REGISTRY:PASS ambiguous_package_gc_fails_closed=1");
}

void verify_same_package_sibling_array_blocks_instance_delete() {
  constexpr jcvm::jpackage_ID_t package_id = 30;
  jcvm::Context package_context(7, package_id);
  constexpr uint8_t first_aid[] = {0xa0, 0x00, 0x00, 0x1e, 0x01};
  constexpr uint8_t second_aid[] = {0xa0, 0x00, 0x00, 0x1e, 0x02};
  assert(package_context.setAppletAID(first_aid, sizeof(first_aid)));

  const jcvm::jref_t target_object = package_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  const jcvm::jref_t target_owned_array = package_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  package_context.getHeap().getArray(target_owned_array)->setReferenceEntry(
      0, target_object, package_context, package_id);
  assert(!jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));

  assert(package_context.activateAppletInstance(8, second_aid,
                                                 sizeof(second_aid)));
  const jcvm::jref_t sibling_array = package_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_REFERENCE,
      static_cast<jcvm::jc_cp_offset_t>(0xffff));
  const auto sibling_array_object =
      package_context.getHeap().getArray(sibling_array);
  sibling_array_object->setReferenceEntry(0, target_object, package_context,
                                          package_id);
  assert(!sibling_array_object->hasForeignReferenceEntries());
  assert(jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));

  sibling_array_object->setReferenceEntry(0, jcvm::jref_t{}, package_context);
  assert(!jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));

  package_context.getHeap().setStaticReferenceRoot(package_id, 0,
                                                    target_object);
  assert(jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));
  package_context.getHeap().setStaticReferenceRoot(package_id, 0,
                                                    jcvm::jref_t{});
  assert(!jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));
  const jcvm::jref_t package_static_array = package_context.getHeap().addArray(
      1, jcvm::JAVA_ARRAY_T_BYTE);
  jcvm::fs::Tag package_static_tag{};
  package_static_tag.len = 3;
  package_static_tag.value[0] = 0x02;
  package_static_tag.value[1] = package_id;
  package_context.getHeap().getArray(package_static_array)->setPersistentTag(
      package_static_tag);
  package_context.getHeap().setStaticReferenceRoot(
      package_id, 0, package_static_array);
  assert(!jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));
  package_context.getHeap().setStaticReferenceRoot(package_id, 0,
                                                    jcvm::jref_t{});
  const jcvm::jref_t sibling_instance_ref = package_context.getHeap().addInstance(
      package_id, static_cast<jcvm::jclass_index_t>(1));
  const auto sibling_instance =
      package_context.getHeap().getInstance(sibling_instance_ref);
  assert(sibling_instance->getNumberOfFields() != 0);
  const auto sibling_fields = sibling_instance->getFields();
  sibling_fields->at(0).type = jcvm::FieldType::FIELD_TYPE_OBJECT;
  sibling_fields->at(0).value = target_object.compact();
  assert(jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));
  sibling_fields->at(0).type =
      jcvm::FieldType::FIELD_TYPE_UNINITIALIZED;
  sibling_fields->at(0).value = 0;
  assert(!jcvm::Heap::hasForeignArrayReferenceToAppletOwner(
      package_id, 7, package_context.getHeap()));
  std::puts("RMI_REGISTRY:PASS same_package_sibling_array_delete_guard=1");
  std::puts("RMI_REGISTRY:PASS applet_delete_static_reference_guard=1");
  std::puts("RMI_REGISTRY:PASS applet_delete_instance_field_guard=1");
}

void verify_shareable_object_root_lease() {
  jcvm::Context server(7, 4);
  constexpr uint8_t first_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x31};
  constexpr uint8_t second_aid[] = {0xa0, 0x00, 0x00, 0x04, 0x32};
  assert(server.setAppletAID(first_aid, sizeof(first_aid)));
  const jcvm::jref_t server_object(0x0042);

  {
    jcvm::ShareableObjectRootLease lease(server, server_object);
    assert(lease.retain());
    assert(server.activateAppletInstance(8, second_aid, sizeof(second_aid)));
  }
  assert(server.activateAppletInstance(7, first_aid, sizeof(first_aid)));
  VisitedReferences roots;
  server.visitRootReferences(record_reference, &roots);
  assert(!contains(roots, server_object));

  assert(server.retainShareableObjectRoot(server_object));
  {
    jcvm::ShareableObjectRootLease lease(server, server_object);
    assert(lease.retain());
  }
  roots.count = 0;
  server.visitRootReferences(record_reference, &roots);
  assert(contains(roots, server_object));

  server.releaseShareableObjectRoot(server_object, 7);
  roots.count = 0;
  server.visitRootReferences(record_reference, &roots);
  assert(!contains(roots, server_object));
  std::puts("RMI_REGISTRY:PASS shareable_root_lease_rollback=1 "
            "owner=1 existing=1");
}

void verify_shareable_proxy_registry() {
  jcvm::Context client(4, 1);
  jcvm::Context server(5, 1);
  constexpr uint8_t server_aid[] = {0xa0, 0x00, 0x00, 0x03, 0x01};
  const jcvm::jref_t proxy(0x0010);
  const jcvm::jref_t server_object(0x0020);
  const jcvm::jref_t unrelated_proxy(0x0034);
  const jcvm::jref_t unrelated_server_object(0x0036);
  assert(client.findShareableProxy(server_aid, sizeof(server_aid),
                                  server_object)
             .isNullPointer());
  assert(client.registerShareableProxy(proxy, server_aid, sizeof(server_aid),
                                      server_object, 5));
  assert(client.registerShareableProxy(
      unrelated_proxy, server_aid, sizeof(server_aid),
      unrelated_server_object, 6));
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
  constexpr uint8_t other_aid[] = {0xa0, 0x00, 0x00, 0x03, 0x02};
  assert(client.findShareableProxy(other_aid, sizeof(other_aid), server_object)
             .isNullPointer());
  assert(!client.registerShareableProxy(proxy, other_aid, sizeof(other_aid),
                                       server_object, 5));
  assert(!client.registerShareableProxy(jcvm::jref_t{}, server_aid,
                                       sizeof(server_aid), server_object, 5));
  assert(!client.registerShareableProxy(jcvm::jref_t(0x0030), server_aid, 4,
                                       server_object, 5));
  client.forgetShareableProxiesForPackage(5);
  assert(!client.isShareableProxy(proxy));
  assert(client.isShareableProxy(unrelated_proxy));
  assert(!client.getShareableProxy(proxy, returned_aid, returned_aid_length,
                                   returned_server_object));
  assert(client.getShareableProxy(unrelated_proxy, returned_aid,
                                  returned_aid_length,
                                  returned_server_object));
  assert(returned_server_object == unrelated_server_object);
  assert(server.retainShareableObjectRoot(server_object));
  VisitedReferences server_roots;
  server.visitRootReferences(record_reference, &server_roots);
  assert(contains(server_roots, server_object));
  assert(!server.retainShareableObjectRoot(jcvm::jref_t{}));
  assert(!server.retainShareableObjectRoot(jcvm::jref_t(0x0031)));

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
    push_test_frame(stack, 0, 0, 8);
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
    push_test_frame(shuffled_stack, 0, 0, 8);
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
    push_test_frame(private_reference_stack, 0, 0, 2);
    private_reference_stack.push_Reference(receiver);
    private_reference_stack.push_Reference(make_reference(23));
    assert(!jcvm::buildShareableTaggedInitialLocals(
        2, receiver, private_reference_stack, locals, local_count));
    assert(jcvm::buildShareableTaggedInitialLocals(
        2, receiver, private_reference_stack, locals, local_count, 22));
    assert(locals[1].type == jcvm::InterpretorLocalType::Reference);
    assert(locals[1].reference_owner == 22);

    jcvm::Stack reference_provenance_stack;
    push_test_frame(reference_provenance_stack, 0, 2, 8);
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
    push_test_frame(malformed_int_stack, 0, 0, 8);
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
    push_test_frame(stack, 0, 0, 3);
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
    push_test_frame(stack, 0, 0, 3);
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
    push_test_frame(stack, 0, 0, 2);
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
    push_test_frame(stack, 0, 0, 3);
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
    push_test_frame(stack, 0, 0, 1);
    stack.push_Reference(receiver);
    constexpr uint8_t signature[] = {0x03, 0x04}; // (byte) short
    assert(!jcvm::buildShareableInitialLocals(
        signature, sizeof(signature), 1, receiver, stack, locals,
        local_count));
  }
}

void verify_typed_local_access() {
  jcvm::Stack stack;
  push_test_frame(stack, 0, 6, 1);
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
  push_test_frame(primitive_stack, 0, 0, 2);
  primitive_stack.push_Reference(reference);
  try {
    (void)primitive_stack.pop_Short();
  } catch (jcvm::Exceptions error) {
    rejected = error == jcvm::SecurityException;
  }
  assert(rejected);
  assert(primitive_stack.pop_Reference() == reference);

  jcvm::Stack reference_stack;
  push_test_frame(reference_stack, 0, 0, 2);
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
  push_test_frame(return_address_stack, 0, 0, 2);
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
  push_test_frame(int_stack, 0, 0, 4);
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
  verify_dup_x_positive_depth();
  verify_unwind_to_depth_preserves_callers();
  verify_local_bounds_before_metadata_access();
  jcvm::Stack argument_stack;
  push_test_frame(argument_stack, 0, 0, 3);
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

  verify_retained_context_foreign_cycle_collected();
  verify_retained_context_repeated_diamond_reclamation();

  jcvm::Context context(1, 1);
  jcvm::Context second_context(2, 1);
  verify_sensitive_result_cross_context(context, second_context);
  verify_sensitive_result_owner_provenance();
  verify_sensitive_result_roots_heap_object_until_reset();
  verify_previous_context_aid();
  verify_retained_applet_context_lookup();
  verify_package_context_lookup();
  verify_context_retains_sibling_instance_state();
  verify_sibling_rmi_state_isolation_and_delete();
  verify_deleted_owner_is_reserved_until_objects_are_collected();
  verify_owner_id_space_exhaustion_and_reclamation();
  verify_heap_owner_facades_share_package_roots();
  verify_global_object_array_package_ownership();
  verify_local_array_routes_foreign_references_through_gc();
  verify_unreachable_foreign_array_cycle_collected();
  verify_three_heap_foreign_cycle_and_repeated_reclamation();
  verify_foreign_array_diamond_reclamation();
  verify_coordinated_gc_fails_closed_for_ambiguous_package();
  verify_same_package_sibling_array_blocks_instance_delete();
  verify_shareable_object_root_lease();
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
  context.getHeap().reserveStaticReferenceRoot(1, 9);
  context.getHeap().setStaticReferenceRoot(1, 9, static_reference);

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

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 lifecycle_roots=6 session_cleanup=1 sensitive_result_cross_context=1 sensitive_result_gc_root=1 previous_context_aid=1 retained_context_lookup=1 package_context_lookup=1 sibling_instance_state=1 sibling_rmi_isolation=1 sibling_rmi_delete=1 deleted_owner_reservation=1 owner_id_exhaustion=1 package_heap_arrays=1 package_heap_roots=1 global_object_array_owner=1 shareable_proxy_registry=1 shareable_object_roots=1 shareable_reset=1 global_aid_handle=1 stack_receiver_replace=1 shareable_cross_context_arguments=1 typed_local_access=1 typed_operand_pop=1 stack_dup_x=1 unwind_callers=1 local_bounds=1");
  return 0;
}
