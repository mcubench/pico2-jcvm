#include "context.hpp"
#include "exceptions.hpp"
#include "javacard-rom-metadata.h"
#include "javacard-support.h"
#include "native_checksum.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"
#include "native_crypto_exception.hpp"

#include <cstdint>
#include <memory>

namespace jcvm {

namespace {

constexpr uint16_t kAlgorithm = 0;
constexpr uint16_t kStateLow = 1;

std::shared_ptr<JC_Instance> checksumInstance(jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  auto instance = Context::getActiveContext().getHeap().getInstance(reference);
  if (!instance || instance->getPackageID() != ROM_PACKAGE_INDEX_JAVACARD_SECURITY ||
      instance->getClassIndex() != nativeChecksumClass())
    throw Exceptions::SecurityException;
  return instance;
}

void checkRange(const std::shared_ptr<JC_Array> &array, jshort_t offset,
                jshort_t length) {
  if (!array) throw Exceptions::NullPointerException;
  if (array->getType() != JAVA_ARRAY_T_BYTE)
    throw Exceptions::ArrayStoreException;
  if (offset < 0 || length < 0 ||
      static_cast<uint32_t>(offset) + length > array->size())
    throw Exceptions::ArrayIndexOutOfBoundsException;
}

uint16_t updateState(JC_Instance &instance, const JC_Array &input,
                     uint16_t offset, uint16_t length) {
  uint16_t state = instance.getNativeSessionField_Short(kStateLow);
  if (length == 0) return state;
  const auto *data = input.getByteDataView() + offset;
  for (uint16_t i = 0; i < length; ++i) {
    state ^= static_cast<uint16_t>(data[i]) << 8;
    for (unsigned bit = 0; bit < 8; ++bit) {
      state = static_cast<uint16_t>(
          (state << 1) ^ ((state & 0x8000) ? 0x1021 : 0));
    }
  }
  return state;
}

} // namespace

jref_t javacard_security_Checksum_getInstance(jbyte_t algorithm,
                                              jbool_t externalAccess) {
  if (externalAccess || !JC_SUPPORT_CHECKSUM_CRC16 || algorithm != 1)
    throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  auto &heap = Context::getActiveContext().getHeap();
  const auto reference = heap.addInstance(ROM_PACKAGE_INDEX_JAVACARD_SECURITY,
                                          nativeChecksumClass());
  checksumInstance(reference)->setField_Byte(kAlgorithm, algorithm);
  return reference;
}

jbyte_t javacard_security_Checksum_getAlgorithm(jref_t receiver) {
  return checksumInstance(receiver)->getField_Byte(kAlgorithm);
}

void javacard_security_Checksum_init(jref_t receiver,
    std::shared_ptr<JC_Array> seed, jshort_t offset, jshort_t length) {
  auto instance = checksumInstance(receiver);
  if (length != 2) throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(seed, offset, length);
  const auto *bytes = seed->getByteDataView() + offset;
  const uint16_t value = (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1];
  instance->setNativeSessionField_Short(kStateLow, signedShortFromBits(value));
}

void javacard_security_Checksum_update(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length) {
  auto instance = checksumInstance(receiver);
  checkRange(input, offset, length);
  if (length == 0) return;
  const auto state = updateState(*instance, *input, offset, length);
  instance->setNativeSessionField_Short(kStateLow, signedShortFromBits(state));
}

jshort_t javacard_security_Checksum_doFinal(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length,
    std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto instance = checksumInstance(receiver);
  checkRange(input, offset, length);
  checkRange(output, outputOffset, 2);
  const uint16_t result = updateState(*instance, *input, offset, length) ^ 0xffff;
  output->setByteEntry(outputOffset, static_cast<jbyte_t>(result >> 8));
  output->setByteEntry(outputOffset + 1, static_cast<jbyte_t>(result));
  instance->setNativeSessionField_Short(kStateLow, 0);
  context.setSensitiveResultShort(2);
  return 2;
}

} // namespace jcvm
