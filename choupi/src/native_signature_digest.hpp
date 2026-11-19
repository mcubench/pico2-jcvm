#pragma once

#include "jc_types/jc_array.hpp"
#include "jc_types/jc_instance.hpp"

#include <memory>

#include <vector>

namespace jcvm {

bool signatureHasInitialDigest(JC_Instance &signature);
std::vector<unsigned char> continueSignatureDigest(
    JC_Instance &signature, const std::vector<unsigned char> &message);
void clearSignatureInitialDigest(JC_Instance &signature);

// A Signature's working buffers (INITIALVECTOR, MESSAGEBUFFER, RECOVERYHASH)
// are transient arrays allocated on first use: an algorithm that never needs
// one (e.g. ECDSA needs neither IV nor recovery hash) stores no record for it.
std::shared_ptr<JC_Array> signatureBuffer(JC_Instance &signature,
                                          uint16_t field);
// The buffer, or nullptr when it has never been allocated (nothing to clear).
std::shared_ptr<JC_Array> signatureBufferIfAllocated(JC_Instance &signature,
                                                     uint16_t field);

} // namespace jcvm
