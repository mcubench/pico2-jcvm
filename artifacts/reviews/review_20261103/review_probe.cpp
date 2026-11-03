// Review evidence only: invokes current production methods with host test stubs.
// A nonzero result reports a contract violation; this is not a passing regression.
#include "context.hpp"
#include "jc_bytecodes/bytecodes.hpp"
#include "jc_types/jc_array.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

int main(int argc, char **argv) {
  using namespace jcvm;
  const char *mode = argc > 1 ? argv[1] : "division";
  Context a(7, 20), b(8, 21);
  Context::setActiveContext(&a);
  auto &stack = a.getStack();
  stack.push_Frame(0, 0, 8, nullptr);
  Bytecodes bytecodes(a);
  if (std::strcmp(mode, "division") == 0) {
    stack.push_Short(std::numeric_limits<jshort_t>::min());
    stack.push_Short(-1);
    bytecodes.bc_sdiv();
    const auto short_result = stack.pop_Short();
    stack.push_Int(std::numeric_limits<jint_t>::min());
    stack.push_Int(-1);
    bytecodes.bc_idiv();
    const auto int_result = stack.pop_Int();
    std::printf("SDIV actual=%d expected=-32768\nIDIV actual=%d expected=-2147483648\n",
                short_result, int_result);
    return short_result == -32768 && int_result == std::numeric_limits<jint_t>::min() ? 0 : 1;
  }
  if (std::strcmp(mode, "unsigned-shift") == 0) {
    stack.push_Int(0);
    stack.push_Int(0);
    bytecodes.bc_iushr();
    const auto int_result = stack.pop_Int();
    stack.push_Short(0);
    stack.push_Short(0);
    bytecodes.bc_sushr();
    const auto short_result = stack.pop_Short();
    std::printf("IUSHR zero actual=%d expected=0\nSUSHR zero actual=%d expected=0\n",
                int_result, short_result);
    return int_result == 0 && short_result == 0 ? 0 : 1;
  }
  if (std::strcmp(mode, "overflow") == 0) {
    stack.push_Int(std::numeric_limits<jint_t>::max());
    stack.push_Int(1);
    bytecodes.bc_iadd();
    std::printf("IADD overflow actual=%d expected=-2147483648\n", stack.pop_Int());
    return 0; // UBSan diagnostics, if any, are the evidence for this mode.
  }
  if (std::strcmp(mode, "foreign-array-gc") == 0) {
    const auto foreign = a.getHeap().addArray(1, JAVA_ARRAY_T_BYTE);
    const auto collision = b.getHeap().addArray(1, JAVA_ARRAY_T_BYTE);
    const auto retained = b.getHeap().addArray(1, JAVA_ARRAY_T_REFERENCE, static_cast<jc_cp_offset_t>(0xffff));
    auto array = b.getHeap().getArray(retained);
    array->setReferenceEntry(0, foreign, b, a.getAppletPackageID());
    std::vector<jref_t> roots{retained};
    const auto target_gc = b.getHeap().collectUnreachableFrom(roots);
    roots.clear();
    a.appendRootReferences(roots);
    const auto owner_gc = a.getHeap().collectUnreachableFrom(roots);
    std::printf("FOREIGN compact=%04x collision=%04x owner=%u; target_gc reachable=%u reclaimed=%u complete=%u; owner_gc reclaimed=%u complete=%u\n",
                foreign.compact(), collision.compact(), array->getReferenceEntryOwner(0),
                target_gc.arrays_reachable, target_gc.arrays_reclaimed, target_gc.complete,
                owner_gc.arrays_reclaimed, owner_gc.complete);
    return target_gc.arrays_reclaimed == 1 && owner_gc.arrays_reclaimed == 0 ? 0 : 1;
  }
  if (std::strcmp(mode, "sensitive-owner") == 0) {
    const auto left = a.getHeap().addArray(1, JAVA_ARRAY_T_BYTE);
    const auto right = b.getHeap().addArray(1, JAVA_ARRAY_T_BYTE);
    a.setSensitiveResultObject(left);
    Context::setActiveContext(&b);
    bool accepted = false;
    try { b.assertSensitiveResultObject(right); accepted = true; } catch (...) {}
    std::vector<jref_t> roots;
    b.appendRootReferences(roots);
    const auto gc = b.getHeap().collectUnreachableFrom(roots);
    std::printf("SENSITIVE left=%04x right=%04x cross_owner_assert_accepted=%u wrong_heap_arrays_retained=%u\n",
                left.compact(), right.compact(), accepted, gc.arrays_reachable);
    return accepted ? 1 : 0;
  }
  if (std::strcmp(mode, "int-array-size") == 0) {
    const auto ref = a.getHeap().addArray(16384, JAVA_ARRAY_T_INT);
    const auto array = a.getHeap().getArray(ref);
    std::printf("INT_ARRAY requested=16384 actual=%u\n", array->size());
    return array->size() == 16384 ? 0 : 1;
  }
  if (std::strcmp(mode, "reset-clearing") == 0) {
    const auto reset = a.getHeap().addArray(JC_Array(a.getHeap(), 1, JAVA_ARRAY_T_BYTE, true, ClearEvent::CLEAR_ON_RESET));
    const auto deselect = a.getHeap().addArray(JC_Array(a.getHeap(), 1, JAVA_ARRAY_T_BYTE, true, ClearEvent::CLEAR_ON_DESELECT));
    a.getHeap().getArray(reset)->setByteEntry(0, 0x11);
    a.getHeap().getArray(deselect)->setByteEntry(0, 0x22);
    a.getHeap().clearTransientArrays(ClearEvent::CLEAR_ON_RESET);
    const auto reset_value = a.getHeap().getArray(reset)->getByteEntry(0);
    const auto deselect_value = a.getHeap().getArray(deselect)->getByteEntry(0);
    std::printf("RESET clear_on_reset=%02x clear_on_deselect=%02x expected=00/00\n", reset_value, deselect_value);
    return reset_value == 0 && deselect_value == 0 ? 0 : 1;
  }
  if (std::strcmp(mode, "signature-array-overload") == 0) {
    const auto ref = a.getHeap().addArray(16, JAVA_ARRAY_T_BYTE, ClearEvent::CLEAR_ON_RESET);
    const auto array = a.getHeap().getArray(ref);
    std::printf("SIGNATURE_ARRAY is_transient=%u clear_event=%u expected=1/1\n", array->isTransientArray(), array->getClearEvent());
    return array->isTransientArray() ? 0 : 1;
  }
  if (std::strcmp(mode, "restored-transient-size") == 0) {
    fs::Tag tag{}; tag.len = 3; tag.value[0] = 2; tag.value[1] = 20; tag.value[2] = 1;
    JC_Array array(a.getHeap(), JAVA_ARRAY_T_BYTE, 0, tag, true, ClearEvent::CLEAR_ON_RESET, 4);
    std::printf("RESTORED_TRANSIENT requested=4 actual=%u tag_bytes=4\n", array.size());
    return array.size() == 4 ? 0 : 1;
  }
  std::fprintf(stderr, "unknown mode\n");
  return 2;
}
