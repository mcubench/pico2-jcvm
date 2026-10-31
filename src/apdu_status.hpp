#ifndef PICO2_JCVM_APDU_STATUS_HPP
#define PICO2_JCVM_APDU_STATUS_HPP

#include <cstddef>
#include <cstdint>

// A SELECT response with SW1=61 means the applet is selected and has more
// response data available through GET RESPONSE.
constexpr bool select_status_keeps_applet_selected(uint16_t status) noexcept {
  return status == 0x9000 || (status & 0xff00) == 0x6100;
}

// Return the response-data limit encoded by a short APDU. For the status
// word, callers must reserve their own two bytes. Malformed/non-short frames
// use the provided implementation buffer limit instead.
constexpr size_t short_apdu_response_data_limit(
    const uint8_t *command, size_t command_length,
    size_t fallback_limit) noexcept {
  if (command == nullptr || command_length < 5) return fallback_limit;
  uint8_t encoded_le = 0;
  if (command_length == 5) {
    encoded_le = command[4];
  } else {
    // A short case-4 APDU has nonzero Lc and one trailing Le byte. Case-3
    // APDUs and extended-length encodings do not constrain this response.
    if (command[4] == 0 ||
        command_length != static_cast<size_t>(6 + command[4])) {
      return fallback_limit;
    }
    encoded_le = command[command_length - 1];
  }
  const size_t le = encoded_le == 0 ? 256 : encoded_le;
  return le < fallback_limit ? le : fallback_limit;
}

#endif
