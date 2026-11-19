#ifndef JCVM_NATIVE_OWNER_PIN_HPP
#define JCVM_NATIVE_OWNER_PIN_HPP

namespace jcvm {
namespace OwnerPinNative {

// Forget every validated PIN: card reset, power-up and applet deletion.
void clearValidated() noexcept;

} // namespace OwnerPinNative
} // namespace jcvm

#endif
