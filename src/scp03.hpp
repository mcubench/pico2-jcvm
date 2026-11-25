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

  // Verify a MAC-protected command, decrypt it under C-DECRYPTION, and return
  // the plaintext command data (APDU body or a session buffer) and length.
  bool verifyCommand(const uint8_t *apdu, size_t apdu_length,
                     size_t &data_length) noexcept {
    const uint8_t *data = nullptr;
    return verifyCommand(apdu, apdu_length, data, data_length);
  }
  bool verifyCommand(const uint8_t *apdu, size_t apdu_length,
                     const uint8_t *&data, size_t &data_length) noexcept;

  // Apply R-MAC / R-ENC to the response (data || SW) of the command that
  // verifyCommand last accepted; does nothing for any other response.
  // Error status words lose their data, as SCP03 sends them unprotected.
  void wrapResponse(uint8_t *response, size_t &response_length,
                    size_t response_capacity) noexcept;

  // Largest plaintext response data that still fits `limit` bytes once the
  // session's R-ENC padding and R-MAC are added.
  size_t responseDataLimit(size_t limit) const noexcept;

  // GlobalPlatform PUT KEY (INS D8) replacing the single SCP03 keyset with
  // three AES-128 keys wrapped under the current DEK. Returns the status word
  // and writes KVN || 3 KCVs to the response on success.
  uint16_t putKey(uint8_t p1, uint8_t p2, const uint8_t *data,
                  size_t data_length, uint8_t *response,
                  size_t response_capacity, size_t &response_length) noexcept;

  // Key version of the stored keyset; 0 means the default test keys.
  static uint8_t keyVersion() noexcept;

  bool active() const noexcept { return active_; }

private:
  bool initialized_ = false;
  bool active_ = false;
  uint8_t host_challenge_[8]{};
  uint8_t card_challenge_[8]{};
  bool wrap_pending_ = false;
  uint8_t security_level_ = 0;
  uint8_t session_enc_key_[16]{};
  uint8_t session_mac_key_[16]{};
  uint8_t session_rmac_key_[16]{};
  uint8_t chaining_value_[16]{};
  uint8_t encryption_counter_[16]{};
  uint8_t command_data_[240]{};

  bool verifyAndAdvance(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                        const uint8_t *data, size_t data_length,
                        const uint8_t *received_mac) noexcept;
};

#endif
