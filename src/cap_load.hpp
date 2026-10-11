#ifndef PICO2_JCVM_CAP_LOAD_HPP
#define PICO2_JCVM_CAP_LOAD_HPP

#include <cstddef>
#include <cstdint>

class CapLoadSession {
public:
  void reset() noexcept;
  void abortTransfer() noexcept;

  bool beginInstallForLoad(const uint8_t *data, size_t length) noexcept;
  bool addLoadBlock(uint8_t p1, uint8_t p2, const uint8_t *data,
                    size_t length) noexcept;
  bool installAndMakeSelectable(const uint8_t *data,
                                size_t length) noexcept;

  bool installed() const noexcept { return installed_; }
  const uint8_t *instanceAid() const noexcept { return instance_aid_; }
  uint8_t instanceAidLength() const noexcept { return instance_aid_length_; }

private:
  static constexpr size_t kMaxLoadTlvLength = 28 * 1024;
  uint8_t load_data_[kMaxLoadTlvLength]{};
  uint8_t load_package_aid_[16]{};
  uint8_t load_package_aid_length_ = 0;
  uint8_t applet_aid_[16]{};
  uint8_t applet_aid_length_ = 0;
  uint8_t instance_aid_[16]{};
  uint8_t instance_aid_length_ = 0;
  size_t load_data_length_ = 0;
  uint16_t next_block_ = 0;
  bool loading_ = false;
  bool loaded_ = false;
  bool installed_ = false;

  bool finishLoad() noexcept;
};

#endif
