#ifndef PICO2_JCVM_VM_APPLET_CONTEXT_HPP
#define PICO2_JCVM_VM_APPLET_CONTEXT_HPP

#include <cstdint>

namespace jcvm {
class Context;
struct jref_t;
}

/// Find a retained applet context by AID, restoring its installed instance
/// into an inactive context slot when necessary. The selected applet is never
/// replaced by this lookup.
extern "C" {
jcvm::Context *choupi_vm_find_or_restore_applet_context(
    const uint8_t *aid, uint8_t aid_length) noexcept;

/// Invoke the target applet's getShareableInterfaceObject override and return
/// a client-heap proxy, never the server heap reference itself.
jcvm::jref_t choupi_vm_get_applet_shareable_interface_object(
    jcvm::Context &client_context, jcvm::jref_t client_aid, int8_t parameter);
}

#endif // PICO2_JCVM_VM_APPLET_CONTEXT_HPP
