#ifndef PICO2_JCVM_VM_APPLET_CONTEXT_HPP
#define PICO2_JCVM_VM_APPLET_CONTEXT_HPP

#include <cstdint>

namespace jcvm {
class Context;
}

/// Find a retained applet context by AID, restoring its installed instance
/// into an inactive context slot when necessary. The selected applet is never
/// replaced by this lookup.
extern "C" jcvm::Context *
choupi_vm_find_or_restore_applet_context(const uint8_t *aid,
                                         uint8_t aid_length) noexcept;

#endif // PICO2_JCVM_VM_APPLET_CONTEXT_HPP
