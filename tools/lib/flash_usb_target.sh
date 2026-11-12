# USB target selection shared by tools/flash and its offline selection tests.
# The caller provides serial, chipid, usb_path and target_* state.

flash_read_bootsel_chipid() {
  local device="$1"
  local bus address info
  bus="$(<"${device}/busnum")"
  address="$(<"${device}/devnum")"
  info="$(timeout --signal=TERM --kill-after=2s 4s \
      picotool info --device --bus "$bus" --address "$address" 2>&1)" ||
    return 1
  sed -nE \
    's/^[[:space:]]*chipid:[[:space:]]*0x?([[:xdigit:]]{16})[[:space:]]*$/\1/p' \
    <<<"$info" | tr '[:lower:]' '[:upper:]'
}

flash_set_identity_error() {
  target_identity_error="$1"
  return 1
}

capture_usb_device() {
  local device="$1"
  local observed_serial=""
  target_sysfs_path="${device##*/}"
  target_bus="$(<"${device}/busnum")"
  target_address="$(<"${device}/devnum")"
  target_product="$(<"${device}/idProduct")"
  if [[ -r "${device}/serial" ]]; then
    observed_serial="$(<"${device}/serial")"
  fi

  if ((target_selected == 0)); then
    target_expected_usb_serial="${observed_serial^^}"
    if [[ -n "$chipid" ]]; then
      target_expected_chipid="$chipid"
    elif [[ "$serial" =~ ^[[:xdigit:]]{16}$ ]]; then
      target_expected_chipid="${serial^^}"
    elif [[ "$observed_serial" =~ ^[[:xdigit:]]{16}$ ]]; then
      target_expected_chipid="${observed_serial^^}"
    fi
    target_selected=1
  fi
  # Retain the last runtime USB serial for the CDC reset fallback when the
  # board re-enumerates in BOOTSEL without a USB serial descriptor.
  if [[ -n "$observed_serial" ]]; then
    target_device_serial="$observed_serial"
  fi
}

flash_validate_bootsel_device() {
  local device="$1"
  local actual_chipid expected_chipid="$target_expected_chipid"
  actual_chipid="$(flash_read_bootsel_chipid "$device")" ||
    return 1

  if [[ -n "$chipid" ]]; then
    expected_chipid="$chipid"
  elif [[ -n "$serial" ]]; then
    if [[ "$serial" =~ ^[[:xdigit:]]{16}$ ]]; then
      expected_chipid="${serial^^}"
    elif [[ -z "$expected_chipid" ]]; then
      flash_set_identity_error \
        "cannot verify BOOTSEL identity for non-chip serial ${serial}"
      return 1
    fi
  fi

  if [[ -n "$expected_chipid" && "$actual_chipid" != "${expected_chipid^^}" ]]; then
    flash_set_identity_error \
      "BOOTSEL identity mismatch expected=${expected_chipid^^} actual=${actual_chipid}"
    return 1
  fi
  if [[ -z "$expected_chipid" ]]; then
    if ((target_selected != 0)); then
      flash_set_identity_error \
        "cannot verify re-enumerated BOOTSEL device identity at ${target_sysfs_path}"
      return 1
    fi
    target_expected_chipid="$actual_chipid"
  fi
  return 0
}

flash_validate_runtime_device() {
  local device="$1"
  local device_serial=""
  [[ -r "${device}/serial" ]] && device_serial="$(<"${device}/serial")"
  device_serial="${device_serial^^}"

  if [[ -n "$serial" && "$device_serial" != "$serial" ]]; then
    flash_set_identity_error \
      "requested serial ${serial} does not match USB path ${device##*/}"
    return 1
  fi
  if [[ -n "$chipid" && "$device_serial" != "$chipid" ]]; then
    flash_set_identity_error \
      "requested chip ID ${chipid} does not match USB path ${device##*/}"
    return 1
  fi
  if ((target_selected != 0)); then
    if [[ -n "$target_expected_usb_serial" &&
          "$device_serial" != "$target_expected_usb_serial" ]]; then
      flash_set_identity_error \
        "runtime identity changed at USB path ${device##*/}"
      return 1
    fi
    if [[ -z "$target_expected_usb_serial" &&
          -z "$target_expected_chipid" ]]; then
      flash_set_identity_error \
        "cannot verify runtime identity at USB path ${device##*/}"
      return 1
    fi
    if [[ -n "$target_expected_chipid" &&
          "$device_serial" != "$target_expected_chipid" ]]; then
      flash_set_identity_error \
        "runtime chip ID changed at USB path ${device##*/}"
      return 1
    fi
  fi
  return 0
}

refresh_target_usb() {
  local sysfs_root="${1:-/sys/bus/usb/devices}"
  local device device_serial observed_chipid
  local -a bootsel_devices=() serial_devices=() chipid_runtime_devices=() rp_devices=()
  target_identity_error=""

  if [[ -n "$target_sysfs_path" ]]; then
    device="${sysfs_root}/${target_sysfs_path}"
    if [[ -r "${device}/idVendor" && "$(<"${device}/idVendor")" == 2e8a ]]; then
      if [[ "$(<"${device}/idProduct")" == 000f ]]; then
        flash_validate_bootsel_device "$device" || return 1
      else
        flash_validate_runtime_device "$device" || return 1
      fi
      capture_usb_device "$device"
      return 0
    fi
    if [[ -n "$usb_path" ]]; then
      return 1
    fi
    if ((target_selected != 0)); then
      return 1
    fi
  fi

  for device in "${sysfs_root}"/*; do
    [[ -r "${device}/idVendor" && -r "${device}/idProduct" &&
       -r "${device}/busnum" && -r "${device}/devnum" ]] || continue
    [[ "$(<"${device}/idVendor")" == 2e8a ]] || continue
    rp_devices+=("${device}")
    if [[ "$(<"${device}/idProduct")" == 000f ]]; then
      bootsel_devices+=("${device}")
      continue
    fi
    if [[ -r "${device}/serial" ]]; then
      device_serial="$(<"${device}/serial")"
      device_serial="${device_serial^^}"
      [[ -z "$serial" || "$device_serial" != "$serial" ]] ||
        serial_devices+=("${device}")
      [[ -z "$chipid" || "$device_serial" != "$chipid" ]] ||
        chipid_runtime_devices+=("${device}")
    fi
  done

  if [[ -n "$chipid" ]]; then
    if ((${#chipid_runtime_devices[@]} > 1)); then
      flash_set_identity_error "chip ID ${chipid} matches multiple runtime devices"
      return 1
    elif ((${#chipid_runtime_devices[@]} == 1)); then
      capture_usb_device "${chipid_runtime_devices[0]}"
      return 0
    fi
  fi

  if [[ -n "$serial" ]]; then
    if ((${#serial_devices[@]} > 1)); then
      flash_set_identity_error "serial ${serial} matches multiple runtime devices"
      return 1
    elif ((${#serial_devices[@]} == 1)); then
      capture_usb_device "${serial_devices[0]}"
      return 0
    elif [[ ! "$serial" =~ ^[[:xdigit:]]{16}$ ]]; then
      flash_set_identity_error "requested serial ${serial} is not connected"
      return 1
    fi
  fi

  if [[ -n "$chipid" || "$serial" =~ ^[[:xdigit:]]{16}$ ]]; then
    local expected_chipid="$chipid"
    [[ -n "$expected_chipid" ]] || expected_chipid="${serial^^}"
    local -a matching_bootsel_devices=()
    for device in "${bootsel_devices[@]}"; do
      observed_chipid="$(flash_read_bootsel_chipid "$device")" || continue
      [[ "$observed_chipid" != "$expected_chipid" ]] ||
        matching_bootsel_devices+=("${device}")
    done
    if ((${#matching_bootsel_devices[@]} > 1)); then
      flash_set_identity_error \
        "chip ID ${expected_chipid} matches multiple BOOTSEL devices"
      return 1
    elif ((${#matching_bootsel_devices[@]} == 1)); then
      target_expected_chipid="$expected_chipid"
      capture_usb_device "${matching_bootsel_devices[0]}"
      return 0
    fi
    if [[ -n "$chipid" || -n "$serial" ]]; then
      flash_set_identity_error \
        "requested identity ${expected_chipid} is not connected"
      return 1
    fi
  elif [[ -n "$serial" ]]; then
    flash_set_identity_error \
      "requested serial ${serial} cannot be matched to a BOOTSEL chip ID"
    return 1
  fi

  if ((${#bootsel_devices[@]} == 1)); then
    device="${bootsel_devices[0]}"
    observed_chipid="$(flash_read_bootsel_chipid "$device")" || return 1
    target_expected_chipid="$observed_chipid"
    capture_usb_device "$device"
    return 0
  fi
  if ((${#rp_devices[@]} == 1)); then
    capture_usb_device "${rp_devices[0]}"
    return 0
  fi
  return 1
}
