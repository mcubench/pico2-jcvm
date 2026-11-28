// Review findings #4 and #25 (docs/review_claude_20261127.md).
#include "exceptions.hpp"
#include "stack.hpp"

#include <cstdint>
#include <cstdio>

namespace {

unsigned failures = 0;
unsigned passes = 0;

void check(const bool condition, const char *name) {
  std::printf("TEST:%s %s\n", condition ? "PASS" : "FAIL", name);
  ++(condition ? passes : failures);
}

// Long enough that a saved return address (opcode + 3) stays inside the method.
const uint8_t method_code[8] = {};

void push_frame(jcvm::Stack &stack) {
  stack.push_Frame(0, 1, 2, method_code, method_code,
                   method_code + sizeof(method_code));
}

} // namespace

int main() {
  // #4: return addresses saved by jsr share one budget across live frames,
  // so nested frames cannot multiply the per-frame limit of 256.
  {
    jcvm::Stack stack;
    unsigned saved = 0;
    bool limited = false;
    try {
      for (unsigned frame = 0; frame < 4; ++frame) {
        push_frame(stack);
        for (unsigned index = 0; index < 200; ++index) {
          (void)stack.savePC();
          ++saved;
        }
      }
    } catch (jcvm::Exceptions) {
      limited = true;
    }
    std::printf("RETURN_ADDRESSES:SAVED count=%u\n", saved);
    check(limited && saved <= 256,
          "saved return addresses are limited across all live frames");
  }

  // #25: after an uncaught exception unwinds the last frame, frame accessors
  // must fail with a VM exception instead of reading freed or missing state.
  // Kept last: without the guard AddressSanitizer stops the process here.
  {
    jcvm::Stack stack;
    push_frame(stack);
    stack.pop_Frame();
    bool guarded = false;
    try {
      (void)stack.getCurrentFrame();
    } catch (jcvm::Exceptions) {
      guarded = true;
    }
    check(guarded, "getCurrentFrame on an empty stack raises a VM exception");
  }

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passes, failures);
  return failures == 0 ? 0 : 1;
}
