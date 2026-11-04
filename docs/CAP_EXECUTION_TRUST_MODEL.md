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
inside the Method component before reading it. Each interpreter frame now
limits bytecode operand fetches and relative PC updates to that component and
rejects reads or targets outside it. These bounds are component-wide: they do
not yet isolate individual method bodies or prove that a branch target is an
instruction boundary. The VM also does not yet verify nested constant-pool
linkage or enforce heap field kinds. Malformed-CAP runtime/sanitizer coverage
is still required before claiming safe execution of malformed or hostile CAP
programs. Valid upstream CAPs remain the compatibility baseline.
