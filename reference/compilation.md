---
name: pico2-compilation
description: Build this repository for the Pico 2 RP2350 using its ARM Cortex-M33 or Hazard3 RISC-V configuration, including build environment diagnosis and artifact location.
---

# Pico 2 compilation

Use the repository wrapper for every build. Do not invoke a compiler, CMake, or
Ninja directly: `tools/build` selects a matched SDK and toolchain and configures
the target consistently.

## Architectures and commands

| Command | Pico SDK platform | Compiler prefix | Build directory |
| --- | --- | --- | --- |
| `./tools/build arm` | `rp2350-arm-s` | `arm-none-eabi` | `build/arm` |
| `./tools/build riscv` | `rp2350-riscv` | `riscv32-pico-elf` | `build/riscv` |

Each architecture has its own CMake cache and output directory. The wrapper
configures Ninja and a Release build for `PICO_BOARD=pico2`, then builds
`pico2_jcvm_smoke`. The UF2 artifact is
`build/<arch>/pico2_jcvm_smoke.uf2`.

After changing platform-independent source, build both architectures:

```sh
./tools/build arm
./tools/build riscv
```

The project enables `-Wall -Wextra -Werror`; treat any warning or failed build
as a failure and fix it before flashing.

## Build environment

`tools/common.sh` locates dependencies in this order:

- Pico SDK: a valid `PICO_SDK_PATH`, otherwise the newest SDK under
  `${PICO_HOME:-$HOME/.pico-sdk}/sdk`.
- CMake, Ninja, and `picotool`: first the command on `PATH`, then the newest
  managed installation under `$PICO_HOME`.
- Cross toolchains: the newest matching compiler under
  `$PICO_HOME/toolchain`.

The build wrapper requires the SDK, CMake, Ninja, the architecture's matching
toolchain, and `picotool`. If a dependency is missing, run `./tools/doctor` to
see which host components are unavailable. Set `PICO_SDK_PATH` to the SDK root
or `PICO_HOME` to the managed tools root when using a non-default installation.

## Build versus flash

`./tools/build <arch>` only configures and builds. `./tools/cycle <arch>` builds
both architectures, flashes the requested one, and captures runtime output. Use
the latter only when a hardware cycle is requested and the repository's safety
conditions for flashing are met. In particular, do not flash after a shared
source change until both builds pass. Never use commands that alter OTP, boot
security, flash partition tables, or machine configuration.
