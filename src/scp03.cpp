#include "scp03.hpp"

#include <array>
#include <cstring>

#include "ffi.h"
#include "vm_filesystem.h"
#include "mbedtls/aes.h"
#include "mbedtls/platform_util.h"
#ifdef PC_VERSION
#include <random>
#else
#include "pico/rand.h"
#endif

#ifndef PC_VERSION
extern "C" void mbedtls_platform_zeroize(void *buffer, size_t length) {
  auto *bytes = static_cast<volatile unsigned char *>(buffer);
  while (length-- != 0) *bytes++ = 0;
}
#endif

namespace {
constexpr uint8_t kStaticKey[16] = {
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f};
constexpr size_t kMacLength = 8;
constexpr size_t kKeyLength = 16;
// Persistent keyset record set by PUT KEY: KVN || ENC || MAC || DEK. Absent
// means the default test key (INV-12) under KVN 0. One record, so a torn
// write leaves either the old or the new keyset.
constexpr uint8_t kKeysetTag[] = {0x02, 0xfe, 'G', 'P', 'K', 'E', 'Y', 'S'};
constexpr size_t kKeysetLength = 1 + 3 * kKeyLength;
constexpr size_t kEncOffset = 1;
constexpr size_t kMacOffset = kEncOffset + kKeyLength;
constexpr size_t kDekOffset = kMacOffset + kKeyLength;

// Load the keyset into `record`. A present but malformed record fails closed
// rather than falling back to the default key.
bool load_keyset(uint8_t record[kKeysetLength]) noexcept {
  uint32_t length = 0;
  const uint8_t status = fs_length(kKeysetTag, sizeof(kKeysetTag), &length);
  if (status == FS_STATUS_NO_SUCH_TAG) {
    record[0] = 0x00;
    for (size_t offset = kEncOffset; offset < kKeysetLength;
         offset += kKeyLength) {
      std::memcpy(record + offset, kStaticKey, kKeyLength);
    }
    return true;
  }
  return status == FS_STATUS_OK && length == kKeysetLength &&
         fs_read(kKeysetTag, sizeof(kKeysetTag), record, kKeysetLength) ==
             FS_STATUS_OK;
}

bool aes_cmac(const uint8_t key[16], const uint8_t *input, size_t input_length,
              uint8_t output[16]) noexcept {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  if (mbedtls_aes_setkey_enc(&aes, key, 128) != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }

  uint8_t zero[16]{};
  uint8_t l[16];
  uint8_t k1[16];
  uint8_t k2[16];
  uint8_t state[16]{};
  uint8_t block[16]{};
  if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, zero, l) != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }
  auto double_block = [](const uint8_t source[16], uint8_t destination[16]) {
    uint8_t carry = 0;
    for (int index = 15; index >= 0; --index) {
      const uint8_t next_carry = source[index] >> 7;
      destination[index] = static_cast<uint8_t>((source[index] << 1) | carry);
      carry = next_carry;
    }
    if (carry != 0) destination[15] ^= 0x87;
  };
  double_block(l, k1);
  double_block(k1, k2);

  const size_t block_count = input_length == 0 ? 1 : (input_length + 15) / 16;
  const bool complete_last_block = input_length != 0 && input_length % 16 == 0;
  for (size_t block_index = 0; block_index + 1 < block_count; ++block_index) {
    for (size_t byte = 0; byte < 16; ++byte) {
      block[byte] = static_cast<uint8_t>(state[byte] ^ input[block_index * 16 + byte]);
    }
    if (mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, block, state) != 0) {
      mbedtls_aes_free(&aes);
      return false;
    }
  }

  const size_t last_offset = (block_count - 1) * 16;
  const size_t last_length = input_length > last_offset ? input_length - last_offset : 0;
  std::memset(block, 0, sizeof(block));
  if (last_length != 0) std::memcpy(block, input + last_offset, last_length);
  if (!complete_last_block) block[last_length] = 0x80;
  const uint8_t *subkey = complete_last_block ? k1 : k2;
  for (size_t byte = 0; byte < 16; ++byte) {
    block[byte] ^= static_cast<uint8_t>(state[byte] ^ subkey[byte]);
  }
  const bool success =
      mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, block, output) == 0;
  mbedtls_aes_free(&aes);
  return success;
}

bool aes_block(const uint8_t key[16], int mode, const uint8_t input[16],
               uint8_t output[16]) noexcept {
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  const bool success =
      (mode == MBEDTLS_AES_ENCRYPT ? mbedtls_aes_setkey_enc(&aes, key, 128)
                                   : mbedtls_aes_setkey_dec(&aes, key, 128)) ==
          0 &&
      mbedtls_aes_crypt_ecb(&aes, mode, input, output) == 0;
  mbedtls_aes_free(&aes);
  return success;
}

bool scp03_kdf(const uint8_t key[16], uint8_t constant,
               const uint8_t context[16], size_t output_bits,
               uint8_t output[16]) noexcept {
  // SCP03 counter KDF input: label || derivation constant || separator ||
  // requested output bit length || counter || context. Session keys use 128
  // bits; card and host cryptograms use 64 bits.
  uint8_t input[32]{};
  input[11] = constant;
  input[13] = static_cast<uint8_t>((output_bits >> 8) & 0xff);
  input[14] = static_cast<uint8_t>(output_bits & 0xff);
  input[15] = 0x01; // one-byte counter
  std::memcpy(input + 16, context, 16);
  return aes_cmac(key, input, sizeof(input), output);
}

bool make_cmac_input(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2,
                     const uint8_t *data, size_t data_length,
                     const uint8_t chaining_value[16], uint8_t *input,
                     size_t input_capacity, size_t &input_length) noexcept {
  if (data_length > 255 || input_capacity < 21 + data_length) {
    return false;
  }
  std::memcpy(input, chaining_value, 16);
  input[16] = cla;
  input[17] = ins;
  input[18] = p1;
  input[19] = p2;
  input[20] = static_cast<uint8_t>(data_length + kMacLength);
  if (data_length != 0) {
    std::memcpy(input + 21, data, data_length);
  }
  input_length = 21 + data_length;
  return true;
}
} // namespace

void Scp03Session::reset() noexcept {
  initialized_ = false;
  active_ = false;
  std::memset(host_challenge_, 0, sizeof(host_challenge_));
  std::memset(card_challenge_, 0, sizeof(card_challenge_));
  std::memset(session_mac_key_, 0, sizeof(session_mac_key_));
  std::memset(chaining_value_, 0, sizeof(chaining_value_));
}

bool Scp03Session::initializeUpdate(const uint8_t *apdu, size_t apdu_length,
                                   uint8_t *response,
                                   size_t response_capacity,
                                   size_t &response_length) noexcept {
  reset();
  response_length = 0;
  // Short case-4 INITIALIZE UPDATE, key version 0 (any) or the stored
  // version, key ID 0, S8 challenge.
  uint8_t keyset[kKeysetLength];
  if (apdu == nullptr || apdu_length != 14 || response == nullptr ||
      response_capacity < 29 || apdu[0] != 0x80 || apdu[1] != 0x50 ||
      apdu[3] != 0x00 || apdu[4] != 0x08 || apdu[13] != 0x00 ||
      !load_keyset(keyset) || (apdu[2] != 0x00 && apdu[2] != keyset[0])) {
    mbedtls_platform_zeroize(keyset, sizeof(keyset));
    return false;
  }
  std::memcpy(host_challenge_, apdu + 5, sizeof(host_challenge_));

#ifdef PC_VERSION
  try {
    std::random_device random;
    for (uint8_t &value : card_challenge_) {
      value = static_cast<uint8_t>(random());
    }
  } catch (...) {
    return false;
  }
#else
  const rng_128_t random{};
  auto card_random = random;
  get_rand_128(&card_random);
  std::memcpy(card_challenge_, &card_random, sizeof(card_challenge_));
#endif

  uint8_t context[16];
  std::memcpy(context, host_challenge_, 8);
  std::memcpy(context + 8, card_challenge_, 8);
  uint8_t session_enc_key[16];
  uint8_t session_rmac_key[16];
  uint8_t card_cryptogram[16];
  const bool derived = scp03_kdf(keyset + kEncOffset, 0x04, context, 128,
                                 session_enc_key) &&
                       scp03_kdf(keyset + kMacOffset, 0x06, context, 128,
                                 session_mac_key_) &&
                       scp03_kdf(keyset + kMacOffset, 0x07, context, 128,
                                 session_rmac_key) &&
                       scp03_kdf(session_mac_key_, 0x00, context, 64,
                                 card_cryptogram);
  std::memset(session_enc_key, 0, sizeof(session_enc_key));
  std::memset(session_rmac_key, 0, sizeof(session_rmac_key));
  const uint8_t key_version = keyset[0];
  mbedtls_platform_zeroize(keyset, sizeof(keyset));
  if (!derived) {
    reset();
    return false;
  }

  std::memset(response, 0, 10); // no key diversification data
  response[10] = key_version;   // key version number
  response[11] = 0x03;          // SCP03
  response[12] = 0x00;          // S8, no pseudorandom challenge / sequence
  std::memcpy(response + 13, card_challenge_, 8);
  std::memcpy(response + 21, card_cryptogram, kMacLength);
  response_length = 29;
  initialized_ = true;
  return true;
}

bool Scp03Session::verifyAndAdvance(uint8_t cla, uint8_t ins, uint8_t p1,
                                    uint8_t p2, const uint8_t *data,
                                    size_t data_length,
                                    const uint8_t *received_mac) noexcept {
  if (data_length > 247 || received_mac == nullptr) {
    return false;
  }
  std::array<uint8_t, 272> input{};
  size_t input_length = 0;
  if (!make_cmac_input(cla, ins, p1, p2, data, data_length, chaining_value_,
                       input.data(), input.size(), input_length)) {
    return false;
  }
  uint8_t mac[16];
  if (!aes_cmac(session_mac_key_, input.data(), input_length, mac) ||
      std::memcmp(mac, received_mac, kMacLength) != 0) {
    return false;
  }
  std::memcpy(chaining_value_, mac, sizeof(chaining_value_));
  return true;
}

bool Scp03Session::externalAuthenticate(const uint8_t *apdu,
                                       size_t apdu_length) noexcept {
  if (!initialized_ || active_ || apdu == nullptr ||
      (apdu_length != 21 && apdu_length != 22) ||
      apdu[0] != 0x84 || apdu[1] != 0x82 || apdu[2] != 0x01 ||
      apdu[3] != 0x00 || apdu[4] != 0x10 ||
      (apdu_length == 22 && apdu[21] != 0x00)) {
    return false;
  }
  uint8_t context[16];
  std::memcpy(context, host_challenge_, 8);
  std::memcpy(context + 8, card_challenge_, 8);
  uint8_t expected_host_cryptogram[16];
  if (!scp03_kdf(session_mac_key_, 0x01, context, 64,
                 expected_host_cryptogram) ||
      std::memcmp(expected_host_cryptogram, apdu + 5, kMacLength) != 0 ||
      !verifyAndAdvance(apdu[0], apdu[1], apdu[2], apdu[3], apdu + 5,
                        kMacLength, apdu + 5 + kMacLength)) {
    return false;
  }
  active_ = true;
  initialized_ = false;
  return true;
}

bool Scp03Session::verifyCommand(const uint8_t *apdu, size_t apdu_length,
                                size_t &data_length) noexcept {
  data_length = 0;
  if (!active_ || apdu == nullptr || apdu_length < 14 || apdu[0] != 0x84 ||
      apdu[4] < kMacLength) {
    return false;
  }
  const size_t lc = apdu[4];
  if (apdu_length != 5 + lc && apdu_length != 6 + lc) {
    return false;
  }
  data_length = lc - kMacLength;
  if (!verifyAndAdvance(apdu[0], apdu[1], apdu[2], apdu[3], apdu + 5,
                        data_length, apdu + 5 + data_length)) {
    data_length = 0;
    return false;
  }
  return true;
}

uint8_t Scp03Session::keyVersion() noexcept {
  uint8_t version = 0;
  // A malformed record reads as the default version; INITIALIZE UPDATE still
  // fails closed on it.
  return fs_read_1b_at(kKeysetTag, sizeof(kKeysetTag), 0, &version) ==
                 FS_STATUS_OK
             ? version
             : 0x00;
}

uint16_t Scp03Session::putKey(uint8_t p1, uint8_t p2, const uint8_t *data,
                              size_t data_length, uint8_t *response,
                              size_t response_capacity,
                              size_t &response_length) noexcept {
  // GPC 2.3 11.8 / Amendment D: new KVN, then ENC, MAC, DEK as
  // 88 11 10 <AES-CBC(DEK, zero IV) key> 03 <KCV>.
  constexpr size_t kKeyBlockLength = 3 + kKeyLength + 1 + 3;
  constexpr size_t kDataLength = 1 + 3 * kKeyBlockLength;
  response_length = 0;
  if (!active_) return 0x6982;
  if (p2 != 0x81) return 0x6a86;
  if (data == nullptr || data_length != kDataLength || response == nullptr ||
      response_capacity < 10) {
    return 0x6a80;
  }
  uint8_t current[kKeysetLength];
  if (!load_keyset(current)) return 0x6581;
  if (p1 != 0x00 && p1 != current[0]) {
    mbedtls_platform_zeroize(current, sizeof(current));
    return 0x6a88;
  }
  const uint8_t new_version = data[0];
  // 0 marks the default keyset and 0x80-0xFF are reserved by GlobalPlatform.
  if (new_version == 0x00 || new_version > 0x7f) {
    mbedtls_platform_zeroize(current, sizeof(current));
    return 0x6a80;
  }

  uint8_t keyset[kKeysetLength];
  keyset[0] = new_version;
  response[0] = new_version;
  static constexpr uint8_t kKcvInput[16] = {
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
  uint16_t status = 0x9000;
  for (size_t index = 0; index < 3 && status == 0x9000; ++index) {
    const uint8_t *block = data + 1 + index * kKeyBlockLength;
    uint8_t *key = keyset + kEncOffset + index * kKeyLength;
    uint8_t kcv[16];
    if (block[0] != 0x88 || block[1] != kKeyLength + 1 ||
        block[2] != kKeyLength || block[3 + kKeyLength] != 0x03) {
      status = 0x6a80;
    } else if (!aes_block(current + kDekOffset, MBEDTLS_AES_DECRYPT,
                          block + 3, key) ||
               !aes_block(key, MBEDTLS_AES_ENCRYPT, kKcvInput, kcv)) {
      status = 0x6f00;
    } else if (std::memcmp(kcv, block + 4 + kKeyLength, 3) != 0) {
      status = 0x6a80; // wrong DEK or corrupted key
    } else {
      std::memcpy(response + 1 + index * 3, kcv, 3);
    }
  }
  if (status == 0x9000 &&
      (fs_write(kKeysetTag, sizeof(kKeysetTag), keyset, kKeysetLength) !=
           FS_STATUS_OK ||
       fs_commit_tag(kKeysetTag, sizeof(kKeysetTag)) != FS_STATUS_OK)) {
    // Put the previous keyset back so a later sync cannot apply a key the
    // host was told failed. Stored versions are never 0, so 0 means default.
    if (current[0] == 0x00) {
      (void)fs_erase(kKeysetTag, sizeof(kKeysetTag));
    } else {
      (void)fs_write(kKeysetTag, sizeof(kKeysetTag), current, kKeysetLength);
    }
    status = 0x6581;
  }
  mbedtls_platform_zeroize(current, sizeof(current));
  mbedtls_platform_zeroize(keyset, sizeof(keyset));
  if (status == 0x9000) response_length = 10;
  return status;
}
