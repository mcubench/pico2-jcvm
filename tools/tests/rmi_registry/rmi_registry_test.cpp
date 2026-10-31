#include "context.hpp"

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
} // namespace

int main() {
  jcvm::Context context(1, 1);
  jcvm::Context second_context(2, 1);
  verify_sensitive_result_cross_context(context, second_context);

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
  const auto looked_up_aid_reference = make_reference(9);
  constexpr uint8_t looked_up_aid[] = {0xA0, 0x00, 0x00, 0x00, 0x62, 0x55};
  context.setAppletAIDObjectRef(applet_aid_object);
  assert(context.cacheLookedUpAIDObject(
      looked_up_aid, sizeof(looked_up_aid), looked_up_aid_reference));
  assert(context.findLookedUpAIDObject(looked_up_aid,
                                       sizeof(looked_up_aid)) ==
         looked_up_aid_reference);
  constexpr uint8_t different_aid[] = {0xA0, 0x00, 0x00, 0x00, 0x62, 0x56};
  assert(context.findLookedUpAIDObject(different_aid,
                                       sizeof(different_aid)).isNullPointer());
  context.markAppletRegistered(registered_applet);
  context.setAPDUBufferRef(apdu_buffer, 0, 0, false);
  context.setAPDUObjectRef(apdu_object);
  context.getStack().setRootReferenceReturnValue(root_return);
  context.getHeap().reserveStaticReferenceRoot(2, 9);
  context.getHeap().setStaticReferenceRoot(2, 9, static_reference);

  VisitedReferences lifecycle_roots;
  context.visitRootReferences(record_reference, &lifecycle_roots);
  assert(contains(lifecycle_roots, applet_aid_object));
  assert(contains(lifecycle_roots, looked_up_aid_reference));
  assert(contains(lifecycle_roots, registered_applet));
  assert(contains(lifecycle_roots, apdu_buffer));
  assert(contains(lifecycle_roots, apdu_object));
  assert(contains(lifecycle_roots, root_return));
  assert(contains(lifecycle_roots, static_reference));

  context.getStack().setRootReturnValue(0x1234);
  VisitedReferences primitive_return_roots;
  context.visitRootReferences(record_reference, &primitive_return_roots);
  assert(!contains(primitive_return_roots, root_return));

  std::array<uint16_t, 32> ids{};
  for (uint16_t index = 0; index < ids.size(); ++index) {
    ids[index] = context.exportRemoteObject(make_reference(index + 10));
    assert(ids[index] != 0xffff);
  }
  assert(context.exportRemoteObject(make_reference(100)) == 0xffff);

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 lifecycle_roots=7 session_cleanup=1 sensitive_result_cross_context=1 aid_lookup_cache=1");
  return 0;
}
