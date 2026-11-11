#include "ccid_apdu_route.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

namespace {

void check(const bool condition, const char *name) {
  if (!condition) {
    std::fprintf(stderr, "TEST:FAIL %s\n", name);
    std::exit(1);
  }
  std::printf("TEST:PASS %s\n", name);
}

} // namespace

int main() {
  using pico2_jcvm::CcidApduRoute;
  using pico2_jcvm::route_ccid_apdu;

  constexpr std::array<uint8_t, 5> select_default_isd = {
      0x00, 0xa4, 0x04, 0x00, 0x00};
  constexpr std::array<uint8_t, 13> select_explicit_isd = {
      0x00, 0xa4, 0x04, 0x00, 0x08, 0xa0, 0x00,
      0x00, 0x01, 0x51, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 10> select_applet = {
      0x00, 0xa4, 0x04, 0x00, 0x05, 0xa0, 0x00, 0x00, 0x62, 0x03};
  constexpr std::array<uint8_t, 5> gp_delete = {
      0x80, 0xe4, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 5> public_get_data = {
      0x80, 0xca, 0x00, 0x66, 0x00};
  constexpr std::array<uint8_t, 5> applet_apdu = {
      0x80, 0x10, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 5> applet_get_response = {
      0x00, 0xc0, 0x00, 0x00, 0x00};
  constexpr std::array<uint8_t, 5> secure_get_response = {
      0x84, 0xc0, 0x00, 0x00, 0x00};

  check(route_ccid_apdu(select_default_isd.data(), select_default_isd.size(),
                        true, true) == CcidApduRoute::SelectIsd,
        "default ISD selection routes to card manager");
  check(route_ccid_apdu(select_explicit_isd.data(), select_explicit_isd.size(),
                        true, true) == CcidApduRoute::SelectIsd,
        "explicit ISD AID selection routes to card manager");
  check(route_ccid_apdu(select_applet.data(), select_applet.size(), true,
                        false) == CcidApduRoute::SelectApplet,
        "applet SELECT routes to applet selection handler");
  check(route_ccid_apdu(gp_delete.data(), gp_delete.size(), true, true) ==
            CcidApduRoute::ProtectedCardManager,
        "protected GP command has one card-manager route while applet selected");
  check(pico2_jcvm::authorize_ccid_apdu_route(
            CcidApduRoute::ProtectedCardManager, true, false) ==
            CcidApduRoute::AuthenticationFailed,
        "failed SCP authentication cannot fall through to selected applet");
  check(pico2_jcvm::authorize_ccid_apdu_route(
            CcidApduRoute::ProtectedCardManager, true, true) ==
            CcidApduRoute::ProtectedCardManager,
        "authenticated GP command remains on the card-manager route");
  check(pico2_jcvm::authorize_ccid_apdu_route(
            CcidApduRoute::Applet, true, false) == CcidApduRoute::Applet,
        "active SCP03 does not block ordinary applet APDUs");
  check(pico2_jcvm::authorize_ccid_apdu_route(
            CcidApduRoute::PublicCardManager, true, false) ==
            CcidApduRoute::PublicCardManager,
        "active SCP03 does not block public card-manager commands");
  check(route_ccid_apdu(public_get_data.data(), public_get_data.size(), true,
                        true) == CcidApduRoute::PublicCardManager,
        "public GP GET DATA has one card-manager route while applet selected");
  check(route_ccid_apdu(gp_delete.data(), gp_delete.size(), false, true) ==
            CcidApduRoute::PoweredOff,
        "powered-off card rejects card-manager APDU before dispatch");
  check(pico2_jcvm::route_ccid_xfr_block(
            1, gp_delete.data(), gp_delete.size(), true, true) ==
            CcidApduRoute::InvalidSlot,
        "nonzero CCID slot is rejected before APDU dispatch");
  check(route_ccid_apdu(applet_apdu.data(), applet_apdu.size(), true, true) ==
            CcidApduRoute::Applet,
        "ordinary APDU routes to selected applet");
  check(route_ccid_apdu(applet_apdu.data(), applet_apdu.size(), true, false) ==
            CcidApduRoute::NoApplet,
        "ordinary APDU has no applet route when none is selected");
  check(route_ccid_apdu(applet_get_response.data(), applet_get_response.size(),
                        true, true) == CcidApduRoute::Applet,
        "applet GET RESPONSE stays with selected applet without manager data");
  check(route_ccid_apdu(applet_get_response.data(), applet_get_response.size(),
                        true, true, true) == CcidApduRoute::PublicCardManager,
        "GET RESPONSE routes to manager while its segmented response is pending");
  check(route_ccid_apdu(secure_get_response.data(), secure_get_response.size(),
                        true, true) == CcidApduRoute::PublicCardManager,
        "secure-channel GET RESPONSE stays on manager path");
  check(route_ccid_apdu(nullptr, 5, true, false) == CcidApduRoute::Invalid,
        "null APDU input is invalid");
  std::puts("TEST:SUMMARY pass=17 fail=0");
  return 0;
}
