#!/usr/bin/env bash

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
local_root="${repo_root}/.local-toolchain"
tool_root="${local_root}/root"
export PICO_HOME="${PICO_HOME:-${local_root}}"
export PICO_SDK_PATH="${PICO_SDK_PATH:-${PICO_HOME}/sdk}"
export MAVEN_OPTS="${MAVEN_OPTS:-} -Dmaven.repo.local=${local_root}/m2"
export PATH="${tool_root}/usr/bin:${tool_root}/pioasm:${tool_root}/picotool:${local_root}/maven/apache-maven-3.10.0/bin:${local_root}/toolchain/riscv/bin:${PATH}"
export LD_LIBRARY_PATH="${tool_root}/usr/lib/x86_64-linux-gnu:${local_root}/toolchain/riscv/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_host_usb_access() {
  local no_new_privileges
  no_new_privileges="$(awk '/^NoNewPrivs:/ { print $2 }' /proc/self/status 2>/dev/null || true)"
  if [[ "${no_new_privileges}" == 1 ]]; then
    die "USB/PCSC access is unavailable in this restricted context; rerun via the host-access execution path"
  fi
}

require_sdk() {
  [[ -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]] || \
    die "Pico SDK not found at ${PICO_SDK_PATH} (set PICO_SDK_PATH or PICO_HOME)"
}
