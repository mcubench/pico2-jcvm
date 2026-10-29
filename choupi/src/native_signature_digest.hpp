#pragma once

#include "jc_types/jc_instance.hpp"

#include <vector>

namespace jcvm {

bool signatureHasInitialDigest(JC_Instance &signature);
std::vector<unsigned char> continueSignatureDigest(
    JC_Instance &signature, const std::vector<unsigned char> &message);
void clearSignatureInitialDigest(JC_Instance &signature);

} // namespace jcvm
