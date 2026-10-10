# Repository-local RP2350 toolchain

The local host environment does not have system package installation rights,
and the repository instructions prohibit changing files outside this checkout.
The toolchain is therefore installed under `.local-toolchain/` and excluded
from Git by the root `.gitignore`.

Run build and diagnostic commands with the local environment wrapper:

```sh
./tools/env ./tools/doctor
./tools/env ./tools/build arm
./tools/env ./tools/build riscv
```

Installed or fetched locally:

- CMake 4.2.3, Ninja 1.13.2, Rust/Cargo stable 1.99.0 with
  `thumbv8m.main-none-eabihf`
- `arm-none-eabi-gcc` 14.2.1, binutils, Newlib, and the ARM C++ runtime
- `riscv32-pico-elf` GCC 16 and its matching target libraries
- Raspberry Pi Pico SDK 2.3.1 at commit `079c6f39023649b154152db30f1d781e884879bc`, including its submodules
- Raspberry Pi `picotool` 2.3.1 and `pioasm` 2.3.1 tools

The Pico SDK and Raspberry Pi binary tool archives were fetched from their
official GitHub releases. SHA-256 checks for `pico-sdk-tools-2.3.1-x86_64-lin`
(`39758f147a8e615f8c77586ce3c85a25c6728910a86612eea184fc86eb606992`),
`picotool-2.3.1-x86_64-lin`
(`878a1a1e7d8336b8be60300c7218f366d9790c0d3a211b181d6d172e80b1248e`), and
the RISC-V toolchain (`fc36f1b37f99de54a358115443638023de34e15fe199e718e50cca4d212e7e9f`)
matched release metadata. Rustup and its toolchains use local `RUSTUP_HOME` and
`CARGO_HOME` directories; no shell startup files were changed.
The complete local toolchain directory occupies 5.9 GiB.

The connected board reported RP2350 revision A4, QFN60, and 2048 KiB flash
(`FLASH_DEVINFO=0x0c00`) via read-only `picotool info -a`. Treat 2 MiB as the
working flash size until physical capacity is independently confirmed. The
current ARM smoke firmware was built and flashed after both architecture builds
passed. Its bounded 8-second CDC log is
`artifacts/logs/cycle-arm-20261010T053019Z.log`. Before the first flash, a
read-only 2 MiB image of the prior `pico2_btc_miner` firmware was saved at
`artifacts/backups/flash-before-smoke-20261010.bin` (SHA-256
`49a650c2093df2e2d528ec4ef9adce788db95133c7303b5f96d852d8b5ecc078`). The
flash size has not been independently measured. USB device access required a
reviewed escalation for board queries and cycles.

`tools/build`, `tools/cycle`, and `tools/monitor` now build the standalone
RP2350 smoke target, flash only after both ISA builds pass, and capture bounded
USB CDC output. The smoke result checks that `get_rand_32()` varies across a
small sample; it is not an entropy-quality assessment and does not validate the
CHOUPI VM. The OS, Java Card API, and ROM-mask source checkouts are now present
at the plan's pinned commits inside `choupi/`. The nested STM32CubeF4 submodule
is uninitialized and Maven is not installed, so the legacy dependency build has
not yet been validated.
