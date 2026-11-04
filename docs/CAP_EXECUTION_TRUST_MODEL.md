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

This is one boundary only. Runtime method lookup checks that the normal or
extended method header fits inside the Method component before reading it. It
does not yet bound instruction fetches or method bodies, validate branch
targets, verify nested constant-pool linkage, or enforce heap field kinds.
Those checks remain required before claiming safe execution of malformed or
hostile CAP programs. Valid upstream CAPs remain the compatibility baseline.
