#include "context.hpp"
#include "context_registry.hpp"

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

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 lifecycle_roots=6 session_cleanup=1 sensitive_result_cross_context=1 previous_context_aid=1 retained_context_lookup=1 shareable_proxy_registry=1 shareable_object_roots=1 global_aid_handle=1 stack_receiver_replace=1");
  return 0;
}
