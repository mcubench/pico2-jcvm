#include "short_apdu.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

namespace {

void expect(bool condition, const char *label) {
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", label);
    std::exit(1);
  }
}

bool reference_parse(const std::vector<uint8_t> &bytes,
                     pico2_jcvm::ShortApduCommand &parsed) {
  parsed = {};
  const size_t length = bytes.size();
  if (length < 4) return false;
  if (length == 4) return true;
  if (length == 5) {
    parsed.has_expected_response_length = true;
    parsed.expected_response_length = bytes[4] == 0 ? 256 : bytes[4];
    return true;
  }
  const uint8_t incoming_length = bytes[4];
  if (incoming_length == 0) return false;
  if (length == 5u + incoming_length) {
    parsed.incoming_length = incoming_length;
    return true;
  }
  if (length == 6u + incoming_length) {
    parsed.incoming_length = incoming_length;
    parsed.has_expected_response_length = true;
    parsed.expected_response_length = bytes.back() == 0 ? 256 : bytes.back();
    return true;
  }
  return false;
}

} // namespace

int main() {
  pico2_jcvm::ShortApduCommand parsed;
  expect(!pico2_jcvm::parse_short_apdu(nullptr, 0, parsed),
         "null_empty_rejected");
  expect(!pico2_jcvm::parse_short_apdu(nullptr, 4, parsed),
         "null_nonempty_rejected");

  constexpr unsigned iterations = 250000;
  std::mt19937 generator(0x41504455);
  std::uniform_int_distribution<unsigned> byte_distribution(0, 255);
  std::uniform_int_distribution<unsigned> length_distribution(0, 300);
  for (unsigned iteration = 0; iteration < iterations; ++iteration) {
    std::vector<uint8_t> input(length_distribution(generator));
    for (uint8_t &value : input) {
      value = static_cast<uint8_t>(byte_distribution(generator));
    }

    const bool actual = pico2_jcvm::parse_short_apdu(
        input.empty() ? nullptr : input.data(),
        static_cast<uint16_t>(input.size()), parsed);
    pico2_jcvm::ShortApduCommand expected;
    const bool reference = reference_parse(input, expected);
    expect(actual == reference, "acceptance_matches_reference");
    expect(parsed.incoming_length == expected.incoming_length &&
               parsed.expected_response_length ==
                   expected.expected_response_length &&
               parsed.has_expected_response_length ==
                   expected.has_expected_response_length,
           "decoded_lengths_match_reference");
  }

  std::printf("TEST:PASS apdu_parser_fuzz inputs=%u\n", iterations);
  return 0;
}
