#include "../src/context.hpp"
#include "../src/heap.hpp"
#include "../src/jc_types/jc_array.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

namespace jcvm {
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t);
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
    jref_t, jshort_t, jref_t, jshort_t, jshort_t);
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
    jref_t, jshort_t, std::shared_ptr<JC_Array>, jshort_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_isTransient(jref_t);
jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet(jref_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAssignedChannel();
jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitiveArraysSupported();
jbool_t
fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitive(jref_t);
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(jbyte_t);
void fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
    std::shared_ptr<JC_Array>, jshort_t, jbyte_t);
jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_getPreviousContextAID();
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(jshort_t,
                                                                   jbyte_t);
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientBooleanArray(
    jshort_t, jbyte_t);
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientShortArray(jshort_t,
                                                                    jbyte_t);
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(jshort_t,
                                                                  jbyte_t);
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
    jshort_t, jbyte_t);
}

namespace {
void enumerate_context_roots(jcvm::Heap::ReferenceVisitor visitor,
                             void *visitor_context, void *context) {
  static_cast<const jcvm::Context *>(context)->visitRootReferences(
      visitor, visitor_context);
}
} // namespace

int main() {
  jcvm::Context context(0, 0);
  jcvm::Context::setActiveContext(&context);
  jcvm::Heap &heap = context.getHeap();
  constexpr uint8_t test_aid[] = {0x4a, 0x43, 0x41, 0x6c, 0x67,
                                  0x54, 0x65, 0x73, 0x74, 0x31};
  uint8_t copied_aid[16]{};
  uint8_t copied_aid_length = 0;
  if (context.getAppletPackageID() != 0 ||
      !context.setAppletAID(test_aid, sizeof(test_aid)) ||
      !context.copyAppletAID(copied_aid, copied_aid_length) ||
      copied_aid_length != sizeof(test_aid) ||
      std::memcmp(copied_aid, test_aid, sizeof(test_aid)) != 0) {
    std::cerr << "applet instance AID was not retained by the VM context\n";
    return 1;
  }
  if (context.setAppletAID(test_aid, 4) ||
      context.copyAppletAID(copied_aid, copied_aid_length)) {
    std::cerr << "invalid applet instance AID was accepted\n";
    return 1;
  }
  if (!context.setAppletAID(test_aid, sizeof(test_aid))) {
    std::cerr << "valid applet instance AID could not be restored\n";
    return 1;
  }
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet(
          jcvm::jref_t{}) !=
      jcvm::FALSE) {
    std::cerr << "selectingApplet reported a pending selection without a JCRE\n";
    return 1;
  }
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAssignedChannel() !=
      0) {
    std::cerr << "default transport did not select the basic channel\n";
    return 1;
  }
  if (!jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getPreviousContextAID()
           .isNullPointer()) {
    std::cerr << "previous applet context should be null without sharing\n";
    return 1;
  }
  const auto integrityArraysSupported =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitiveArraysSupported();
  if (integrityArraysSupported != jcvm::FALSE) {
    std::cerr << "unsupported integrity-sensitive arrays were reported available\n";
    return 1;
  }
  jcvm::JC_Array bytes(heap, 7, jcvm::JAVA_ARRAY_T_BYTE, true,
                       jcvm::CLEAR_ON_DESELECT);
  jcvm::JC_Array shorts(heap, 5, jcvm::JAVA_ARRAY_T_SHORT, true,
                        jcvm::CLEAR_ON_RESET);
  jcvm::JC_Array pattern(heap, 2, jcvm::JAVA_ARRAY_T_BYTE, true,
                         jcvm::CLEAR_ON_RESET);

  if (bytes.size() != 7 || shorts.size() != 5 ||
      !bytes.isTransientArray() || !shorts.isTransientArray() ||
      bytes.getClearEvent() != jcvm::CLEAR_ON_DESELECT ||
      shorts.getClearEvent() != jcvm::CLEAR_ON_RESET) {
    std::cerr << "transient array size or clear event mismatch\n";
    return 1;
  }

  for (uint16_t index = 0; index < bytes.size(); ++index) {
    bytes.setByteEntry(index, static_cast<jcvm::jbyte_t>(index + 1));
  }
  for (uint16_t index = 0; index < shorts.size(); ++index) {
    shorts.setShortEntry(index, static_cast<jcvm::jshort_t>(index + 1));
  }
  bytes.setByteEntry(6, static_cast<jcvm::jbyte_t>(0xFF));

  const jcvm::jref_t bytesRef = heap.addArray(bytes);
  const jcvm::jref_t shortsRef = heap.addArray(shorts);
  auto storedShorts = heap.getArray(shortsRef);
  pattern.setByteEntry(0, 0x12);
  pattern.setByteEntry(1, 0x34);
  const jcvm::jref_t patternRef = heap.addArray(pattern);
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitive(
          bytesRef) != jcvm::FALSE) {
    std::cerr << "ordinary array was reported as integrity-sensitive\n";
    return 1;
  }
  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_isIntegritySensitive(
        jcvm::jref_t{});
    std::cerr << "isIntegritySensitive accepted a null reference\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NullPointerException) {
      std::cerr << "isIntegritySensitive null raised wrong exception\n";
      return 1;
    }
  }

  const auto nativeTransient =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
          3, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_RESET));
  const auto nativeBoolean =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientBooleanArray(
          2, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_DESELECT));
  const auto nativeShort =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientShortArray(
          4, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_RESET));
  const auto nativeInt =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(
          5, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_DESELECT));
  const auto nativeObject =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
          3, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_DESELECT));
  if (nativeTransient->size() != 3 || !nativeTransient->isTransientArray() ||
      nativeTransient->getClearEvent() != jcvm::CLEAR_ON_RESET ||
      nativeBoolean->size() != 2 ||
      nativeBoolean->getType() != jcvm::JAVA_ARRAY_T_BOOLEAN ||
      nativeShort->size() != 4 ||
      nativeShort->getType() != jcvm::JAVA_ARRAY_T_SHORT ||
      nativeInt->size() != 5 ||
      nativeInt->getType() != jcvm::JAVA_ARRAY_T_INT ||
      nativeObject->size() != 3 ||
      nativeObject->getType() != jcvm::JAVA_ARRAY_T_REFERENCE ||
      !nativeObject->isTransientArray() ||
      nativeObject->getClearEvent() != jcvm::CLEAR_ON_DESELECT ||
      !nativeObject->getReferenceEntry(0).isNullPointer() ||
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_isTransient(bytesRef) !=
          static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_DESELECT)) {
    std::cerr << "transient native factory or query returned wrong metadata\n";
    return 1;
  }

  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_isTransient(
        jcvm::jref_t{});
    std::cerr << "isTransient accepted a null reference\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NullPointerException) {
      std::cerr << "isTransient null raised wrong exception\n";
      return 1;
    }
  }

  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
        -1, static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_RESET));
    std::cerr << "negative transient array length was accepted\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NegativeArraySizeException) {
      std::cerr << "negative transient array length raised wrong exception\n";
      return 1;
    }
  }

  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
        1, 0);
    std::cerr << "invalid transient clear event was accepted\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::SystemException) {
      std::cerr << "invalid clear event raised wrong exception\n";
      return 1;
    }
  }

  const auto copyEnd =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
          bytesRef, 0, 4, bytesRef, 2);
  auto storedBytes = heap.getArray(bytesRef);
  if (copyEnd != 6 || storedBytes->getByteEntry(0) != 1 ||
      storedBytes->getByteEntry(1) != 2 ||
      storedBytes->getByteEntry(2) != 1 ||
      storedBytes->getByteEntry(5) != 4) {
    std::cerr << "overlapping array copy did not preserve source values\n";
    return 1;
  }

  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
        bytesRef, -1, 1, bytesRef, 0);
    std::cerr << "negative array copy offset was accepted\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::ArrayIndexOutOfBoundsException) {
      std::cerr << "negative array copy offset raised the wrong exception\n";
      return 1;
    }
  }

  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          bytesRef, 0, bytesRef, 0, 6) != 0 ||
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          bytesRef, 0, bytesRef, 1, 1) >= 0 ||
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          bytesRef, 0, bytesRef, 6, 1) <= 0) {
    std::cerr << "byte array comparison returned an incorrect ordering\n";
    return 1;
  }
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          bytesRef, 6, bytesRef, 0, 1) >= 0 ||
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          shortsRef, 0, shortsRef, 1, 1) >= 0) {
    std::cerr << "signed primitive array comparison returned wrong ordering\n";
    return 1;
  }

  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
          bytesRef, 0, heap.getArray(patternRef), 0) != -1) {
    std::cerr << "byte search matched a multi-byte pattern as a single byte\n";
    return 1;
  }
  auto bytePattern = std::make_shared<jcvm::JC_Array>(
      heap, 1, jcvm::JAVA_ARRAY_T_BYTE, true, jcvm::CLEAR_ON_RESET);
  bytePattern->setByteEntry(0, static_cast<jcvm::jbyte_t>(0xFF));
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
          bytesRef, 0, bytePattern, 0) != 6) {
    std::cerr << "byte array search failed to find the requested byte\n";
    return 1;
  }

  auto shortPattern = std::make_shared<jcvm::JC_Array>(
      heap, 2, jcvm::JAVA_ARRAY_T_BYTE, true, jcvm::CLEAR_ON_RESET);
  shortPattern->setByteEntry(0, 0);
  shortPattern->setByteEntry(1, 3);
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
          shortsRef, 0, shortPattern, 0) != 2) {
    std::cerr << "short array search did not decode its big-endian value\n";
    return 1;
  }
#ifdef JCVM_INT_SUPPORTED
  jcvm::JC_Array ints(heap, 3, jcvm::JAVA_ARRAY_T_INT, true,
                      jcvm::CLEAR_ON_RESET);
  ints.setIntEntry(0, -1);
  ints.setIntEntry(1, static_cast<jcvm::jint_t>(0x80000000u));
  ints.setIntEntry(2, 0x7fffffff);
  const jcvm::jref_t intsRef = heap.addArray(ints);
  auto intPattern = std::make_shared<jcvm::JC_Array>(
      heap, 4, jcvm::JAVA_ARRAY_T_BYTE, true, jcvm::CLEAR_ON_RESET);
  intPattern->setByteEntry(0, static_cast<jcvm::jbyte_t>(0x80));
  intPattern->setByteEntry(1, 0);
  intPattern->setByteEntry(2, 0);
  intPattern->setByteEntry(3, 0);
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFindGeneric(
          intsRef, 0, intPattern, 0) != 1 ||
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
          intsRef, 1, intsRef, 0, 1) >= 0) {
    std::cerr << "int array search or signed comparison failed\n";
    return 1;
  }
  auto intFillValue = std::make_shared<jcvm::JC_Array>(
      heap, 1, jcvm::JAVA_ARRAY_T_INT, true, jcvm::CLEAR_ON_RESET);
  intFillValue->setIntEntry(0, 0x12345678);
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
          intsRef, 0, 2, heap.addArray(*intFillValue), 0) != 2 ||
      heap.getArray(intsRef)->getIntEntry(0) != 0x12345678 ||
      heap.getArray(intsRef)->getIntEntry(1) != 0x12345678) {
    std::cerr << "int fill did not repeat the selected primitive value\n";
    return 1;
  }
#endif
  std::cout << "TEST:PASS jni_array_find\n";

  const auto fillEnd =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
          bytesRef, 4, 3, patternRef, 0);
  if (fillEnd != 7 || storedBytes->getByteEntry(4) != 0x12 ||
      storedBytes->getByteEntry(5) != 0x12 ||
      storedBytes->getByteEntry(6) != 0x12) {
    std::cerr << "byte fill did not repeat the selected primitive value\n";
    return 1;
  }

  auto shortFillValue = std::make_shared<jcvm::JC_Array>(
      heap, 1, jcvm::JAVA_ARRAY_T_SHORT, true, jcvm::CLEAR_ON_RESET);
  shortFillValue->setShortEntry(0, 0x1234);
  const auto shortFillEnd =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
          shortsRef, 1, 3, heap.addArray(*shortFillValue), 0);
  if (shortFillEnd != 4 || storedShorts->getShortEntry(1) != 0x1234 ||
      storedShorts->getShortEntry(3) != 0x1234) {
    std::cerr << "short fill did not repeat the selected primitive value\n";
    return 1;
  }
  std::cout << "TEST:PASS jni_array_fill_primitive_types\n";
  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
        bytesRef, 0, 1, shortsRef, 0);
    std::cerr << "copy between incompatible array types was accepted\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::ArrayStoreException) {
      std::cerr << "incompatible array copy raised the wrong exception\n";
      return 1;
    }
  }

  heap.clearTransientArrays(jcvm::CLEAR_ON_RESET);

  if (storedBytes->getByteEntry(0) != 1 ||
      storedShorts->getShortEntry(0) != 0) {
    std::cerr << "clear-on-reset event cleared the wrong arrays\n";
    return 1;
  }

  heap.clearTransientArrays(jcvm::CLEAR_ON_DESELECT);
  if (storedBytes->getByteEntry(0) != 0) {
    std::cerr << "clear-on-deselect event did not clear its array\n";
    return 1;
  }

  std::cout << "TEST:PASS jcarray_transient_size\n";
  std::cout << "TEST:PASS jcarray_clear_events\n";
  std::cout << "TEST:PASS jni_array_copy_overlap\n";
  std::cout << "TEST:PASS jni_array_copy_errors\n";
  std::cout << "TEST:PASS jni_array_compare\n";
  std::cout << "TEST:PASS jni_transient_factory\n";
  std::cout << "TEST:PASS jni_transient_object_array\n";
  std::cout << "TEST:PASS jni_selecting_applet_fallback\n";
  std::cout << "TEST:PASS jni_basic_channel_fallback\n";
  std::cout << "TEST:PASS jni_previous_context_aid_fallback\n";
  std::cout << "TEST:PASS jni_integrity_arrays_capability\n";
  std::cout << "TEST:PASS jni_integrity_sensitive_query\n";

  auto memoryInfo = std::make_shared<jcvm::JC_Array>(
      heap, 4, jcvm::JAVA_ARRAY_T_SHORT, true, jcvm::CLEAR_ON_RESET);
  for (jcvm::jbyte_t memoryType = 0; memoryType <= 2; ++memoryType) {
    memoryInfo->setShortEntry(0, 0x1234);
    memoryInfo->setShortEntry(1, 0x5678);
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
        memoryInfo, 1, memoryType);
    if (memoryInfo->getShortEntry(0) != 0x1234 ||
        memoryInfo->getShortEntry(1) != 0 ||
        memoryInfo->getShortEntry(2) != 0 ||
        memoryInfo->getShortEntry(3) != 0) {
      std::cerr << "available-memory buffer wrote outside its two-word result\n";
      return 1;
    }
  }
  for (const jcvm::jbyte_t memoryType : {-1, 3}) {
    try {
      (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
          memoryType);
      std::cerr << "scalar available-memory query accepted an invalid type\n";
      return 1;
    } catch (jcvm::Exceptions error) {
      if (error != jcvm::Exceptions::SystemException) {
        std::cerr << "invalid scalar memory type raised the wrong exception\n";
        return 1;
      }
    }
    try {
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
          memoryInfo, 0, memoryType);
      std::cerr << "buffer available-memory query accepted an invalid type\n";
      return 1;
    } catch (jcvm::Exceptions error) {
      if (error != jcvm::Exceptions::SystemException) {
        std::cerr << "invalid buffer memory type raised the wrong exception\n";
        return 1;
      }
    }
  }
  try {
    jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
        nullptr, 0, 0);
    std::cerr << "buffer available-memory query accepted null\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NullPointerException) {
      std::cerr << "null memory-info buffer raised the wrong exception\n";
      return 1;
    }
  }
  for (const jcvm::jshort_t offset : {-1, 3}) {
    try {
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_getAvailableMemory(
          memoryInfo, offset, 0);
      std::cerr << "buffer available-memory query accepted an invalid range\n";
      return 1;
    } catch (jcvm::Exceptions error) {
      if (error != jcvm::Exceptions::ArrayIndexOutOfBoundsException) {
        std::cerr << "invalid memory-info buffer range raised the wrong exception\n";
        return 1;
      }
    }
  }
  std::cout << "TEST:PASS jni_available_memory_validation\n";

  // Exercise collection as well as enumeration: cached static and APDU Java
  // buffer references must survive while an unrelated array is reclaimed.
  const auto apduBufferRef = heap.addArray(5, jcvm::JAVA_ARRAY_T_BYTE);
  const auto apduBuffer = heap.getArray(apduBufferRef);
  context.setAPDUJavaBuffer(apduBuffer);
  context.setAPDUBufferRef(jcvm::jref_t{}, 0, 1, true);
  (void)context.beginAPDUOutgoing(false);
  context.setAPDUOutgoingLength(1);
  const auto outgoingBufferRef = heap.addArray(5, jcvm::JAVA_ARRAY_T_BYTE);
  const auto outgoingBuffer = heap.getArray(outgoingBufferRef);
  context.setAPDUOutgoing(outgoingBuffer, 0, 1);
  const auto graphRootRef = heap.addArray(
      2, jcvm::JAVA_ARRAY_T_REFERENCE, static_cast<jcvm::jc_cp_offset_t>(0xFFFF));
  auto graphRoot = heap.getArray(graphRootRef);
  const auto graphChildRef = heap.addArray(4, jcvm::JAVA_ARRAY_T_BYTE);
  auto graphChild = heap.getArray(graphChildRef);
  graphRoot->setReferenceEntry(0, graphChildRef, context);
  heap.reserveStaticReferenceRoot(0, 9);
  heap.setStaticReferenceRoot(0, 9, bytesRef);
  heap.reserveStaticReferenceRoot(0, 10);
  heap.setStaticReferenceRoot(0, 10, graphRootRef);
  const auto referenceResultRef = heap.addArray(6, jcvm::JAVA_ARRAY_T_BYTE);
  auto referenceResult = heap.getArray(referenceResultRef);
  context.getStack().setRootReferenceReturnValue(referenceResultRef);
  const auto unrootedRef = heap.addArray(3, jcvm::JAVA_ARRAY_T_BYTE);
  const auto collection = heap.collectUnreachableFrom(
      enumerate_context_roots, &context);
  if (!collection.complete || collection.arrays_reclaimed == 0 ||
      heap.getArray(bytesRef) != storedBytes ||
      heap.getArray(apduBufferRef) != apduBuffer ||
      heap.getArray(outgoingBufferRef) != outgoingBuffer ||
      heap.getArray(graphRootRef) != graphRoot ||
      heap.getArray(graphChildRef) != graphChild ||
      heap.getArray(referenceResultRef) != referenceResult) {
    std::cerr << "collector failed to retain static/APDU roots or reclaim garbage\n";
    return 1;
  }
  bool unrooted_reclaimed = false;
  try {
    (void)heap.getArray(unrootedRef);
  } catch (jcvm::Exceptions error) {
    unrooted_reclaimed = error == jcvm::Exceptions::SecurityException;
  }
  if (!unrooted_reclaimed) {
    std::cerr << "collector retained an unrooted array handle\n";
    return 1;
  }
  context.getStack().setRootReturnValue(42);
  const auto primitiveResultRef = heap.addArray(6, jcvm::JAVA_ARRAY_T_BYTE);
  const auto primitiveResultCollection = heap.collectUnreachableFrom(
      enumerate_context_roots, &context);
  bool primitive_result_reclaimed = false;
  try {
    (void)heap.getArray(primitiveResultRef);
  } catch (jcvm::Exceptions error) {
    primitive_result_reclaimed = error == jcvm::Exceptions::SecurityException;
  }
  if (!primitiveResultCollection.complete || !primitive_result_reclaimed) {
    std::cerr << "primitive root result incorrectly retained an array\n";
    return 1;
  }
  std::cout << "TEST:PASS gc_retains_static_and_apdu_buffers\n";
  std::cout << "TEST:PASS gc_traces_reference_array_child\n";
  std::cout << "TEST:PASS gc_tracks_reference_result_type\n";
  std::cout << "TEST:PASS gc_reclaims_unrooted_array reclaimed="
            << collection.arrays_reclaimed << "\n";
  return 0;
}
