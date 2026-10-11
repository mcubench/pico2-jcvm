#ifndef PICO2_JCVM_SCP03_HPP
#define PICO2_JCVM_SCP03_HPP

#include <cstddef>
#include <cstdint>

class Scp03Session {
public:
  void reset() noexcept;

  // Process an ISO 7816 INITIALIZE UPDATE APDU and return response data
  // (without SW1/SW2). Only the default 8-byte SCP03 challenge is supported.
  bool initializeUpdate(const uint8_t *apdu, size_t apdu_length,
                        uint8_t *response, size_t response_capacity,
                        size_t &response_length) noexcept;

  // Process EXTERNAL AUTHENTICATE, including its SCP03 C-MAC.
  bool externalAuthenticate(const uint8_t *apdu,
                            size_t apdu_length) noexcept;

  // Verify a MAC-protected command and return the unwrapped data length.
  bool verifyCommand(const uint8_t *apdu, size_t apdu_length,
                     size_t &data_length) noexcept;

  bool active() const noexcept { return active_; }

private:
  bool initialized_ = false;
  bool active_ = false;
  uint8_t host_challenge_[8]{};
  uint8_t card_challenge_[8]{};
  uint8_t session_mac_key_[16]{};
  uint8_t chaining_value_[16]{};

  bool verifyAndAdvance(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                        const uint8_t *data, size_t data_length,
                        const uint8_t *received_mac) noexcept;
};

#endif
