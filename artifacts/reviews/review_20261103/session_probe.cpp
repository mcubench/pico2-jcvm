// Review copy: only the obsolete constructor invocation was adapted.
#include "vpcd_session.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr std::array<uint8_t, 7> kAppletAid = {
    0x4a, 0x43, 0x41, 0x6c, 0x67, 0x54, 0x31};

struct FakeCard {
  unsigned int clear_count = 0;
  unsigned int restore_count = 0;
  unsigned int select_count = 0;
  unsigned int deselect_count = 0;
  bool last_deselect_same_applet = false;
  unsigned int process_count = 0;
  unsigned int card_manager_process_count = 0;
  unsigned int card_manager_reset_count = 0;
  uint8_t restore_result = 0;
  uint16_t select_status = 0x9000;
  uint16_t process_status = 0x9000;
  std::vector<uint8_t> last_command;
};

void clear(void *opaque) {
  ++static_cast<FakeCard *>(opaque)->clear_count;
}

uint8_t restore(void *opaque, const uint8_t *aid, uint8_t aid_length) {
  auto &card = *static_cast<FakeCard *>(opaque);
  ++card.restore_count;
  if (aid_length != kAppletAid.size()) return 1;
  for (size_t index = 0; index < aid_length; ++index) {
    if (aid[index] != kAppletAid[index]) return 1;
  }
  return card.restore_result;
}

uint16_t select(void *opaque) {
  auto &card = *static_cast<FakeCard *>(opaque);
  ++card.select_count;
  return card.select_status;
}

uint16_t deselect(void *opaque, bool selecting_same_applet) {
  auto &card = *static_cast<FakeCard *>(opaque);
  ++card.deselect_count;
  card.last_deselect_same_applet = selecting_same_applet;
  return 0x9000;
}

uint16_t process(void *opaque, const uint8_t *command,
                 uint16_t command_length, uint8_t *response,
                 uint16_t response_capacity, uint16_t *response_length) {
  auto &card = *static_cast<FakeCard *>(opaque);
  ++card.process_count;
  card.last_command.assign(command, command + command_length);
  if (response_capacity < 4) return 0x6f00;
  response[0] = 0xde;
  response[1] = 0xad;
  response[2] = static_cast<uint8_t>(card.process_status >> 8);
  response[3] = static_cast<uint8_t>(card.process_status);
  *response_length = 4;
  return card.process_status;
}

uint16_t process_card_manager(void *opaque, const uint8_t *command,
                             uint16_t command_length, uint8_t *response,
                             uint16_t response_capacity,
                             uint16_t *response_length) {
  auto &card = *static_cast<FakeCard *>(opaque);
  ++card.card_manager_process_count;
  if (command_length != 5 || command[0] != 0x80 || command[1] != 0xca ||
      response_capacity < 3) {
    return 0x6f00;
  }
  response[0] = 0x66;
  response[1] = 0x90;
  response[2] = 0x00;
  *response_length = 3;
  return 0x9000;
}

void reset_card_manager(void *opaque) {
  ++static_cast<FakeCard *>(opaque)->card_manager_reset_count;
}

bool expect(bool condition, const char *name) {
  if (!condition) std::fprintf(stderr, "FAIL: %s\n", name);
  return condition;
}

std::vector<uint8_t> applet_select() {
  std::vector<uint8_t> command{0x00, 0xa4, 0x04, 0x00,
                               static_cast<uint8_t>(kAppletAid.size())};
  command.insert(command.end(), kAppletAid.begin(), kAppletAid.end());
  command.push_back(0x40);
  return command;
}

std::vector<uint8_t> unknown_applet_select() {
  constexpr std::array<uint8_t, 7> unknown_aid = {
      0xf0, 0x00, 0x00, 0x00, 0x01, 0x23, 0x45};
  std::vector<uint8_t> command{0x00, 0xa4, 0x04, 0x00,
                               static_cast<uint8_t>(unknown_aid.size())};
  command.insert(command.end(), unknown_aid.begin(), unknown_aid.end());
  command.push_back(0x40);
  return command;
}

std::vector<uint8_t> control(uint8_t value) { return {value}; }

} // namespace

int main() {
  unsigned int passed = 0;
  unsigned int failed = 0;
  auto check = [&](bool value, const char *name) {
    if (expect(value, name)) {
      ++passed;
    } else {
      ++failed;
    }
  };

  FakeCard card;
  const choupi::vpcd::CardCallbacks callbacks{
      &card, clear, restore, select, deselect, process,
      process_card_manager, reset_card_manager};
  choupi::vpcd::Session session(callbacks);
  std::vector<uint8_t> response;

  auto message = control(0x04);
  auto result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({0x3b, 0x80, 0x01, 0x81}) &&
            !session.powered(),
        "ATR probe reports card present before power on");
  message = control(0x01);
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::NoResponse && session.powered(),
        "power on enables the card");
  message = control(0x04);
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({0x3b, 0x80, 0x01, 0x81}),
        "ATR request returns configured T=1 ATR");

  message = applet_select();
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            session.applet_selected() && card.restore_count == 1 &&
            card.select_count == 1 && card.process_count == 1 &&
            card.last_command == message &&
            response == std::vector<uint8_t>({0xde, 0xad, 0x90, 0x00}),
        "applet SELECT restores, selects, and dispatches its APDU");

  const std::vector<uint8_t> command{0x00, 0x10, 0x00, 0x00};
  result = session.handle_message(command.data(), command.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            card.process_count == 2 && card.last_command == command &&
            response == std::vector<uint8_t>({0xde, 0xad, 0x90, 0x00}),
        "selected applet receives APDU and returns response");

  message = unknown_applet_select();
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({0x6a, 0x82}) &&
            !session.applet_selected() && card.deselect_count == 1 &&
            !card.last_deselect_same_applet && card.process_count == 2,
        "unknown AID SELECT deselects applet without dispatching to process");

  message = control(0x00);
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::NoResponse &&
            !session.powered() && !session.applet_selected() &&
            card.deselect_count == 1,
        "power off deselects and clears selected state");
  result = session.handle_message(command.data(), command.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({0x69, 0x85}),
        "APDU while off receives conditions-not-satisfied");

  message = control(0x02);
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::NoResponse &&
            session.powered() && card.clear_count >= 1,
        "reset powers on and clears prior VM state");
  message = applet_select();
  (void)session.handle_message(message.data(), message.size(), response);
  const std::vector<uint8_t> isd_select{0x00, 0xa4, 0x04, 0x00, 0x08,
                                        0xa0, 0x00, 0x00, 0x01, 0x51,
                                        0x00, 0x00, 0x00};
  result = session.handle_message(isd_select.data(), isd_select.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            !session.applet_selected() && card.deselect_count == 2 &&
            response.size() == 14 && response[12] == 0x90 &&
            response[13] == 0x00,
        "ISD SELECT deselects applet and returns FCI");

  const std::vector<uint8_t> default_isd_select{
      0x00, 0xa4, 0x04, 0x00, 0x00};
  result = session.handle_message(default_isd_select.data(),
                                  default_isd_select.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({
                            0x6f, 0x0a, 0x84, 0x08, 0xa0, 0x00, 0x00,
                            0x01, 0x51, 0x00, 0x00, 0x00, 0x90, 0x00}),
        "case-1 empty-AID SELECT selects the default ISD");
  const std::vector<uint8_t> card_data{
      0x80, 0xca, 0x00, 0x66, 0x00};
  result = session.handle_message(card_data.data(), card_data.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            response == std::vector<uint8_t>({0x66, 0x90, 0x00}) &&
            card.card_manager_process_count == 1,
        "card-manager APDU routes after ISD SELECT");

  card.process_status = 0x6a82;
  message = applet_select();
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            !session.applet_selected() &&
            response == std::vector<uint8_t>({0xde, 0xad, 0x6a, 0x82}),
        "failed SELECT process response clears applet selection");
  card.process_status = 0x9000;

  message = applet_select();
  card.restore_result = 1;
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            !session.applet_selected() &&
            response == std::vector<uint8_t>({0x6f, 0x00}),
        "failed restore returns error status");
  card.restore_result = 0;
  card.select_status = 0x6999;
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            !session.applet_selected() &&
            response == std::vector<uint8_t>({0x69, 0x99}),
        "failed applet select preserves VM status");
  card.select_status = 0x9000;
  card.process_status = 0x6103;
  result = session.handle_message(message.data(), message.size(), response);
  check(result == choupi::vpcd::SessionResult::Response &&
            session.applet_selected() &&
            response == std::vector<uint8_t>({0xde, 0xad, 0x61, 0x03}),
        "SELECT 61xx keeps applet selected for GET RESPONSE");
  const std::vector<uint8_t> get_response{0x00, 0xc0, 0x00, 0x00, 0x03};
  card.process_status = 0x9000;
  result = session.handle_message(get_response.data(), get_response.size(),
                                  response);
  check(result == choupi::vpcd::SessionResult::Response &&
            session.applet_selected() && card.last_command == get_response &&
            response == std::vector<uint8_t>({0xde, 0xad, 0x90, 0x00}),
        "GET RESPONSE dispatches to applet after SELECT 61xx");

  const uint8_t bad_control = 0x03;
  result = session.handle_message(&bad_control, 1, response);
  check(result == choupi::vpcd::SessionResult::ProtocolError,
        "unknown control byte is rejected");
  result = session.handle_message(nullptr, 0, response);
  check(result == choupi::vpcd::SessionResult::ProtocolError,
        "empty message is rejected");
  session.disconnect();
  check(!session.powered() && !session.applet_selected(),
        "disconnect clears card session state");
  check(card.card_manager_reset_count != 0,
        "disconnect resets card-manager security state");

  std::printf("TEST:SUMMARY pass=%u fail=%u\n", passed, failed);
  return failed == 0 ? 0 : 1;
}
