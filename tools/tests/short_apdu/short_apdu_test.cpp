#include "short_apdu.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
void expect(const bool condition, const char *name) {
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL short_apdu %s\n", name);
    std::exit(EXIT_FAILURE);
  }
  std::printf("TEST:PASS short_apdu %s\n", name);
}
} // namespace

int main() {
  using pico2_jcvm::ShortApduCommand;
  using pico2_jcvm::parse_short_apdu;

  ShortApduCommand parsed;
  constexpr std::array<uint8_t, 4> case1{0x00, 0x84, 0x00, 0x00};
  expect(parse_short_apdu(case1.data(), case1.size(), parsed) &&
             parsed.incoming_length == 0 &&
             !parsed.has_expected_response_length,
         "case1");

  constexpr std::array<uint8_t, 5> case2{0x00, 0xca, 0x00, 0x00, 0x10};
  expect(parse_short_apdu(case2.data(), case2.size(), parsed) &&
             parsed.incoming_length == 0 &&
             parsed.has_expected_response_length &&
             parsed.expected_response_length == 16,
         "case2_le");

  constexpr std::array<uint8_t, 5> case2_max{0x00, 0xca, 0x00, 0x00, 0x00};
  expect(parse_short_apdu(case2_max.data(), case2_max.size(), parsed) &&
             parsed.expected_response_length == 256,
         "case2_zero_le_means_256");

  constexpr std::array<uint8_t, 8> case3{0x00, 0xda, 0x00, 0x00,
                                         0x03, 0xaa, 0xbb, 0xcc};
  expect(parse_short_apdu(case3.data(), case3.size(), parsed) &&
             parsed.incoming_length == 3 &&
             !parsed.has_expected_response_length,
         "case3");

  constexpr std::array<uint8_t, 9> case4{0x00, 0xda, 0x00, 0x00,
                                         0x03, 0xaa, 0xbb, 0xcc, 0x20};
  expect(parse_short_apdu(case4.data(), case4.size(), parsed) &&
             parsed.incoming_length == 3 &&
             parsed.expected_response_length == 32,
         "case4");

  constexpr std::array<uint8_t, 9> case4_max{0x00, 0xda, 0x00, 0x00,
                                             0x03, 0xaa, 0xbb, 0xcc, 0x00};
  expect(parse_short_apdu(case4_max.data(), case4_max.size(), parsed) &&
             parsed.expected_response_length == 256,
         "case4_zero_le_means_256");

  constexpr std::array<uint8_t, 7> extended{0x00, 0xda, 0x00, 0x00,
                                            0x00, 0x00, 0x01};
  expect(!parse_short_apdu(extended.data(), extended.size(), parsed),
         "extended_rejected");
  expect(!parse_short_apdu(case2.data(), 3, parsed), "truncated_rejected");
  expect(!parse_short_apdu(case3.data(), 7, parsed), "bad_lc_rejected");

  std::array<uint8_t, 600> response{};
  for (uint16_t index = 0; index < response.size(); ++index) {
    response[index] = static_cast<uint8_t>(index);
  }
  pico2_jcvm::ShortApduResponseChain chain;
  expect(chain.queue(response.data(), response.size()), "chain_queue");
  std::array<uint8_t, 256> chunk{};
  uint16_t status_word = 0;
  expect(chain.take(256, chunk.data(), chunk.size(), status_word) == 256 &&
             std::memcmp(chunk.data(), response.data(), 256) == 0 &&
             status_word == 0x6100,
         "chain_first_chunk_6100");
  expect(chain.take(256, chunk.data(), chunk.size(), status_word) == 256 &&
             std::memcmp(chunk.data(), response.data() + 256, 256) == 0 &&
             status_word == 0x6158,
         "chain_second_chunk_remaining");
  expect(chain.take(256, chunk.data(), chunk.size(), status_word) == 88 &&
             std::memcmp(chunk.data(), response.data() + 512, 88) == 0 &&
             status_word == 0x9000 && !chain.has_pending(),
         "chain_final_chunk");
  return EXIT_SUCCESS;
}
