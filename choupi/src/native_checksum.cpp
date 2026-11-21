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
constexpr uint16_t kStateHigh = 2;

jshort_t checksumLength(jbyte_t algorithm) {
  if (algorithm == 1 && JC_SUPPORT_CHECKSUM_CRC16) return 2;
  if (algorithm == 2 && JC_SUPPORT_CHECKSUM_CRC32) return 4;
  throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
}

std::shared_ptr<JC_Instance> checksumInstance(jref_t reference) {
  if (reference.isNullPointer()) throw Exceptions::NullPointerException;
  if (reference.isArray()) throw Exceptions::SecurityException;
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

uint32_t reflectSeed(uint32_t seed) {
  uint32_t reflected = 0;
  for (unsigned bit = 0; bit < 32; ++bit) {
    reflected = (reflected << 1) | (seed & 1);
    seed >>= 1;
  }
  return reflected;
}

void saveState(JC_Instance &instance, uint32_t state) {
  instance.setNativeSessionField_Short(kStateLow, signedShortFromBits(state));
  instance.setNativeSessionField_Short(kStateHigh, signedShortFromBits(state >> 16));
}

uint32_t updateState(JC_Instance &instance, const JC_Array &input,
                     uint16_t offset, uint16_t length, bool crc32) {
  uint32_t state = static_cast<uint16_t>(
      instance.getNativeSessionField_Short(kStateLow));
  if (crc32)
    state |= static_cast<uint32_t>(static_cast<uint16_t>(
        instance.getNativeSessionField_Short(kStateHigh))) << 16;
  if (length == 0) return state;
  const auto *data = input.getByteDataView() + offset;
  if (crc32) {
    // 0xEDB88320 is the bit-reflected representation of 0x04C11DB7.
    for (uint16_t i = 0; i < length; ++i) {
      state ^= data[i];
      for (unsigned bit = 0; bit < 8; ++bit)
        state = (state >> 1) ^ ((state & 1) ? 0xEDB88320u : 0);
    }
  } else {
    for (uint16_t i = 0; i < length; ++i) {
      state ^= static_cast<uint16_t>(data[i]) << 8;
      for (unsigned bit = 0; bit < 8; ++bit) {
        state = static_cast<uint16_t>(
            (state << 1) ^ ((state & 0x8000) ? 0x1021 : 0));
      }
    }
  }
  return state;
}

} // namespace

jref_t javacard_security_Checksum_getInstance(jbyte_t algorithm,
                                              jbool_t externalAccess) {
  if (externalAccess) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
  checksumLength(algorithm);
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
  if (length != checksumLength(instance->getField_Byte(kAlgorithm)))
    throwCryptoException(CryptoExceptionReason::IllegalValue);
  checkRange(seed, offset, length);
  const auto *bytes = seed->getByteDataView() + offset;
  uint32_t value = 0;
  for (jshort_t i = 0; i < length; ++i) value = (value << 8) | bytes[i];
  saveState(*instance, length == 4 ? reflectSeed(value) : value);
}

void javacard_security_Checksum_update(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length) {
  auto instance = checksumInstance(receiver);
  checkRange(input, offset, length);
  if (length == 0) return;
  const bool crc32 = checksumLength(instance->getField_Byte(kAlgorithm)) == 4;
  saveState(*instance, updateState(*instance, *input, offset, length, crc32));
}

jshort_t javacard_security_Checksum_doFinal(jref_t receiver,
    std::shared_ptr<JC_Array> input, jshort_t offset, jshort_t length,
    std::shared_ptr<JC_Array> output, jshort_t outputOffset) {
  auto &context = Context::getActiveContext();
  context.resetSensitiveResult();
  auto instance = checksumInstance(receiver);
  checkRange(input, offset, length);
  const jshort_t width = checksumLength(instance->getField_Byte(kAlgorithm));
  checkRange(output, outputOffset, width);
  const uint32_t result = updateState(*instance, *input, offset, length, width == 4) ^
                          (width == 4 ? 0xffffffffu : 0xffffu);
  for (jshort_t i = 0; i < width; ++i)
    output->setByteEntry(outputOffset + i,
                         static_cast<jbyte_t>(result >> (8 * (width - 1 - i))));
  saveState(*instance, 0);
  context.setSensitiveResultShort(width);
  return width;
}

} // namespace jcvm
