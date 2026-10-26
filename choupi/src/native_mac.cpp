#include "exceptions.hpp"
#include "javacard-support.h"
#include "native_crypto_exception.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

#if JC_SUPPORT_SIGNATURE_DES_MAC8_NOPAD || JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M1 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M1 || JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_M2 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_M2 || JC_SUPPORT_SIGNATURE_DES_MAC4_PKCS5 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_PKCS5 || JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M2_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M2_ALG3 || JC_SUPPORT_SIGNATURE_DES_MAC4_ISO9797_1_M1_ALG3 || \
    JC_SUPPORT_SIGNATURE_DES_MAC8_ISO9797_1_M1_ALG3 || JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD || \
    JC_SUPPORT_SIGNATURE_AES_CMAC_128

namespace jcvm {
int cipherAesEcb(bool, std::vector<unsigned char>&,
                 const std::vector<unsigned char>&, std::vector<unsigned char>&);
int cipherDesEcb(bool, std::vector<unsigned char>&,
                 const std::vector<unsigned char>&, std::vector<unsigned char>&);

namespace {
void cryptBlock(bool aes, bool encrypt, std::vector<unsigned char>& key,
                  const std::vector<unsigned char>& input,
                  std::vector<unsigned char>& output) {
  const int result = aes ? cipherAesEcb(encrypt, key, input, output)
                         : cipherDesEcb(encrypt, key, input, output);
  if (result != 0)
    throwCryptoException(CryptoExceptionReason::IllegalUse);
}

void xorBlock(std::vector<unsigned char>& left,
              const std::vector<unsigned char>& right, size_t block) {
  for (size_t i = 0; i < block; ++i) left[i] ^= right[i];
}

void shiftCmac(std::vector<unsigned char>& value) {
  unsigned char carry = 0;
  for (int i = static_cast<int>(value.size()) - 1; i >= 0; --i) {
    unsigned char next = static_cast<unsigned char>(value[static_cast<size_t>(i)] >> 7);
    value[static_cast<size_t>(i)] = static_cast<unsigned char>((value[static_cast<size_t>(i)] << 1) | carry);
    carry = next;
  }
  if (carry) value.back() ^= 0x87;
}

std::vector<unsigned char> mac(bool aes, uint8_t algorithm,
    std::vector<unsigned char>& key, const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& message) {
  const size_t block = aes ? 16 : 8;
  const bool cmac = algorithm == 49;
  const bool alg3 = algorithm == 19 || algorithm == 20 || algorithm == 47 || algorithm == 48;
  const bool nopad = algorithm == 1 || algorithm == 2 || algorithm == 18;
  const bool method1 = algorithm == 3 || algorithm == 4 || algorithm == 47 || algorithm == 48;
  std::vector<unsigned char> data = message;
  if (nopad && (data.empty() || data.size() % block))
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  if (!nopad && !cmac) {
    if (algorithm == 7 || algorithm == 8) {
      const size_t count = block - data.size() % block;
      data.insert(data.end(), count, static_cast<unsigned char>(count));
    } else if (!method1) {
      data.push_back(0x80);
      while (data.size() % block) data.push_back(0);
    } else {
      while (data.size() % block) data.push_back(0);
    }
  }

  if (cmac) {
    std::vector<unsigned char> zero(block, 0), l(block), k1(block), k2(block);
    cryptBlock(true, true, key, zero, l);
    k1 = l; shiftCmac(k1); k2 = k1; shiftCmac(k2);
    const size_t blocks = data.empty() ? 1 : (data.size() + block - 1) / block;
    const bool complete = !data.empty() && data.size() % block == 0;
    if (!complete) { data.push_back(0x80); while (data.size() % block) data.push_back(0); }
    std::vector<unsigned char> state(block, 0), in(block), out(block);
    for (size_t n = 0; n + 1 < blocks; ++n) {
      std::copy_n(data.begin() + n * block, block, in.begin());
      xorBlock(in, state, block); cryptBlock(true, true, key, in, state);
    }
    std::copy_n(data.begin() + (blocks - 1) * block, block, in.begin());
    xorBlock(in, complete ? k1 : k2, block); xorBlock(in, state, block);
    cryptBlock(true, true, key, in, out);
    return out;
  }

  if (data.empty())
    throwCryptoException(CryptoExceptionReason::IllegalUse);
  std::vector<unsigned char> state(block, 0), in(block), out(block);
  for (size_t offset = 0; offset < data.size(); offset += block) {
    std::copy_n(data.begin() + offset, block, in.begin());
    if (offset == 0 && iv.size() >= block)
      for (size_t i = 0; i < block; ++i) state[i] = iv[i];
    xorBlock(in, state, block);
    if (alg3) {
      std::vector<unsigned char> k1(key.begin(), key.begin() + 8);
      cryptBlock(false, true, k1, in, state);
    } else {
      cryptBlock(aes, true, key, in, state);
    }
  }
  if (alg3) {
    std::vector<unsigned char> k2(key.begin() + 8, key.begin() + 16), temp(block), k1(key.begin(), key.begin() + 8);
    cryptBlock(false, false, k2, state, temp);
    cryptBlock(false, true, k1, temp, state);
  }
  return state;
}

}

std::vector<unsigned char> computeSymmetricMac(
    const uint8_t algorithm, std::vector<unsigned char>& key,
    const std::vector<unsigned char>& iv,
    const std::vector<unsigned char>& message) {
  const bool aes = algorithm == 18 || algorithm == 49;
#if !JC_SUPPORT_SIGNATURE_AES_MAC_128_NOPAD && !JC_SUPPORT_SIGNATURE_AES_CMAC_128
  if (aes) throwCryptoException(CryptoExceptionReason::NoSuchAlgorithm);
#endif
  return mac(aes, algorithm, key, iv, message);
}
} // namespace jcvm
#endif
