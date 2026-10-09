#include "../src/context.hpp"
#include "../src/heap.hpp"
#include "../src/jc_types/jc_array.hpp"

#include <cstdint>
#include <iostream>
#include <memory>

namespace jcvm {
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t);
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
    jref_t, jshort_t, jref_t, jshort_t, jshort_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_isTransient(jref_t);
jbool_t fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet();
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
}

int main() {
  jcvm::Context context(0, 0);
  jcvm::Context::setActiveContext(&context);
  jcvm::Heap &heap = context.getHeap();
  if (jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_selectingApplet() !=
      jcvm::FALSE) {
    std::cerr << "selectingApplet reported a pending selection without a JCRE\n";
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
  pattern.setByteEntry(0, 0x12);
  pattern.setByteEntry(1, 0x34);
  const jcvm::jref_t patternRef = heap.addArray(pattern);

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
  if (nativeTransient->size() != 3 || !nativeTransient->isTransientArray() ||
      nativeTransient->getClearEvent() != jcvm::CLEAR_ON_RESET ||
      nativeBoolean->size() != 2 ||
      nativeBoolean->getType() != jcvm::JAVA_ARRAY_T_BOOLEAN ||
      nativeShort->size() != 4 ||
      nativeShort->getType() != jcvm::JAVA_ARRAY_T_SHORT ||
      nativeInt->size() != 5 ||
      nativeInt->getType() != jcvm::JAVA_ARRAY_T_INT ||
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
    if (error != jcvm::Exceptions::SecurityException) {
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

  const auto fillEnd =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_arrayFillGeneric(
          bytesRef, 4, 3, patternRef, 0);
  if (fillEnd != 7 || storedBytes->getByteEntry(4) != 0x12 ||
      storedBytes->getByteEntry(5) != 0x34 ||
      storedBytes->getByteEntry(6) != 0x12) {
    std::cerr << "pattern array fill did not repeat its pattern\n";
    return 1;
  }

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

  auto storedShorts = heap.getArray(shortsRef);
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
  std::cout << "TEST:PASS jni_array_fill_pattern\n";
  std::cout << "TEST:PASS jni_transient_factory\n";
  std::cout << "TEST:PASS jni_selecting_applet_fallback\n";
  return 0;
}
