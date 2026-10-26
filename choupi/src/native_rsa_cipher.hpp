#pragma once

#include "javacard-support.h"
#include "types.hpp"
#include <memory>

namespace jcvm {
class JC_Array;
class JC_Instance;

#if JC_SUPPORT_CIPHER_RSA_PKCS1 || JC_SUPPORT_CIPHER_RSA_NOPAD || \
    JC_SUPPORT_CIPHER_RSA_OAEP
void initializeRsaCipher(std::shared_ptr<JC_Instance> cipher, jref_t key,
                         jbyte_t mode);
void rejectRsaCipherParameters(std::shared_ptr<JC_Instance> cipher,
                               jref_t key, jbyte_t mode,
                               std::shared_ptr<JC_Array> parameters,
                               jshort_t offset, jshort_t length);
jshort_t updateRsaCipher(std::shared_ptr<JC_Instance> cipher,
                         std::shared_ptr<JC_Array> input, jshort_t inputOffset,
                         jshort_t inputLength, std::shared_ptr<JC_Array> output,
                         jshort_t outputOffset);
jshort_t doFinalRsaCipher(std::shared_ptr<JC_Instance> cipher,
                          std::shared_ptr<JC_Array> input, jshort_t inputOffset,
                          jshort_t inputLength, std::shared_ptr<JC_Array> output,
                          jshort_t outputOffset);
#endif
} // namespace jcvm
