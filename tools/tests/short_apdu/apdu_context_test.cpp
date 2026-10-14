#include "context.hpp"
#include "exceptions.hpp"
#include "jc_types/jc_array.hpp"
#include "jc_types/jc_array_type.hpp"

#include <cstdio>
#include <cstdlib>
#include <memory>

namespace {
void expect(const bool condition, const char *name) {
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL apdu_context %s\n", name);
    std::exit(EXIT_FAILURE);
  }
  std::printf("TEST:PASS apdu_context %s\n", name);
}

template <typename Action>
bool throws_apdu_exception(const Action &action) {
  try {
    action();
  } catch (const jcvm::Exceptions exception) {
    return exception == jcvm::Exceptions::APDUException;
  }
  return false;
}
} // namespace

int main() {
  jcvm::Context context(0, 1);
  jcvm::Context::setActiveContext(&context);

  context.setAPDUBufferRef(jcvm::jref_t{}, 3, 8, true);
  expect(context.getAPDUState() == 0 &&
             context.getAPDUExpectedResponseLength() == 8,
         "reset_for_new_command");
  expect(context.beginAPDUOutgoing(false) == 8 &&
             context.getAPDUState() == 3,
         "begin_outgoing");
  context.setAPDUOutgoingLength(4);
  expect(context.getAPDUState() == 4, "outgoing_length_known");

  auto buffer = std::make_shared<jcvm::JC_Array>(
      context.getHeap(), 4, jcvm::JAVA_ARRAY_T_BYTE);
  buffer->setByteEntry(0, 0x11);
  buffer->setByteEntry(1, 0x22);
  buffer->setByteEntry(2, 0x33);
  buffer->setByteEntry(3, 0x44);
  context.setAPDUOutgoing(buffer, 0, 2);
  expect(context.getAPDUState() == 5 &&
             context.getAPDUOutgoingLength() == 2 &&
             context.getAPDUOutgoingByte(0) == 0x11 &&
             context.getAPDUOutgoingByte(1) == 0x22,
         "partial_outgoing");
  context.setAPDUOutgoing(buffer, 2, 2);
  expect(context.getAPDUState() == 6 &&
             context.getAPDUOutgoingLength() == 4 &&
             context.getAPDUOutgoingByte(2) == 0x33 &&
             context.getAPDUOutgoingByte(3) == 0x44,
         "full_outgoing_appends_chunks");
  expect(throws_apdu_exception(
             [&context, &buffer] { context.setAPDUOutgoing(buffer, 0, 1); }),
         "reject_send_after_complete");

  context.setAPDUBufferRef(jcvm::jref_t{}, 0, 16, true);
  context.markAPDUIncomingReceived();
  expect(context.getAPDUState() == 2, "full_incoming");
  expect(context.beginAPDUOutgoing(true) == 16, "begin_no_chaining");
  expect(throws_apdu_exception(
             [&context] { context.setAPDUOutgoingLength(253); }),
         "no_chaining_bound");
  context.setAPDUOutgoingLength(252);
  expect(context.getAPDUState() == 4, "no_chaining_valid_length");

  context.setAPDUBufferRef(jcvm::jref_t{}, 0, 0, false);
  expect(throws_apdu_exception(
             [&context] { (void)context.beginAPDUOutgoing(false); }),
         "reject_outgoing_without_le");
  jcvm::Context::setActiveContext(nullptr);
  return EXIT_SUCCESS;
}
