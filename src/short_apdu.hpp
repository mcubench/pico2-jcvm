#ifndef PICO2_JCVM_SHORT_APDU_HPP
#define PICO2_JCVM_SHORT_APDU_HPP

#include <array>
#include <cstdint>
#include <cstring>

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

class ShortApduResponseChain {
public:
  static constexpr uint16_t capacity = 1022;

  bool queue(const uint8_t *data, const uint16_t length) noexcept {
    this->length_ = 0;
    this->offset_ = 0;
    if ((data == nullptr && length != 0) || length > this->data_.size()) {
      return false;
    }
    if (length != 0) {
      std::memcpy(this->data_.data(), data, length);
    }
    this->length_ = length;
    this->offset_ = 0;
    return true;
  }

  bool has_pending() const noexcept { return this->offset_ < this->length_; }

  uint16_t take(const uint16_t expected_length, uint8_t *output,
                const uint16_t output_capacity,
                uint16_t &status_word) noexcept {
    if (!this->has_pending() || output == nullptr || expected_length == 0 ||
        output_capacity == 0) {
      return 0;
    }
    const uint16_t remaining = this->length_ - this->offset_;
    const uint16_t count = remaining < expected_length
                               ? remaining
                               : expected_length < output_capacity
                                     ? expected_length
                                     : output_capacity;
    std::memcpy(output, this->data_.data() + this->offset_, count);
    this->offset_ = static_cast<uint16_t>(this->offset_ + count);
    const uint16_t bytes_left = this->length_ - this->offset_;
    if (bytes_left == 0) {
      status_word = 0x9000;
      this->length_ = 0;
      this->offset_ = 0;
    } else {
      const uint16_t sw2 = bytes_left >= 256 ? 0 : bytes_left;
      status_word = static_cast<uint16_t>(0x6100 | sw2);
    }
    return count;
  }

private:
  std::array<uint8_t, capacity> data_{};
  uint16_t length_ = 0;
  uint16_t offset_ = 0;
};

} // namespace pico2_jcvm

#endif // PICO2_JCVM_SHORT_APDU_HPP
