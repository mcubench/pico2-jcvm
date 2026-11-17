#!/usr/bin/env bash
# Shared toolchain PATH selection, sourced by tools/env and tools/common.sh.
# Usage: toolchain_select_path <local_root>   (sets PATH, LD_LIBRARY_PATH,
# TOOLCHAIN_ARM_GNU and TOOLCHAIN_ROOT_USABLE)
#
# - An Arm GNU Toolchain under .local-toolchain/toolchain/arm-gnu-14.2 (or
#   $ARM_GNU_TOOLCHAIN) is preferred and is self-contained (own newlib).
# - The prebuilt .local-toolchain/root tree (cmake, ninja, gcc 14.2.1 from a
#   newer distro) is used only if its cmake runs on this host; otherwise the
#   host's cmake/ninja are used and its library dir is kept off
#   LD_LIBRARY_PATH so it cannot shadow system libraries.
toolchain_select_path() {
  local local_root="$1"
  local tool_root="${local_root}/root"
  local arm_gnu="${ARM_GNU_TOOLCHAIN:-${local_root}/toolchain/arm-gnu-14.2}"
  local front="" libs=""
  TOOLCHAIN_ARM_GNU=""
  if [[ -x "${arm_gnu}/bin/arm-none-eabi-gcc" ]]; then
    TOOLCHAIN_ARM_GNU="${arm_gnu}"
    front="${arm_gnu}/bin:"
  fi
  TOOLCHAIN_ROOT_USABLE=0
  if "${tool_root}/usr/bin/cmake" --version >/dev/null 2>&1; then
    TOOLCHAIN_ROOT_USABLE=1
    front="${front}${tool_root}/usr/bin:"
    libs="${tool_root}/usr/lib/x86_64-linux-gnu:"
  fi
  # Apache Ant (Java Card API build); the host may not provide one.
  [[ -x "${local_root}/ant/bin/ant" ]] && front="${front}${local_root}/ant/bin:"
  export PATH="${front}${tool_root}/pioasm:${tool_root}/picotool:${local_root}/maven/apache-maven-3.10.0/bin:${local_root}/toolchain/riscv/bin:${PATH}"
  export LD_LIBRARY_PATH="${libs}${local_root}/toolchain/riscv/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
  export TOOLCHAIN_ARM_GNU TOOLCHAIN_ROOT_USABLE
}
