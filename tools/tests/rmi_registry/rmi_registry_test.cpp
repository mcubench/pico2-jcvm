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
} // namespace

int main() {
  jcvm::Context context(1, 1);
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

  std::array<uint16_t, 32> ids{};
  for (uint16_t index = 0; index < ids.size(); ++index) {
    ids[index] = context.exportRemoteObject(make_reference(index + 10));
    assert(ids[index] != 0xffff);
  }
  assert(context.exportRemoteObject(make_reference(100)) == 0xffff);

  std::puts("RMI_REGISTRY:PASS ids=32 roots=1 session_cleanup=1");
  return 0;
}
