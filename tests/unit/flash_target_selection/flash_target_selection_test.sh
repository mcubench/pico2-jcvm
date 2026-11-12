#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
temporary_dir="$(mktemp -d)"
trap 'rm -rf "$temporary_dir"' EXIT
root="${temporary_dir}/devices"
chipid_map="${temporary_dir}/chipids"
mkdir -p "$root" "$chipid_map"

serial=""
chipid=""
usb_path=""
target_sysfs_path=""
target_bus=""
target_address=""
target_product=""
target_device_serial=""
target_expected_usb_serial=""
target_expected_chipid=""
target_selected=0
target_identity_error=""

failures=0
expect() {
  local condition="$1" name="$2"
  if [[ "$condition" == true ]]; then
    printf 'TEST:PASS %s\n' "$name"
  else
    printf 'TEST:FAIL %s\n' "$name"
    failures=$((failures + 1))
  fi
}

timeout() {
  shift 3
  "$@"
}

picotool() {
  [[ "$1" == info ]] || return 1
  shift
  local bus="" address=""
  while (($#)); do
    case "$1" in
      --bus) bus="$2"; shift 2 ;;
      --address) address="$2"; shift 2 ;;
      *) shift ;;
    esac
  done
  local id_file="${chipid_map}/${bus}-${address}"
  [[ -r "$id_file" ]] || return 1
  printf 'chipid: 0x%s\n' "$(<"$id_file")"
}

reset_selection() {
  serial=""
  chipid=""
  usb_path=""
  target_sysfs_path=""
  target_bus=""
  target_address=""
  target_product=""
  target_device_serial=""
  target_expected_usb_serial=""
  target_expected_chipid=""
  target_selected=0
  target_identity_error=""
  rm -rf "${root:?}"/* "${chipid_map:?}"/*
}

add_device() {
  local path="$1" product="$2" serial_value="$3" bus="$4" address="$5" chip="$6"
  local device="${root}/${path}"
  mkdir -p "$device"
  printf '2e8a\n' > "${device}/idVendor"
  printf '%s\n' "$product" > "${device}/idProduct"
  printf '%s\n' "$bus" > "${device}/busnum"
  printf '%s\n' "$address" > "${device}/devnum"
  [[ -z "$serial_value" ]] || printf '%s\n' "$serial_value" > "${device}/serial"
  [[ -z "$chip" ]] || printf '%s\n' "$chip" > "${chipid_map}/${bus}-${address}"
}

source "${repo_root}/tools/lib/flash_usb_target.sh"

# An absent explicit serial must not fall back to the only other Pico.
reset_selection
serial="AAAAAAAAAAAAAAAA"
add_device "1-4" 0009 BBBBBBBBBBBBBBBB 1 4 BBBBBBBBBBBBBBBB
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false && "$target_identity_error" == *"not connected"* ]] && echo true || echo false)" \
  "explicit_serial_absent_does_not_select_sole_other_device"

# Duplicate runtime serials are ambiguous even when one device would be a
# tempting fallback by USB order or topology.
reset_selection
serial="AAAAAAAAAAAAAAAA"
add_device "1-3" 0009 AAAAAAAAAAAAAAAA 1 3 AAAAAAAAAAAAAAAA
add_device "1-4" 0009 AAAAAAAAAAAAAAAA 1 4 AAAAAAAAAAAAAAAA
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false && "$target_identity_error" == *"multiple runtime"* ]] && echo true || echo false)" \
  "duplicate_explicit_serial_is_rejected"

# Explicit serial selects only its matching runtime device.
reset_selection
serial="AAAAAAAAAAAAAAAA"
add_device "1-3" 0009 AAAAAAAAAAAAAAAA 1 3 AAAAAAAAAAAAAAAA
add_device "1-4" 0009 BBBBBBBBBBBBBBBB 1 4 BBBBBBBBBBBBBBBB
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == true && "$target_sysfs_path" == 1-3 && "$target_bus" == 1 && "$target_address" == 3 ]] && echo true || echo false)" \
  "explicit_serial_selects_exact_runtime_device"

# Replacing the runtime device at the remembered path with another serial is
# rejected before any later flash operation can use that path.
device="${root}/1-3"
printf 'BBBBBBBBBBBBBBBB\n' > "${device}/serial"
printf '8\n' > "${device}/devnum"
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false && "$target_identity_error" == *"does not match"* && "$target_address" == 3 ]] && echo true || echo false)" \
  "replaced_runtime_device_at_stable_path_is_rejected"

# Restore the original runtime identity for the BOOTSEL re-enumeration cases.
printf 'AAAAAAAAAAAAAAAA\n' > "${device}/serial"
printf '3\n' > "${device}/devnum"
target_identity_error=""

# A selected runtime board must retain identity after re-enumeration. A
# different BOOTSEL chip ID at the same USB path is a replacement, not a match.
device="${root}/1-3"
rm -f "${device}/idProduct" "${device}/serial" "${device}/busnum" "${device}/devnum"
printf '000f\n' > "${device}/idProduct"
printf '1\n' > "${device}/busnum"
printf '9\n' > "${device}/devnum"
printf 'BBBBBBBBBBBBBBBB\n' > "${chipid_map}/1-9"
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false && "$target_identity_error" == *"identity mismatch"* ]] && echo true || echo false)" \
  "replaced_bootsel_device_at_stable_path_is_rejected"

# The same path is accepted when the ROM chip ID proves it is the original.
printf 'AAAAAAAAAAAAAAAA\n' > "${chipid_map}/1-9"
target_identity_error=""
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == true && "$target_expected_chipid" == AAAAAAAAAAAAAAAA && "$target_address" == 9 ]] && echo true || echo false)" \
  "original_bootsel_device_at_stable_path_is_verified"

# A direct serial request in BOOTSEL is resolvable only when it names the
# sixteen-digit chip ID reported by the ROM.
reset_selection
serial="CCCCCCCCCCCCCCCC"
add_device "1-5" 000f "" 1 5 CCCCCCCCCCCCCCCC
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == true && "$target_sysfs_path" == 1-5 ]] && echo true || echo false)" \
  "explicit_chip_serial_selects_matching_bootsel_device"

reset_selection
serial="CCCCCCCCCCCCCCCC"
add_device "1-5" 000f "" 1 5 DDDDDDDDDDDDDDDD
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false && "$target_identity_error" == *"not connected"* ]] && echo true || echo false)" \
  "explicit_chip_serial_rejects_mismatched_bootsel_device"

# Automatic selection remains available only when unambiguous.
reset_selection
add_device "1-3" 000f "" 1 3 1111111111111111
add_device "1-4" 000f "" 1 4 2222222222222222
selected=false
refresh_target_usb "$root" && selected=true
expect "$([[ "$selected" == false ]] && echo true || echo false)" \
  "automatic_selection_rejects_multiple_bootsel_devices"

printf 'TEST:SUMMARY fail=%d\n' "$failures"
((failures == 0))
