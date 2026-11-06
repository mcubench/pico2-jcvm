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

Runtime method lookup checks that the normal or extended method header fits
inside the Method component before reading it. The Descriptor component's
method offset and bytecode count define each frame's bytecode body, capped at
the next descriptor-listed method start. This next start is a hard boundary
even when the count is too large. In the deployed 3.0.5 API CAP suite, 975 of
978 concrete method counts end exactly at the next method; three exceed it by
one byte and are clamped. Uploaded CAP calls fail closed when a valid range
cannot be resolved. Operand fetches, relative PC updates, and exception
diagnostics are checked against that body. For dynamic/uploaded packages, each
taken short, wide, and switch branch also requires a decoded instruction-start
target. The width decoder fails closed on unknown or truncated instructions
and bounds variable-size switch tables. It does not statically check untaken
edges or control-flow stack types. The VM also does not yet verify nested
constant-pool linkage or enforce heap field kinds/access. Malformed-CAP
runtime/sanitizer coverage is still required before claiming safe execution of
malformed or hostile CAP programs.

The loader rejects ConstantPool entries whose tags are outside the six defined
CAP 2.1/2.2 reference kinds, in addition to checking the component entry count
and size. This is structural screening only: it does not prove that each
class, import, field, or method reference resolves to a compatible target.
Install preflight also rejects external ConstantPool package tokens that do
not identify an entry in the source CAP's Import component. It confirms every
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
Internal class/member targets and malformed-interpreter runtime behavior
remain open.

There is a compatibility gap with ordinary CAP loading: the Java Card 3.0.5
VM specification makes Descriptor loading optional, and GlobalPlatformPro's
default CAP load omits Descriptor and Debug. The Method component alone does
not include the per-method bytecode lengths needed by this VM's strict frame
bounds. Therefore dynamic packages without Descriptor currently fail closed
when execution enters their method. Do not replace the missing per-method
bounds with the entire Method component: that allows bytecode branches and
reads to cross method bodies. Supporting standard Descriptor-free loads while
keeping this trust policy requires another verifiable method-bounds source or
a separate CAP verification/trust mechanism. See `.agent/QKB.md` for the
reproduced `6A80` SIO service-install result and the [Java Card 3.0.5 VM
specification](https://docs.oracle.com/cd/E59935_01/JCVMS.pdf), §6.10 and §6.14.
