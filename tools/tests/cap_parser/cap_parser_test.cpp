#include "jc_handlers/jc_cap.hpp"
#include "exceptions.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

namespace {

unsigned checks = 0;

void expect(bool condition, const char *label) {
  ++checks;
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", label);
    std::exit(1);
  }
}

bool parses(const uint8_t *bytes, uint16_t length) {
  try {
    const jcvm::JC_Cap parsed(length, bytes);
    (void)parsed;
    return true;
  } catch (jcvm::Exceptions) {
    return false;
  }
}

} // namespace

int main(int argc, char **argv) {
  expect(parses(nullptr, 0), "empty_null_input");
  expect(!parses(nullptr, 1), "null_nonempty_input_rejected");

  const std::array<uint8_t, 6> framed_method = {7, 0, 3, 0x11, 0x22, 0x33};
  expect(parses(framed_method.data(), framed_method.size()),
         "complete_component_frame");
  for (uint16_t prefix = 1; prefix < framed_method.size(); ++prefix) {
    expect(!parses(framed_method.data(), prefix),
           "every_truncated_component_prefix_rejected");
  }

  const std::array<uint8_t, 4> oversized_frame = {7, 0xff, 0xff, 0};
  expect(!parses(oversized_frame.data(), oversized_frame.size()),
         "oversized_component_rejected");

  const std::array<uint8_t, 6> duplicate_method = {7, 0, 0, 7, 0, 0};
  expect(!parses(duplicate_method.data(), duplicate_method.size()),
         "duplicate_component_rejected");

  std::mt19937 generator(0x4a43564d);
  std::uniform_int_distribution<unsigned> byte_distribution(0, 255);
  std::uniform_int_distribution<unsigned> length_distribution(0, 128);
  constexpr unsigned iterations = 100000;
  for (unsigned iteration = 0; iteration < iterations; ++iteration) {
    const uint16_t length =
        static_cast<uint16_t>(length_distribution(generator));
    std::vector<uint8_t> input(length);
    for (uint8_t &value : input) {
      value = static_cast<uint8_t>(byte_distribution(generator));
    }
    (void)parses(input.empty() ? nullptr : input.data(), length);
  }

  unsigned corpus_files = 0;
  for (int argument = 1; argument < argc; ++argument) {
    std::ifstream file(argv[argument], std::ios::binary);
    expect(file.good(), "corpus_file_open");
    std::vector<uint8_t> cap((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
    expect(!cap.empty() && cap.size() <= UINT16_MAX,
           "corpus_file_length_valid");
    expect(parses(cap.data(), static_cast<uint16_t>(cap.size())),
           "upstream_cap_parses");
    ++corpus_files;

    size_t offset = 0;
    while (offset < cap.size()) {
      expect(cap.size() - offset >= 3, "corpus_component_header_present");
      const size_t payload_size =
          (static_cast<size_t>(cap[offset + 1]) << 8) | cap[offset + 2];
      const size_t frame_size = 3 + payload_size;
      expect(frame_size <= cap.size() - offset,
             "corpus_component_frame_fits");

      std::vector<uint8_t> oversized = cap;
      oversized[offset + 1] = 0xff;
      oversized[offset + 2] = 0xff;
      expect(!parses(oversized.data(), static_cast<uint16_t>(oversized.size())),
             "mutated_component_length_rejected");
      expect(!parses(cap.data(), static_cast<uint16_t>(offset + 1)),
             "truncated_component_tag_rejected");
      expect(!parses(cap.data(), static_cast<uint16_t>(offset + 2)),
             "truncated_component_length_rejected");
      if (payload_size != 0) {
        expect(!parses(cap.data(), static_cast<uint16_t>(offset + frame_size - 1)),
               "truncated_component_payload_rejected");
      }
      offset += frame_size;
    }
  }

  std::printf(
      "TEST:PASS cap_parser_framing checks=%u fuzz_inputs=%u corpus_files=%u\n",
      checks, iterations, corpus_files);
  return 0;
}
