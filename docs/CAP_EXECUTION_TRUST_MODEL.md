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
diagnostics are checked against that body. This does not prove that a branch
target is an instruction boundary. The VM also does not yet verify nested
constant-pool linkage or
enforce heap field kinds. Malformed-CAP runtime/sanitizer coverage is still
required before claiming safe execution of malformed or hostile CAP programs.
Valid upstream CAPs remain the compatibility baseline.
