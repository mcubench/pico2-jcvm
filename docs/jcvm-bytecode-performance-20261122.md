# Java Card bytecode performance on Pico 2

**Repeated validation of the uploaded CAP dominates the two JCAlgTest software workloads in the host profile.** On the Pico 2, software AES took **2,524 ms per 16-byte block**, compared with **7.80 ms** for a 16-byte native-backed AES-128 ECB `Cipher.doFinal()` invocation. The XOR of two 16-byte spans took **34.86 ms**.

## Baseline

The physical replay used EE116F37C7E4A04E, reader
`Pico 2 CHOUPI CCID [JCVM CCID] (EE116F37C7E4A04E) 00 00`, on normal
`max_crypto` firmware from `0cf976ddba5b8a3f`. CPLC confirmed that identity.
The UF2 SHA-256 was
`effccdec0c2b3319d02db2955a94451e07aa8e396c5ccf9d6514a693603acda6`.
The ARM build emitted no warnings and the full JCVM cycle printed
`TEST:SUMMARY pass=1 fail=0`.

The replay used the unchanged 1.8.5/jc305 CAP
(`fa64582e0dc968f7440434f01851c1aa72b57821e099fb08091dae32521b48fb`),
installed with GlobalPlatformPro. `GETVERSION` returned `1.8.5_jc305`.
The software APDU configurations came from the supplied JCAlgTest rows.
For the native comparison, the JCAlgTest AES-128 ECB no-padding encrypt
`doFinal` configuration used 16 bytes. The focused replay sent the
APDUs directly through the existing `ApduTimer` driver on hardware
and VPCD on the host. The pinned 1.8.5-20261121 client JAR was verified
(`3cca70cbc243a773b998fa43792a135e26934d4bf616e7baa88c5369db18a703`).
No Java client campaign was started. The development cycle preserved
storage, so this is an exploratory timing comparison under INV-27.

| Workload | Pico 2 raw 50-repeat APDU | Pico 2 zero-repeat median | Pico 2 net ms/op | Host net ms/op | Net Java Card bytecodes/op |
| --- | ---: | ---: | ---: | ---: | ---: |
| Software XOR, 16 bytes | 1,868 ms | 125 ms | 34.86 | 1.701 | 149 |
| Software AES, 16 bytes | 126,304 ms | 125 ms | 2,523.58 | 101.099 | 25,935.12 |
| Native-backed AES-128 ECB, 16 bytes | 517 ms | 127 ms | 7.80 | 0.206 | 22 |

Net times use the difference between the 50-repeat and zero-repeat APDUs
divided by 50. XOR and native AES use three measured APDUs per point.
Software AES has one physical sample each at 5 and 50 repeats and three
at 1 repeat. Its 1-, 5- and 50-repeat net rates were 2,519.0, 2,522.2 and
2,523.6 ms/op. The corresponding XOR rates were 35.0, 34.8 and 34.86 ms/op.
Every measured APDU returned `AA9000`. Host timings use the unmodified
`-Os` PC build with the RP2350 overlay enabled
and three samples per point. A second run measured 1.696, 101.309
and 0.225 ms/op respectively at 50 repeats.

The embedded class files in the supplied CAP show what each loop runs.
AES schedules round keys in prepare (`C0`) and calls
`JavaCardAES.AESEncryptBlock` once per measured repeat (`A5`).
XOR is an unrolled loop body of 16 byte XORs, and reloads the
`m_ram1` field twice per byte. The native-backed result includes applet
loop and VM-to-native API overhead, Java Card buffer handling and Mbed TLS.
The roughly 324× ratio compares completed operations through their APDUs.
It cannot be used as a ratio for the underlying AES primitives or as a
measure of relative MCU speed.

The supplied 2,784.12 ms AES and 40.82 ms XOR are consistent in scale.
An archived run on F42DD9D07D6D2B07 (`d7e90a18`, client
`724bda75022756605bdde5f54edc9fece4e0b70a23eed5ddc86645d60e1eabd9`)
reported 2,736.61 and 38.51 ms/op respectively. That run and the supplied
smartcard comparison are independent of the new EE11 measurement. The
commercial card's MCU speed and the basis of its reported advantage are
unverified.

## Where the time goes

- **Flash commits and USB account for very little of the long APDU.**
  On EE11, the AES 50-repeat APDU spent 126,290.824 ms in the VM and
  3.292 ms in sync, with 768 journal bytes and no snapshot. The repeated
  XOR APDUs spent 1,866.355–1,866.424 ms in the VM, with 0.170–0.172 ms
  in sync and zero journal or snapshot bytes. This rules out flash
  programming as the dominant cost in those samples. The CAP bytes
  can still incur flash/XIP read costs during repeated validation.
- **Field access dwarfs the arithmetic and array handlers in the sampled
  host workload.** The isolated PC build counted all bytecodes and
  timed roughly one in 64 handler calls. In the 50-repeat XOR APDU,
  `getfield_a_this` accounted for 99.4% of estimated handler time
  (21 sampled field calls). In software AES it accounted for 93.2%
  (1,439 sampled field calls), followed by `getstatic_a` at 4.2%.
  AES executed 100,759 `baload` and 53,577 `bastore` instructions,
  but each handler accounted for under 1% of estimated time.
  After subtracting the zero-repeat APDU, XOR executed 32 field reads,
  32 byte loads and 16 byte stores per repeat. AES executed about 1,779
  `getfield_a_this` instructions per repeat. Estimates multiply the
  mean sampled time for each opcode by its count. They exclude
  interpreter dispatch and are host attribution, with no ARM cycle split.
- **The field handler repeatedly reparses the CAP.**
  `getfield_a_this` resolves the field token, validates the declared
  field kind and checks the receiver class and inheritance before reading
  the instance field. Each sampled handler in these workloads made six
  dynamic and six ROM `FlashMemory_Handler::getCap()` calls. The dynamic
  calls consumed 98.7% of sampled XOR field-handler time and 98.8% for AES.
  `Package::getCap()` constructs a fresh `JC_Cap`. For an uploaded
  package, that rebuilds all component views and revalidates component
  payloads. The stored field value itself already has an instance cache
  with rollback invalidation. A third host capture timed each component
  validator. Descriptor validation accounted for 92.6% of dynamic
  `getCap()` time in both workloads. It scans the package's field, method
  and constant-pool type references across multiple bitmap windows on
  each parse, even when the caller needs a single field token.
- **The ARM interpreter has additional per-bytecode costs.** The normal
  ELF calls a watchdog reload on every iteration and writes five
  watchdog scratch registers after each fetch in the applet path.
  Dispatch performs a checked fetch, an out-of-line decoder call,
  then an indirect member-function call. These are potential costs after
  CAP validation is amortized. Their share of ARM time was unmeasured.

Relevant code paths are
[`bc_getfield_a_this` and field resolution](../choupi/src/jc_bytecodes/bc_object_field.cpp#L1234),
[`getCap`](../choupi/src/jc_handlers/flashmemory.cpp#L3045),
[`JC_Cap` construction](../choupi/src/jc_handlers/jc_cap.cpp#L719),
[Descriptor type-offset validation](../choupi/src/jc_handlers/jc_cap.cpp#L412),
[the reference value cache](../choupi/src/jc_types/jc_instance.cpp#L562)
and [the interpreter loop](../choupi/src/interpretor.cpp#L393).

## Speedup candidates in order

1. **Reuse a validated CAP view per package generation.** Amortize component
   validation across bytecode execution. An APDU-scoped cache is a bounded
   first experiment. A longer-lived cache needs a package generation beyond
   package ID, pointer and length, since GP DELETE and LOAD can reuse a slot
   and address. Preserve validation on initial use and after package changes
   or boot recovery. Keep all live view bounds and runtime trust checks.
   Measure misses, metadata bytes, and time on the same 0/1/5/50-repeat APDUs.
2. **Cache checked field resolution if it remains hot.** For a bounded
   cache of the resolved field offset, include calling package generation,
   constant-pool entry, expected field kind and receiver class identity.
   Read the field value on every access and preserve current reference
   ownership, proxy, receiver and ROM-field checks. Repeat with static-field
   resolution and method linkage only if they become material.
3. **Measure and reduce interpreter loop overhead after re-profiling.**
   Quantify watchdog reloads and scratch writes separately on RP2350.
   Consider servicing them at a bounded cadence that preserves the
   watchdog deadline, recovery breadcrumbs and CCID progress. A direct
   opcode dispatch table, fewer repeated frame lookups and hot-code
   placement are further experiments. Keep bytecode fetch and branch
   bounds and check the firmware flash and RAM budgets.

The host profile makes the first experiment the priority. Hardware
speedup is still unmeasured. No production interpreter changes were made.

## Reproduction

Use `./tools/fixture-path --verify` and
`./tools/build-pc --profile max_crypto --rp2350-overlay`, then run

```sh
./tools/env python3 tests/integration/jcalgtest_bytecode_benchmark.py \
  artifacts/results/jcvm-bytecode-baseline
```

The runner logs the exact APDUs for replay with
[`ApduTimer`](../tests/hardware/ApduTimer.java) against an identified
Pico 2. Host timing and diagnostic binaries were built separately. The
initial external Callgrind attempt failed on the emulator's ptrace/SIGTRAP
protocol, so handler and CAP timings used a source copy with sampling
probes. The benchmark runner accepts that binary with `--profile-emulator`.

Local measurement logs, build provenance, the ARM disassembly, and the
sampling diffs are under
`artifacts/build-audit/bytecode-performance-20261122/`. The unmodified
host timing is in `host-timing.log`. The physical APDU and serial timing
are in `hardware-ee11.log` and `serial-ee11.log`.
