// JCRE-owned temporary OneShot instances (Java Card 3.0.5 Cipher.OneShot,
// Signature.OneShot, MessageDigest.OneShot, RandomData.OneShot).

#ifndef CHOUPI_NATIVE_ONESHOT_HPP
#define CHOUPI_NATIVE_ONESHOT_HPP

#include "jc_types/jc_array.hpp"
#include "jc_types/jref_t.hpp"
#include "types.hpp"

#include <memory>

namespace jcvm {

class OneShotNative {
public:
  /// Release every open OneShot back to the JCRE. Called when an applet entry
  /// point returns (end of each APDU), on card reset and before an applet
  /// context is deleted.
  static void releaseAll() noexcept;
};

jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotOpen(
    jbyte_t kind, jbyte_t first, jbyte_t second, jbyte_t third);
jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotDelegate(
    jref_t oneShot);
void fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotClose(jref_t oneShot);
void fr_gouv_ssi_nativeimpl_NativeImplementation_oneShotRandom(
    jref_t oneShot, std::shared_ptr<JC_Array> buffer, jshort_t offset,
    jshort_t length);

} // namespace jcvm

#endif // CHOUPI_NATIVE_ONESHOT_HPP
