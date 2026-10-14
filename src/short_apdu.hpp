#ifndef PICO2_JCVM_SHORT_APDU_HPP
#define PICO2_JCVM_SHORT_APDU_HPP

#include <cstdint>

namespace pico2_jcvm {

struct ShortApduCommand {
  uint16_t incoming_length = 0;
  uint16_t expected_response_length = 0;
  bool has_expected_response_length = false;
};

// Parse ISO 7816 short APDU cases 1, 2S, 3S, and 4S. Extended APDUs are not
// supported by the current CCID bridge and are rejected here.
inline bool parse_short_apdu(const uint8_t *command, const uint16_t length,
                             ShortApduCommand &parsed) noexcept {
  parsed = {};
  if (command == nullptr || length < 4) {
    return false;
  }
  if (length == 4) {
    return true; // Case 1
  }
  if (length == 5) {
    parsed.has_expected_response_length = true; // Case 2S
    parsed.expected_response_length = command[4] == 0 ? 256 : command[4];
    return true;
  }

  const uint16_t incoming_length = command[4];
  if (incoming_length == 0) {
    return false; // Extended-length marker; not supported by this bridge.
  }
  if (length == static_cast<uint16_t>(5 + incoming_length)) {
    parsed.incoming_length = incoming_length; // Case 3S
    return true;
  }
  if (length == static_cast<uint16_t>(6 + incoming_length)) {
    parsed.incoming_length = incoming_length; // Case 4S
    parsed.has_expected_response_length = true;
    const uint8_t le = command[length - 1];
    parsed.expected_response_length = le == 0 ? 256 : le;
    return true;
  }
  return false;
}

} // namespace pico2_jcvm

#endif // PICO2_JCVM_SHORT_APDU_HPP
