# Java Card bytecode performance on Pico 2

## Implemented fix and measured result

The VM now reuses validated CAP metadata and checked reference-field
offsets. On the same physical Pico 2, software XOR ran **23.9× faster**
and software AES ran **5.92× faster**. The 50-operation software AES APDU
fell from **126.029 s to 21.344 s**.

| Workload, 16 bytes | Before, net ms/op | After, net ms/op | Speedup |
| --- | ---: | ---: | ---: |
| `SWAlgs_xor()` | 34.86 | 1.46 | 23.9× |
| `SWAlgs_AES()` | 2,518.08 | 425.68 | 5.92× |
| Native-backed AES-128 ECB `Cipher.doFinal()` | 7.80 | 3.24 | 2.41× |

- **CAP views.** Each uploaded package slot has one cache entry. The
  first runtime lookup in a CAP store revision runs the existing full
  structural parser. Later lookups reuse its validated component view.
  The store advances its revision before slot changes and rechecks stored
  code integrity before publishing views. Slot reuse at the same address
  and length therefore requires fresh validation. The initial APDU-only
  experiment was extended to reuse across APDUs in `cceada94`.
- **Reference-field offsets.** Uploaded `getfield_a_this` instructions
  reuse checked offsets within one APDU, keyed by the calling package,
  constant-pool index, and the live receiver's concrete package and class.
  A cache miss uses the full field resolver. Each access still checks the
  live reference, reads the current field value, and propagates its
  owner. This change is in `a2a69d95`.

The combined caches and revision state use **745 bytes of static ARM
RAM**. LOAD, INSTALL, and restore retain their preflight checks. The
runtime still checks method bounds, fetches, branches, and access to live
objects. The native-backed speedup is for the complete Java Card API
invocation, including its applet and VM overhead. See the
[physical A/B details](#physical-ab-after-cap-caching) and the
[execution trust model](../CAP_EXECUTION_TRUST_MODEL.md).

## Original baseline and profiling

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
[`bc_getfield_a_this` and field resolution](../../choupi/src/jc_bytecodes/bc_object_field.cpp#L1234),
[`getCap`](../../choupi/src/jc_handlers/flashmemory.cpp#L3045),
[`JC_Cap` construction](../../choupi/src/jc_handlers/jc_cap.cpp#L719),
[Descriptor type-offset validation](../../choupi/src/jc_handlers/jc_cap.cpp#L412),
[the reference value cache](../../choupi/src/jc_types/jc_instance.cpp#L562)
and [the interpreter loop](../../choupi/src/interpretor.cpp#L393).

## Original optimization proposals

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

The first two proposals led to the caches described above. Changes to
dispatch, branch checks, and watchdog cadence remain open pending
measurement of their costs on the RP2350.

## Reproduction

Use `./tools/fixture-path --verify` and
`./tools/build-pc --profile max_crypto --rp2350-overlay`, then run

```sh
./tools/env python3 tests/integration/jcalgtest_bytecode_benchmark.py \
  artifacts/results/jcvm-bytecode-baseline
```

The runner logs the exact APDUs for replay with
[`ApduTimer`](../../tests/hardware/ApduTimer.java) against an identified
Pico 2. Host timing and diagnostic binaries were built separately. The
initial external Callgrind attempt failed on the emulator's ptrace/SIGTRAP
protocol, so handler and CAP timings used a source copy with sampling
probes. The benchmark runner accepts that binary with `--profile-emulator`.

Local measurement logs, build provenance, the ARM disassembly, and the
sampling diffs are under
`artifacts/build-audit/bytecode-performance-20261122/`. The unmodified
host timing is in `host-timing.log`. The physical APDU and serial timing
are in `hardware-ee11.log` and `serial-ee11.log`.

## Physical A/B after CAP caching

The comparison used the same Pico 2, EE116F37C7E4A04E, and the same
installed JCAlgTest 1.8.5/jc305 applet. Both were normal ARM `max_crypto`
images. Each run began with a reader reset and used the same prepare and
measurement APDUs. The standard GlobalPlatformPro install omitted the
Descriptor component. The development flash preserved card storage.

Net time is the difference between the 50-repeat APDU and the median
zero-repeat APDU, divided by 50. XOR and native AES had three 50-repeat
samples per image. Software AES had one, plus three single-repeat samples
and one five-repeat sample per image. The optimized software AES net time
was 429.0 ms/op at one repeat and 425.8 ms/op at five repeats.

Serial telemetry for the full software AES APDU measured 126.022295 s
and 21.338876 s inside the VM, with
3.280 and 3.519 ms in flash sync. Each APDU journaled 768 bytes and wrote
no full snapshot.

CPLC identified the baseline as `0cf976ddba5b8a3f` and the candidate as
`a2a69d952c309690`. The baseline UF2 SHA-256 was
`effccdec0c2b3319d02db2955a94451e07aa8e396c5ccf9d6514a693603acda6`.
The candidate UF2 SHA-256 was
`cf1ea6b55d560c9a053a8dd706b284f3f3abd87865089dc0140a51372af385d7`.
The full JCVM flash and runtime cycle passed. Both runs used reader
`Pico 2 CHOUPI CCID [JCVM CCID] (EE116F37C7E4A04E) 00 00`.
All 35 workload and prepare APDUs on each image returned `AA9000`.
The timings measure the combined CAP-view and field-offset caches.

The APDU logs, serial captures, and checked timing summary are in
`artifacts/build-audit/cap-view-cache-20261122/`.
