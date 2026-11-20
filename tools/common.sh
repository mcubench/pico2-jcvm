#!/usr/bin/env bash

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
local_root="${repo_root}/.local-toolchain"
tool_root="${local_root}/root"
export PICO_HOME="${PICO_HOME:-${local_root}}"
# The repo pins a TinyUSB-patched SDK; ignore an ambient PICO_SDK_PATH (e.g. from
# ~/.bashrc). Override explicitly with PICO2_SDK_PATH.
export PICO_SDK_PATH="${PICO2_SDK_PATH:-${PICO_HOME}/sdk}"
export MAVEN_OPTS="${MAVEN_OPTS:-} -Dmaven.repo.local=${local_root}/m2"
source "${repo_root}/tools/toolchain-path.sh"
toolchain_select_path "${local_root}"

die() {
  printf 'ERROR: %s\n' "$*" >&2
  exit 1
}

require_command() {
  command -v "$1" >/dev/null 2>&1 || die "required command not found: $1"
}

require_host_usb_access() {
  local no_new_privileges
  if [[ "${PICO2_HOST_USB_ACCESS:-}" != 1 ]]; then
    die "USB/PCSC commands require the explicit host-access marker; rerun this command with sandbox_permissions=require_escalated and PICO2_HOST_USB_ACCESS=1"
  fi
  no_new_privileges="$(awk '/^NoNewPrivs:/ { print $2; found=1 } END { if (!found) exit 1 }' /proc/self/status 2>/dev/null)" || \
    die "cannot verify USB/PCSC execution context; rerun with sandbox_permissions=require_escalated"
  if [[ "${no_new_privileges}" != 0 ]]; then
    die "USB/PCSC access is unavailable in this restricted context; rerun with sandbox_permissions=require_escalated"
  fi
}

require_sdk() {
  [[ -f "${PICO_SDK_PATH}/external/pico_sdk_import.cmake" ]] || \
    die "Pico SDK not found at ${PICO_SDK_PATH} (set PICO_SDK_PATH or PICO_HOME)"
}

# Restart pcscd through the approved helper only when it is needed and safe.
# The helper restarts pcscd.service, which drops every open PC/SC session on
# every token, including other sessions' campaigns, and even a reader listing
# can disturb them (AGENTS.md). While a JCAlgTest campaign or serial monitor
# is live, do neither: the caller's first GPPro command reports a missing
# reader. Otherwise list the readers and restart only if the exact reader is
# absent.
pcsc_refresh_if_reader_missing() {
  local reader="$1"
  local helper="${2:-/usr/local/sbin/pico2-pcsc-reader}"
  local listing
  if pgrep -f '[A]lgTestJClient|[j]calgtest-apdu-watchdog|tools/[m]onitor' \
       >/dev/null 2>&1; then
    printf 'PCSC:REFRESH_SKIPPED reason=live_campaign reader=%s\n' "${reader}" >&2
    return 0
  fi
  listing="$("${repo_root}/tools/list-readers" 2>/dev/null || true)"
  if cut -f2- <<<"${listing}" | grep -Fxq -- "${reader}"; then
    return 0
  fi
  PICO2_HOST_USB_ACCESS=1 sudo -n "${helper}"
}
