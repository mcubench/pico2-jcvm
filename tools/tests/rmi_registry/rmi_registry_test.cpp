#include "context.hpp"
#include "context_registry.hpp"
#include "jc_bytecodes/shareable_arguments.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

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
  verify_shareable_proxy_registry();
  verify_shareable_cross_context_arguments();
  verify_typed_local_access();

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

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 lifecycle_roots=6 session_cleanup=1 sensitive_result_cross_context=1 previous_context_aid=1 retained_context_lookup=1 shareable_proxy_registry=1 shareable_object_roots=1 global_aid_handle=1 stack_receiver_replace=1 shareable_cross_context_arguments=1");
  return 0;
}
