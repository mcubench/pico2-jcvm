# CAP execution trust model

CAPs installed through GlobalPlatform are untrusted executable input, even
when the installer authenticated the LOAD/INSTALL exchange. A valid CAP parser
result establishes structural validity only; it does not authorize VM-internal
instructions or prove bytecode control flow safe.

Firmware ROM-mask packages are trusted. The card manager allocates every
dynamically uploaded CAP package in IDs 56 through 63. `impdep1` is reserved
for ROM bytecode that invokes native Java Card API dispatch. It must never
dispatch an applet-supplied native method index. The VM therefore rejects
`impdep1` and `impdep2` while the current executing package is dynamically
allocated.

## When validation runs

The VM validates an uploaded CAP during LOAD and INSTALL, during restore,
and again when runtime code asks for a CAP view.

| Stage | Checks |
| --- | --- |
| Final GP LOAD block | The loader validates the assembled CAP before persisting it. It checks component framing and payloads, required components, and references that can be checked without resolving dependencies. The CAP package AID must match the AID in the LOAD request. Earlier blocks are checked for sequence and size as they arrive. [LOAD code](../src/cap_load.cpp#L1288) |
| GP INSTALL | The loader reads the stored CAP and reruns the LOAD-stage CAP checks. It then resolves declared imports and checks linkage against installed packages, walking the reachable uploaded dependencies before running the applet's install method. [INSTALL code](../src/cap_load.cpp#L1405) · [Dependency preflight](../src/cap_load.cpp#L1022) |
| Storage initialization and applet restore | Storage initialization checks slot metadata and the code CRC before making the stored bytes available. Applet restore on reader power-on or SELECT by AID runs the structural and import-independent CAP checks for the package being restored. [Slot initialization](../src/cap_store.cpp#L380) · [Applet restore](../src/cap_load.cpp#L1833) · [Reader power-on](../src/ccid_device.cpp#L610) · [SELECT](../src/ccid_device.cpp#L1009) |
| Runtime metadata lookup | The first `Package::getCap()` call for an uploaded package in the current CAP store revision validates component payloads and cross-component structure. Later calls can reuse that view. The fixed cache has one entry per dynamic package slot. The store withdraws its code views before flash changes and publishes them after checking the surviving slot metadata and code CRCs. A revision change invalidates the views even if replacement code has the same address and length. Trusted ROM-mask lookups check framing and skip uploaded-payload validation. [CAP lookup](../choupi/src/jc_handlers/flashmemory.cpp) · [CAP constructor](../choupi/src/jc_handlers/jc_cap.cpp#L719) · [CAP store](../src/cap_store.cpp) |

One field access can fetch the constant pool, declaring class, receiver
class, and other metadata. Before CAP view reuse, the host SWALGS profile
observed six uploaded-package CAP parses per sampled `getfield_a_this`
instruction. An arithmetic instruction can execute without fetching a CAP
view. That host replay
included Descriptor. The standard GlobalPlatformPro load used for the Pico 2
comparison omitted it, so the measured host share for Descriptor validation
does not establish the share of runtime on the Pico 2. [Field resolution](../choupi/src/jc_bytecodes/bc_object_field.cpp#L144) · [Benchmark](review_VMperf_20261122.md) · [Host load-code extraction](../tests/integration/cap_load_code.py)

## Checks during execution

The VM also checks method bounds, bytecode fetches and branch targets, and
applies instruction-specific reference type and ownership checks where
implemented. These checks depend on runtime state. The CAP view cache
holds structural metadata and leaves the instruction checks in place.

Runtime method lookup checks that the normal or extended method header fits
inside the Method component before reading it. The Descriptor component's
method offset and bytecode count define each frame's bytecode body, capped at
the next descriptor-listed method start. This next start is a hard boundary
even when the count is too large. In the deployed 3.0.5 API CAP suite, all
978 concrete method bodies now end exactly at the next method; no body-length
clamps are needed. Extended method headers are four bytes, matching the VM
method-header struct and JCVMS §6.10.3. Uploaded CAP calls fail closed when a
valid range cannot be resolved. Operand fetches, relative PC updates, and exception
diagnostics are checked against that body. For dynamic/uploaded packages, each
taken short, wide, and switch branch also requires a decoded instruction-start
target. The width decoder fails closed on unknown or truncated instructions
and bounds variable-size switch tables. It does not statically check untaken
edges or control-flow stack types. The VM does not yet verify every nested
constant-pool linkage or enforce all heap field kinds/access paths. Malformed-
CAP runtime/sanitizer coverage is still required before claiming safe execution
of malformed or hostile CAP programs.

The loader rejects ConstantPool entries whose tags are outside the six defined
CAP 2.1/2.2 reference kinds, in addition to checking the component entry count
and size. This is structural screening only: it does not prove that each
class, import, field, or method reference resolves to a compatible target.
LOAD rejects external ConstantPool package tokens that do not identify an
entry in the source CAP's Import component. INSTALL preflight confirms every
declared import resolves to an installed package, and checks that external
class tokens resolve within the imported package's Export table after JC305
compatibility mapping. For ambiguous `CONSTANT_Classref` entries it accepts a
valid class or interface token mapping; execution still validates the concrete
reference type. Static field tokens are checked against each Export record's
static-field count. Static method references accept the imported static-method
or constructor mapping only when it fits the exported static-method count.
External instance-field tokens are bounded by the exported class's declared
instance layout, including consistency of its reference-field range. External
virtual-method tokens are resolved through the same class and compatibility
mapping path used by dispatch. External `SuperMethodref` values are rejected:
the CAP format requires their class references to be local to the package.
Malformed-interpreter runtime behavior remains open.

Internal StaticFieldref offsets are checked against the StaticField image size;
internal StaticMethodref offsets use CAP Method-component coordinates, bounded
after the exception-handler table and through the one-based methods area. When
Descriptor is present, each internal StaticMethodref must match exactly one
Descriptor method record. Descriptor-free CAPs receive range/header-fit checks,
and their execution bounds are inferred from callable offsets in Applet, Class,
Export, and ConstantPool structures. This bounds each frame but does not
independently authenticate private/internal StaticMethodref starts, because
the offset comes from the reference being checked. Static field offsets are
not yet checked against declared field types.

Internal ConstantPool class references must identify an exact class/interface
record start in the Class component. InstanceFieldref, VirtualMethodref, and
SuperMethodref cannot target an interface record. Internal InstanceFieldref
tokens are bounded by declared instance layout; internal VirtualMethodref and
SuperMethodref tokens resolve through the production class-method resolver.

LOAD checks import-independent Class and Export references, including
internal superclass chains, superinterface lists, Export class offsets, and
internal virtual table offsets and interface indexes. INSTALL repeats the
import-independent checks and runs virtual method linkage checks that can
require resolving imports. Negative CAP fixtures exercise these guards.
General applet firewall enforcement remains incomplete (INV-19).

There is a compatibility and assurance gap with ordinary CAP loading: the Java
Card 3.0.5 VM specification makes Descriptor loading optional, and
GlobalPlatformPro's default CAP load omits Descriptor and Debug. The Method
component alone does not include per-method bytecode lengths. For Descriptor-
free methods, the VM derives body bounds from enumerated callable offsets and
fails closed when it cannot resolve a bounded range. This is not an
independent proof that every private/internal static offset is an authentic
method start. Do not replace inferred per-method bounds with the entire Method
component: that allows bytecode branches and reads to cross method bodies.
Closing the exact-start gap requires preserving Descriptor or adding a
separate verifier that can independently establish method boundaries. See
`.agent/QKB.md` for the Descriptor-free GPPro LOAD findings and the [Java Card
3.0.5 VM specification](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf),
§6.10 and §6.14.


## Applet ownership and lifecycle

The VM uses owner-aware references and retained applet contexts. SIO dispatch
can pass references across contexts, so root enumeration must account for
references held by other applets as well as local roots. The table describes
lifecycle boundaries. Complete enforcement of the Java Card firewall and
hostile multi-applet acceptance remain open under INV-19.

| Event | Callbacks and transient state | Context and reference handling |
| --- | --- | --- |
| SELECT | Selecting an applet runs the selection path and delivers the SELECT APDU. Switching can trigger the previous applet's deselect path. | The VM restores or reuses a context for the selected AID. Same-CAP instances can share a context while keeping per-instance ownership. |
| Deselect | The deselect path handles the applet callback and `CLEAR_ON_DESELECT` state, with same-applet reselection handled separately. | Retained contexts can still hold live references. `CLEAR_ON_RESET` state survives deselection. |
| Logical reset or reader power-off | Both CCID and VPCD take the reset path without applet deselect callbacks. It clears both transient-array classes. | The VM clears active selection and shareable session state. Persistent objects and installed instances remain. |
| USB power loss | RAM is lost, including transient contents and pending APDU state. | Boot rebuilds runtime state from validated storage. Only durable persistent state is available for restore. |
| DELETE | Card management removes the target instance or package. | The VM forgets the affected instance or context. Package deletion also invalidates package-associated foreign references and shareable proxies. Surviving contexts still contribute roots. |

See [runtime lifecycle code](../choupi/src/main.cpp),
[Pico CCID](../src/ccid_device.cpp), [VPCD](../choupi/src/vpcd_session.cpp),
and [CTX and PERSIST backlog items](../TODO.md).
