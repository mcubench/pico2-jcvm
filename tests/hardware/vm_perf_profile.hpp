#pragma once

// Included only in the isolated ARM benchmark build.
#include <array>
#include <cstdint>
#include <cstdio>

extern "C" uint32_t choupi_time_us_32(void);

namespace vm_perf {
struct Opcode {
  uint32_t samples = 0;
  uint32_t handler_us = 0;
  uint32_t branch_us = 0;
  uint32_t dynamic_cap_us = 0;
  uint32_t rom_cap_us = 0;
  uint32_t array_metadata_us = 0;
  uint32_t array_payload_us = 0;
  uint32_t array_metadata_calls = 0;
};

struct Profile {
  std::array<Opcode, 256> opcodes{};
  uint32_t instructions = 0;
  uint32_t random = 1;
  uint32_t watchdog_us = 0;
  uint32_t fetch_us = 0;
  uint32_t bookkeeping_us = 0;
  uint32_t decode_us = 0;
};

inline Profile profile;
inline bool enabled = false;
inline Opcode *active_handler = nullptr;

class Validation {
public:
  Validation(const char *kind, uint32_t bytes)
      : kind_(kind), bytes_(bytes), start_(choupi_time_us_32()) {}
  ~Validation() {
    const uint32_t elapsed = choupi_time_us_32() - start_;
    std::printf("VM2:VALIDATION kind=%s bytes=%lu us=%lu\r\n", kind_,
                static_cast<unsigned long>(bytes_),
                static_cast<unsigned long>(elapsed));
  }

private:
  const char *kind_;
  uint32_t bytes_;
  uint32_t start_;
};

class Apdu {
public:
  explicit Apdu(uint8_t instruction) : instruction_(instruction) {
    // Measure the existing JCAlgTest prepare and timed APDUs.
    enabled = instruction == 0xa5 || instruction == 0x43 ||
              instruction == 0xc0 || instruction == 0x31;
    if (enabled) profile = {};
  }
  ~Apdu() {
    if (!enabled) return;
    enabled = false;
    std::printf("VM2:APDU ins=%02x ops=%lu watchdog=%lu fetch=%lu bookkeeping=%lu decode=%lu\r\n",
                instruction_, static_cast<unsigned long>(profile.instructions),
                static_cast<unsigned long>(profile.watchdog_us),
                static_cast<unsigned long>(profile.fetch_us),
                static_cast<unsigned long>(profile.bookkeeping_us),
                static_cast<unsigned long>(profile.decode_us));
    for (unsigned i = 0; i < profile.opcodes.size(); ++i) {
      const auto &op = profile.opcodes[i];
      if (op.samples == 0) continue;
      std::printf("VM2:OP %02x %lu %lu %lu %lu %lu %lu %lu %lu\r\n", i,
                  static_cast<unsigned long>(op.samples),
                  static_cast<unsigned long>(op.handler_us),
                  static_cast<unsigned long>(op.branch_us),
                  static_cast<unsigned long>(op.dynamic_cap_us),
                  static_cast<unsigned long>(op.rom_cap_us),
                  static_cast<unsigned long>(op.array_metadata_us),
                  static_cast<unsigned long>(op.array_payload_us),
                  static_cast<unsigned long>(op.array_metadata_calls));
    }
    std::printf("VM2:END\r\n");
  }

private:
  uint8_t instruction_;
};

class Instruction {
public:
  Instruction() {
    if (!enabled) return;
    ++profile.instructions;
    profile.random = profile.random * 1664525u + 1013904223u;
    sampled_ = (profile.random >> 26) == 0;
    if (sampled_) last_ = choupi_time_us_32();
  }
  ~Instruction() {
    if (opcode_ != nullptr) active_handler = previous_;
  }

  void phase(uint32_t &total) {
    if (!sampled_) return;
    const auto now = choupi_time_us_32();
    total += now - last_;
    last_ = now;
  }

  void begin_handler(uint8_t bytecode) {
    if (!sampled_) return;
    previous_ = active_handler;
    opcode_ = &profile.opcodes[bytecode];
    active_handler = opcode_;
    ++opcode_->samples;
    last_ = choupi_time_us_32();
  }

  void end_handler() {
    if (opcode_ == nullptr) return;
    opcode_->handler_us += choupi_time_us_32() - last_;
  }

private:
  uint32_t last_ = 0;
  Opcode *opcode_ = nullptr;
  Opcode *previous_ = nullptr;
  bool sampled_ = false;
};

class HandlerPhase {
public:
  explicit HandlerPhase(uint32_t Opcode::*counter,
                        uint32_t Opcode::*calls = nullptr)
      : total_(active_handler == nullptr ? nullptr : &(active_handler->*counter)),
        start_(total_ == nullptr ? 0 : choupi_time_us_32()) {
    if (total_ != nullptr && calls != nullptr) ++(active_handler->*calls);
  }
  ~HandlerPhase() {
    if (total_ != nullptr) *total_ += choupi_time_us_32() - start_;
  }

private:
  uint32_t *total_;
  uint32_t start_;
};
} // namespace vm_perf
