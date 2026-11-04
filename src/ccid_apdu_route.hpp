#ifndef PICO2_JCVM_CCID_APDU_ROUTE_HPP
#define PICO2_JCVM_CCID_APDU_ROUTE_HPP

#include <cstddef>
#include <cstdint>

namespace pico2_jcvm {

enum class CcidApduRoute : uint8_t {
  Invalid,
  InvalidSlot,
  PoweredOff,
  SelectIsd,
  SelectApplet,
  PublicCardManager,
  ProtectedCardManager,
  AuthenticationFailed,
  Applet,
  NoApplet,
};

inline constexpr uint8_t kIsdAid[] = {0xa0, 0x00, 0x00, 0x01,
                                       0x51, 0x00, 0x00, 0x00};

inline bool ccid_is_select_by_aid(const uint8_t *apdu,
                                  const size_t apdu_length) noexcept {
  if (apdu == nullptr || apdu_length < 5 || apdu[0] != 0x00 ||
      apdu[1] != 0xa4 || apdu[2] != 0x04 || (apdu[3] & 0xef) != 0 ||
      apdu[4] < 5 || apdu[4] > 16) {
    return false;
  }
  const size_t aid_length = apdu[4];
  return apdu_length == 5u + aid_length || apdu_length == 6u + aid_length;
}

inline bool ccid_is_default_isd_select(const uint8_t *apdu,
                                      const size_t apdu_length) noexcept {
  return apdu != nullptr && apdu_length == 5 && apdu[0] == 0x00 &&
         apdu[1] == 0xa4 && apdu[2] == 0x04 && apdu[3] == 0x00 &&
         apdu[4] == 0x00;
}

inline bool ccid_is_explicit_isd_select(const uint8_t *apdu,
                                       const size_t apdu_length) noexcept {
  if (!ccid_is_select_by_aid(apdu, apdu_length) ||
      apdu[4] != sizeof(kIsdAid)) {
    return false;
  }
  for (size_t index = 0; index < sizeof(kIsdAid); ++index) {
    if (apdu[5 + index] != kIsdAid[index]) return false;
  }
  return true;
}

inline bool ccid_is_gp_manager_command(const uint8_t *apdu,
                                       const size_t apdu_length) noexcept {
  if (apdu == nullptr || apdu_length < 2) return false;
  const uint8_t cla = apdu[0];
  const uint8_t ins = apdu[1];
  if (cla == 0x00 && ins == 0xcb) return true; // ISO GET DATA
  if (ins == 0xc0 && (cla == 0x00 || cla == 0x84)) return true;
  if (cla != 0x80 && cla != 0x84) return false;
  switch (ins) {
  case 0x50: // INITIALIZE UPDATE
  case 0x82: // EXTERNAL AUTHENTICATE
  case 0xca: // GET DATA
  case 0xcb: // GET DATA
  case 0xe4: // DELETE
  case 0xe6: // INSTALL
  case 0xe8: // LOAD
  case 0xf2: // GET STATUS
    return true;
  default:
    return false;
  }
}

inline bool ccid_is_protected_gp_command(const uint8_t *apdu,
                                         const size_t apdu_length) noexcept {
  if (!ccid_is_gp_manager_command(apdu, apdu_length)) return false;
  return !(apdu[1] == 0x50 || apdu[1] == 0x82 || apdu[1] == 0xca ||
           apdu[1] == 0xcb || apdu[1] == 0xc0);
}

inline CcidApduRoute route_ccid_apdu(const uint8_t *apdu,
                                     const size_t apdu_length,
                                     const bool powered,
                                     const bool applet_selected) noexcept {
  if (apdu == nullptr || apdu_length < 2) return CcidApduRoute::Invalid;
  if (!powered) return CcidApduRoute::PoweredOff;
  if (ccid_is_default_isd_select(apdu, apdu_length) ||
      ccid_is_explicit_isd_select(apdu, apdu_length)) {
    return CcidApduRoute::SelectIsd;
  }
  if (ccid_is_select_by_aid(apdu, apdu_length)) {
    return CcidApduRoute::SelectApplet;
  }
  if (ccid_is_gp_manager_command(apdu, apdu_length)) {
    return ccid_is_protected_gp_command(apdu, apdu_length)
               ? CcidApduRoute::ProtectedCardManager
               : CcidApduRoute::PublicCardManager;
  }
  return applet_selected ? CcidApduRoute::Applet : CcidApduRoute::NoApplet;
}

inline CcidApduRoute route_ccid_xfr_block(
    const uint8_t slot, const uint8_t *apdu, const size_t apdu_length,
    const bool powered, const bool applet_selected) noexcept {
  if (slot != 0) return CcidApduRoute::InvalidSlot;
  return route_ccid_apdu(apdu, apdu_length, powered, applet_selected);
}

inline CcidApduRoute authorize_ccid_apdu_route(
    const CcidApduRoute route, const bool secure_channel_active,
    const bool authenticated) noexcept {
  if (secure_channel_active && !authenticated &&
      route != CcidApduRoute::SelectIsd &&
      route != CcidApduRoute::SelectApplet) {
    return CcidApduRoute::AuthenticationFailed;
  }
  return route;
}

} // namespace pico2_jcvm

#endif // PICO2_JCVM_CCID_APDU_ROUTE_HPP
