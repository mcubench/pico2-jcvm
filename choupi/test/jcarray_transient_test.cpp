#include "../src/context.hpp"
#include "../src/heap.hpp"
#include "../src/jc_types/jc_array.hpp"

#include <cstdint>
#include <iostream>

namespace jcvm {
jshort_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCopyRepack(
    jref_t, jshort_t, jshort_t, jref_t, jshort_t);
jbyte_t fr_gouv_ssi_nativeimpl_NativeImplementation_arrayCompareGeneric(
    jref_t, jshort_t, jref_t, jshort_t, jshort_t);
}

int main() {
  jcvm::Context context(0, 0);
  jcvm::Context::setActiveContext(&context);
  jcvm::Heap &heap = context.getHeap();
  jcvm::JC_Array bytes(heap, 7, jcvm::JAVA_ARRAY_T_BYTE, true,
                       jcvm::CLEAR_ON_DESELECT);
  jcvm::JC_Array shorts(heap, 5, jcvm::JAVA_ARRAY_T_SHORT, true,
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
          bytesRef, 0, bytesRef, 6, 1) >= 0) {
    std::cerr << "byte array comparison returned an incorrect ordering\n";
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
  return 0;
}
