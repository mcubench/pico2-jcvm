/*
** The MIT License (MIT)
**
** Copyright (c) 2020, National Cybersecurity Agency of France (ANSSI)
**
** Permission is hereby granted, free of charge, to any person obtaining a copy
** of this software and associated documentation files (the "Software"), to deal
** in the Software without restriction, including without limitation the rights
** to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
** copies of the Software, and to permit persons to whom the Software is
** furnished to do so, subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in
** all copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
** AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
** LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
** OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
** THE SOFTWARE.
**
** Author:
**   - Guillaume Bouffard <guillaume.bouffard@ssi.gouv.fr>
*/

#ifdef PC_VERSION

#include "debug.hpp"
#include "ffi.h"
#include "interpretor.hpp"
#include "jc_config.h"
#include "jni.hpp"
#include "types.hpp"

#ifdef DEBUG
#include <iomanip>
#include <iostream>
#endif /* DEBUG */

#include "jc_handlers/flashmemory.hpp"
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "ffi.h"
#include "cap_load.hpp"
#include "cap_store.hpp"
#include "pc_rommask.hpp"
#include "scp03.hpp"
#include "vpcd_protocol.hpp"
#include "vpcd_session.hpp"
#include "gp_delete.hpp"
#include "gp_get_data.hpp"
#include "gp_registry.hpp"
#include "apdu_status.hpp"
#include "short_apdu.hpp"
#include "../../src/vm_lifecycle.h"

bool isSaving = false;
uint8_t *flash = nullptr;
uint32_t flash_length = 0;
std::string flash_filename;
std::string vpcd_endpoint;
unsigned int vpcd_idle_timeout_seconds = 0;
std::vector<uint8_t> vpcd_applet_aid;
std::vector<uint8_t> vpcd_cap_code;
std::vector<uint8_t> vpcd_package_aid;
uint8_t vpcd_package_id = 63;
uint16_t vpcd_applet_class_index = 0xffff;
bool vpcd_configured = false;
bool vpcd_install_requested = false;
bool vpcd_registry_resume_requested = false;
bool pc_storage_initialization_failed = false;
bool vpcd_package_loaded = false;
std::vector<uint8_t> vpcd_install_parameters;
std::vector<uint8_t> vpcd_stored_instance_aid;
std::vector<uint8_t> vpcd_stored_applet_aid;
uint16_t vpcd_stored_class_index = 0xffff;
bool vpcd_restore_applet_requested = false;
CapLoadSession vpcd_cap_load_session;
Scp03Session vpcd_secure_channel;
bool vpcd_get_status_first_seen = false;
pico2_jcvm::ShortApduResponseChain vpcd_pending_card_manager_response;
uint16_t vpcd_get_status_cursor = 0;
uint8_t vpcd_get_status_p1 = 0;
std::vector<uint8_t> vpcd_get_status_search_data;
std::vector<choupi::pc::RomPackage> host_rom_packages;
std::vector<const uint8_t *> host_rom_package_data;
constexpr uint32_t kHostFlashLength = 0x80000;

struct HostAppletInstance {
  std::vector<uint8_t> applet_aid;
  std::vector<uint8_t> instance_aid;
  uint16_t class_index = 0xffff;
  uint8_t owner_id = 0;
};

struct HostCapPackage {
  uint8_t package_id = 0xff;
  std::vector<uint8_t> package_aid;
  std::vector<uint8_t> code;
  std::vector<HostAppletInstance> instances;
};

struct VpcdRestoreApplet {
  uint8_t package_id = 0xff;
  uint16_t class_index = 0xffff;
  std::vector<uint8_t> applet_aid;
  std::vector<uint8_t> instance_aid;
  std::string cap_code_filename;
};

// The device filesystem has a package-indexed CAP store. Keep the PC/VPCD
// backend equivalent so independent CAP LOAD commands in one process can
// coexist rather than replacing the last package that was loaded.
std::vector<HostCapPackage> host_cap_packages;

HostCapPackage *host_package(uint8_t package_id) noexcept {
  for (auto &package : host_cap_packages)
    if (package.package_id == package_id) return &package;
  return nullptr;
}

const HostCapPackage *host_package_read(uint8_t package_id) noexcept {
  for (const auto &package : host_cap_packages)
    if (package.package_id == package_id) return &package;
  return nullptr;
}

HostAppletInstance *host_instance(HostCapPackage &package,
                                  const uint8_t *aid,
                                  uint8_t aid_length) noexcept {
  if (aid == nullptr) return nullptr;
  for (auto &instance : package.instances)
    if (instance.instance_aid.size() == aid_length &&
        std::memcmp(instance.instance_aid.data(), aid, aid_length) == 0)
      return &instance;
  return nullptr;
}

extern "C" uint8_t choupi_cap_package_applet_aid(
    uint8_t package_id, uint8_t applet_index, uint8_t *aid,
    uint8_t *aid_length);

// PC vpcd runs may load the same contiguous CAP component data used by the VM
// package resolver. The CAP code is supplied independently from the emulator's
// persistent filesystem image.
extern "C" const uint8_t *choupi_cap_store_data(uint32_t *length) {
  if (length == nullptr || vpcd_cap_code.empty()) {
    return nullptr;
  }
  *length = static_cast<uint32_t>(vpcd_cap_code.size());
  return vpcd_cap_code.data();
}

extern "C" const uint8_t *choupi_cap_store_data_for_package(
    uint8_t package_id, uint32_t *length) {
  const HostCapPackage *package = host_package_read(package_id);
  if (length == nullptr || package == nullptr || package->code.empty())
    return nullptr;
  *length = static_cast<uint32_t>(package->code.size());
  return package->code.data();
}

extern "C" const uint8_t *choupi_pc_rom_package_data(uint8_t package_id,
                                                       uint32_t *length) {
  if (length == nullptr || package_id >= host_rom_packages.size() ||
      package_id >= host_rom_package_data.size()) {
    return nullptr;
  }
  *length = static_cast<uint32_t>(
      host_rom_packages[package_id].components.size());
  return host_rom_package_data[package_id];
}

extern "C" uint8_t choupi_store_cap_package(uint8_t package_id,
                                               const uint8_t *package_aid,
                                               uint8_t package_aid_length,
                                               const uint8_t *data,
                                               uint32_t length) {
  if (package_id < 56 || package_id > 63 || package_aid == nullptr ||
      package_aid_length < 5 || package_aid_length > 16 || data == nullptr ||
      length == 0 || length > 64 * 1024) {
    return 1;
  }
  HostCapPackage *stored = host_package(package_id);
  if (stored != nullptr) {
    if (stored->package_aid.size() != package_aid_length ||
        std::memcmp(stored->package_aid.data(), package_aid,
                    package_aid_length) != 0)
      return 1;
    stored->code.assign(data, data + length);
    return 0;
  }
  HostCapPackage package;
  package.package_id = package_id;
  package.package_aid.assign(package_aid, package_aid + package_aid_length);
  package.code.assign(data, data + length);
  host_cap_packages.push_back(std::move(package));
  return 0;
}

extern "C" uint8_t choupi_store_cap_instance(uint8_t package_id,
                                               const uint8_t *applet_aid,
                                               uint8_t applet_aid_length,
                                               const uint8_t *instance_aid,
                                               uint8_t instance_aid_length,
                                               uint16_t applet_class_index,
                                               uint8_t owner_id) {
  static bool failure_reported = false;
  const char *fail_store =
      std::getenv("CHOUPI_TEST_FAIL_CAP_INSTANCE_STORE_ONCE");
  if (fail_store != nullptr && fail_store[0] == '1' && !failure_reported) {
    failure_reported = true;
    std::fprintf(stderr,
                 "VPCD:TEST_FAILED_CAP_INSTANCE_STORE package=%u\n",
                 static_cast<unsigned>(package_id));
    return 1;
  }
  static bool failure_injected = false;
  const char *skip_store =
      std::getenv("CHOUPI_TEST_SKIP_CAP_INSTANCE_STORE_ONCE");
  if (skip_store != nullptr && skip_store[0] == '1' && !failure_injected) {
    failure_injected = true;
    std::fprintf(stderr,
                 "VPCD:TEST_SKIPPED_CAP_INSTANCE_STORE package=%u\n",
                 static_cast<unsigned>(package_id));
    return 0;
  }
  HostCapPackage *package = host_package(package_id);
  if (package == nullptr || applet_aid == nullptr ||
      applet_aid_length < 5 || applet_aid_length > 16 ||
      instance_aid == nullptr || instance_aid_length < 5 ||
      instance_aid_length > 16 ||
      applet_class_index == 0xffff) {
    return 1;
  }
  HostAppletInstance *stored = host_instance(*package, instance_aid,
                                             instance_aid_length);
  if (stored == nullptr) {
    if (package->instances.size() >= 0xff) return 1;
    package->instances.emplace_back();
    stored = &package->instances.back();
  }
  stored->applet_aid.assign(applet_aid, applet_aid + applet_aid_length);
  stored->instance_aid.assign(instance_aid, instance_aid + instance_aid_length);
  stored->class_index = applet_class_index;
  stored->owner_id = owner_id;
  std::fprintf(stderr,
               "VPCD:APPLET_INSTANCE package-id=%u class-index=%u "
               "applet-aid=",
               static_cast<unsigned>(package_id),
               static_cast<unsigned>(applet_class_index));
  for (uint8_t index = 0; index < applet_aid_length; ++index)
    std::fprintf(stderr, "%02X", applet_aid[index]);
  std::fprintf(stderr, " instance-aid=");
  for (uint8_t index = 0; index < instance_aid_length; ++index)
    std::fprintf(stderr, "%02X", instance_aid[index]);
  std::fprintf(stderr, " owner-id=%u\n", static_cast<unsigned>(owner_id));
  return 0;
}

namespace {
uint8_t store_host_cap_instance_with_new_owner(
    uint8_t package_id, const uint8_t *applet_aid,
    uint8_t applet_aid_length, const uint8_t *instance_aid,
    uint8_t instance_aid_length, uint16_t applet_class_index) {
  uint8_t owner_id = 0;
  if (!cap_store_allocate_applet_owner(owner_id)) return 1;
  return choupi_store_cap_instance(package_id, applet_aid, applet_aid_length,
                                   instance_aid, instance_aid_length,
                                   applet_class_index, owner_id);
}

uint8_t store_host_cap_instance_with_owner(
    uint8_t package_id, const uint8_t *applet_aid,
    uint8_t applet_aid_length, const uint8_t *instance_aid,
    uint8_t instance_aid_length, uint16_t applet_class_index,
    uint8_t owner_id) {
  return choupi_store_cap_instance(package_id, applet_aid, applet_aid_length,
                                   instance_aid, instance_aid_length,
                                   applet_class_index, owner_id);
}
} // namespace

bool cap_store_find_package(const uint8_t *aid, uint8_t aid_length,
                            uint8_t &package_id) noexcept {
  if (aid == nullptr) return false;
  for (const auto &package : host_cap_packages) {
    if (package.package_aid.size() == aid_length &&
        !package.package_aid.empty() &&
        std::memcmp(aid, package.package_aid.data(), aid_length) == 0) {
      package_id = package.package_id;
      return true;
    }
  }
  return false;
}

bool cap_store_write_package(uint8_t package_id, const uint8_t *aid,
                             uint8_t aid_length, const uint8_t *data,
                             size_t length) noexcept {
  return choupi_store_cap_package(package_id, aid, aid_length, data,
                                  static_cast<uint32_t>(length)) == 0;
}

bool cap_store_delete_package(uint8_t package_id) noexcept {
  const auto found = std::find_if(host_cap_packages.begin(),
                                  host_cap_packages.end(),
                                  [package_id](const HostCapPackage &item) {
                                    return item.package_id == package_id;
                                  });
  if (found == host_cap_packages.end()) return false;
  host_cap_packages.erase(found);
  if (package_id == vpcd_package_id) {
    vpcd_package_loaded = false;
    vpcd_cap_code.clear();
    vpcd_package_aid.clear();
    vpcd_stored_applet_aid.clear();
    vpcd_stored_instance_aid.clear();
    vpcd_stored_class_index = 0xffff;
  }
  return true;
}

const uint8_t *cap_store_data_for_package(uint8_t package_id,
                                          uint32_t *length) noexcept {
  return choupi_cap_store_data_for_package(package_id, length);
}

bool cap_store_package_aid(uint8_t package_id, uint8_t *aid,
                           uint8_t &aid_length) noexcept {
  const HostCapPackage *package = host_package_read(package_id);
  if (package == nullptr || aid == nullptr || package->package_aid.empty())
    return false;
  aid_length = static_cast<uint8_t>(package->package_aid.size());
  std::copy(package->package_aid.begin(), package->package_aid.end(), aid);
  return true;
}

bool cap_store_allocate_applet_owner(uint8_t &owner_id) noexcept {
  bool used[256]{};
  for (const auto &package : host_cap_packages)
    for (const auto &instance : package.instances)
      if (instance.owner_id != 0) used[instance.owner_id] = true;
  for (uint16_t candidate = 1; candidate <= UINT8_MAX; ++candidate) {
    if (!used[candidate]) {
      owner_id = static_cast<uint8_t>(candidate);
      return true;
    }
  }
  return false;
}

bool cap_store_applet_owner(const uint8_t *aid, uint8_t aid_length,
                            uint8_t &owner_id) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return false;
  for (const auto &package : host_cap_packages)
    for (const auto &instance : package.instances)
      if (instance.instance_aid.size() == aid_length &&
          std::memcmp(instance.instance_aid.data(), aid, aid_length) == 0) {
        owner_id = instance.owner_id;
        return true;
      }
  return false;
}

bool cap_store_mark_package_installed(uint8_t package_id, const uint8_t *aid,
                                      uint8_t aid_length,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index,
                                      uint8_t owner_id) noexcept {
  return choupi_store_cap_instance(package_id, aid, aid_length, instance_aid,
                                   instance_aid_length,
                                   applet_class_index, owner_id) == 0;
}

bool cap_store_mark_package_installed(uint8_t package_id, const uint8_t *aid,
                                      uint8_t aid_length,
                                      const uint8_t *instance_aid,
                                      uint8_t instance_aid_length,
                                      uint16_t applet_class_index) noexcept {
  uint8_t owner_id = 0;
  return cap_store_allocate_applet_owner(owner_id) &&
         cap_store_mark_package_installed(
             package_id, aid, aid_length, instance_aid, instance_aid_length,
             applet_class_index, owner_id);
}

bool cap_store_remove_package_instance(uint8_t package_id,
                                       const uint8_t *aid,
                                       uint8_t aid_length) noexcept {
  HostCapPackage *package = host_package(package_id);
  if (package == nullptr || aid == nullptr) return false;
  const auto found = std::find_if(
      package->instances.begin(), package->instances.end(),
      [aid, aid_length](const HostAppletInstance &instance) {
        return instance.instance_aid.size() == aid_length &&
               std::memcmp(instance.instance_aid.data(), aid, aid_length) == 0;
      });
  if (found == package->instances.end()) return false;
  package->instances.erase(found);
  if (package_id == vpcd_package_id &&
      vpcd_stored_instance_aid.size() == aid_length &&
      std::memcmp(vpcd_stored_instance_aid.data(), aid, aid_length) == 0) {
    vpcd_stored_instance_aid.clear();
    vpcd_stored_applet_aid.clear();
    vpcd_stored_class_index = 0xffff;
  }
  return true;
}

bool cap_store_find_instance(const uint8_t *aid, uint8_t aid_length,
                             uint8_t &package_id, uint8_t *applet_aid,
                             uint8_t &applet_aid_length,
                             uint16_t &class_index) noexcept {
  if (aid == nullptr || applet_aid == nullptr) return false;
  for (const auto &package : host_cap_packages) {
    for (const auto &instance : package.instances) {
      if (instance.instance_aid.size() == aid_length &&
          !instance.instance_aid.empty() &&
          std::memcmp(aid, instance.instance_aid.data(), aid_length) == 0 &&
          instance.class_index != 0xffff) {
        package_id = package.package_id;
        applet_aid_length = static_cast<uint8_t>(instance.applet_aid.size());
        std::copy(instance.applet_aid.begin(), instance.applet_aid.end(),
                  applet_aid);
        class_index = instance.class_index;
        return true;
      }
    }
  }
  return false;
}

uint8_t cap_store_package_instance_count(uint8_t package_id) noexcept {
  const HostCapPackage *package = host_package_read(package_id);
  return package == nullptr ? 0 : static_cast<uint8_t>(package->instances.size());
}

bool cap_store_package_instance_at(uint8_t package_id, uint8_t index,
                                   uint8_t *applet_aid,
                                   uint8_t &applet_aid_length,
                                   uint8_t *instance_aid,
                                   uint8_t &instance_aid_length,
                                   uint16_t &class_index) noexcept {
  const HostCapPackage *package = host_package_read(package_id);
  if (package == nullptr || index >= package->instances.size() ||
      applet_aid == nullptr || instance_aid == nullptr) return false;
  const HostAppletInstance &instance = package->instances[index];
  applet_aid_length = static_cast<uint8_t>(instance.applet_aid.size());
  instance_aid_length = static_cast<uint8_t>(instance.instance_aid.size());
  std::copy(instance.applet_aid.begin(), instance.applet_aid.end(), applet_aid);
  std::copy(instance.instance_aid.begin(), instance.instance_aid.end(),
            instance_aid);
  class_index = instance.class_index;
  return true;
}

bool cap_store_initialize() noexcept {
  return !host_cap_packages.empty() || !vpcd_cap_code.empty();
}

bool cap_store_write(const uint8_t *data, size_t length) noexcept {
  return data != nullptr && length != 0 && length <= 64 * 1024 &&
         !vpcd_package_aid.empty() &&
         cap_store_write_package(vpcd_package_id, vpcd_package_aid.data(),
                                 static_cast<uint8_t>(vpcd_package_aid.size()),
                                 data, length);
}

bool cap_store_mark_installed(const uint8_t *aid, uint8_t aid_length,
                              uint16_t applet_class_index) noexcept {
  uint8_t owner_id = 0;
  if (!cap_store_allocate_applet_owner(owner_id)) return false;
  return choupi_store_cap_instance(vpcd_package_id, aid, aid_length, aid,
                                   aid_length, applet_class_index,
                                   owner_id) == 0;
}

const uint8_t *cap_store_data(uint32_t *length) noexcept {
  if (vpcd_package_loaded)
    return choupi_cap_store_data_for_package(vpcd_package_id, length);
  return choupi_cap_store_data(length);
}

bool cap_store_instance_aid(uint8_t *aid, uint8_t &aid_length) noexcept {
  if (aid == nullptr || vpcd_stored_instance_aid.empty()) return false;
  aid_length = static_cast<uint8_t>(vpcd_stored_instance_aid.size());
  std::copy(vpcd_stored_instance_aid.begin(), vpcd_stored_instance_aid.end(),
            aid);
  return true;
}

bool cap_store_applet_class_index(uint16_t &class_index) noexcept {
  if (vpcd_stored_class_index == 0xffff) return false;
  class_index = vpcd_stored_class_index;
  return true;
}

bool load_host_cap_registry(const uint8_t *package_bits,
                            size_t package_bits_length);

extern "C" uint8_t choupi_pc_initialize_storage(void) {
  if (host_rom_packages.empty()) {
    pc_storage_initialization_failed = true;
    return 1;
  }

  uint8_t package_list_tag = 0x00;
  uint8_t package_bits[JCVM_MAX_PACKAGES / 8]{};
  bool write_package_list = fs_exists(&package_list_tag, 1) == 0;
  if (!write_package_list) {
    uint32_t existing_length = 0;
    if (fs_length(&package_list_tag, 1, &existing_length) != 0 ||
        existing_length != sizeof(package_bits) ||
        fs_read(&package_list_tag, 1, package_bits, sizeof(package_bits)) != 0) {
      std::fprintf(stderr, "ERROR: invalid PC package-list record\n");
      pc_storage_initialization_failed = true;
      return 1;
    }
  }

  for (const auto &package : host_rom_packages) {
    const uint8_t mask =
        static_cast<uint8_t>(1u << (package.package_id % 8));
    uint8_t &package_byte = package_bits[package.package_id / 8];
    if ((package_byte & mask) == 0) {
      package_byte |= mask;
      write_package_list = true;
    }
  }

  if (write_package_list &&
      fs_write(&package_list_tag, 1, package_bits, sizeof(package_bits)) != 0) {
    std::fprintf(stderr, "ERROR: unable to persist PC package list\n");
    pc_storage_initialization_failed = true;
    return 1;
  }
  static bool cap_registry_initialized = false;
  if (!cap_registry_initialized) {
    if (host_cap_packages.empty() && !vpcd_restore_applet_requested &&
        !load_host_cap_registry(package_bits, sizeof(package_bits))) {
      pc_storage_initialization_failed = true;
      return 1;
    }
    for (uint8_t package_id = 56; package_id <= 63; ++package_id) {
      if ((package_bits[package_id / 8] & (1u << (package_id % 8))) != 0 &&
          host_package_read(package_id) == nullptr) {
        std::fprintf(stderr,
                     "ERROR: package %u is enabled in saved VPCD memory but "
                     "has no CAP registry entry\n",
                     static_cast<unsigned>(package_id));
        pc_storage_initialization_failed = true;
        return 1;
      }
    }
    if (vpcd_registry_resume_requested) {
      if (!vpcd_cap_load_session.recoverPendingInstalls()) {
        std::fprintf(stderr,
                     "ERROR: unable to recover pending VPCD CAP installs\n");
        pc_storage_initialization_failed = true;
        return 1;
      }
      uint8_t package_id = 0xff;
      uint8_t applet_aid[16]{};
      uint8_t applet_aid_length = 0;
      uint16_t class_index = 0xffff;
      if (!cap_store_find_instance(
              vpcd_applet_aid.data(),
              static_cast<uint8_t>(vpcd_applet_aid.size()), package_id,
              applet_aid, applet_aid_length, class_index)) {
        std::fprintf(stderr,
                     "ERROR: --applet-aid is absent from the restored "
                     "registry\n");
        pc_storage_initialization_failed = true;
        return 1;
      }
      vpcd_package_id = package_id;
      vpcd_applet_class_index = class_index;
      vpcd_package_loaded = true;
    }
    cap_registry_initialized = true;
  }
  return 0;
}

bool parse_hex(const std::string &text, std::vector<uint8_t> &bytes) {
  if (text.empty() || (text.size() & 1u) != 0) {
    return false;
  }
  bytes.clear();
  bytes.reserve(text.size() / 2);
  for (size_t offset = 0; offset < text.size(); offset += 2) {
    unsigned int value = 0;
    const char *begin = text.data() + offset;
    const char *end = begin + 2;
    const auto result = std::from_chars(begin, end, value, 16);
    if (result.ec != std::errc{} || result.ptr != end || value > 0xff) {
      bytes.clear();
      return false;
    }
    bytes.push_back(static_cast<uint8_t>(value));
  }
  return true;
}

bool parse_unsigned(const std::string &text, unsigned int maximum,
                    unsigned int &value) {
  const auto result = std::from_chars(text.data(), text.data() + text.size(),
                                      value, 10);
  return !text.empty() && result.ec == std::errc{} &&
         result.ptr == text.data() + text.size() && value <= maximum;
}

bool read_cap_components(const std::string &filename,
                         std::vector<uint8_t> &code,
                         std::vector<uint8_t> &package_aid) {
  std::ifstream input(filename, std::ifstream::binary);
  if (!input.is_open()) return false;
  code.assign(std::istreambuf_iterator<char>(input),
              std::istreambuf_iterator<char>());
  if (code.size() < 13 || code.size() > 64 * 1024 || code[0] != 1)
    return false;
  const size_t header_length =
      (static_cast<size_t>(code[1]) << 8) | code[2];
  if (header_length < 13 || header_length > code.size() - 3 ||
      code[12] < 5 || code[12] > 16 || code[12] > header_length - 10)
    return false;
  package_aid.assign(code.begin() + 13,
                     code.begin() + 13 + code[12]);
  return true;
}

bool cap_declares_applet(uint8_t package_id, const uint8_t *aid,
                         uint8_t aid_length) {
  if (aid == nullptr) return false;
  for (uint8_t index = 0; index < 0xff; ++index) {
    uint8_t declared_aid[16]{};
    uint8_t declared_length = 0;
    if (!choupi_cap_package_applet_aid(package_id, index, declared_aid,
                                       &declared_length))
      return false;
    if (declared_length == aid_length &&
        std::memcmp(declared_aid, aid, aid_length) == 0)
      return true;
  }
  return false;
}

uint32_t host_registry_crc32(const uint8_t *data, size_t length) {
  uint32_t crc = 0xffffffffu;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (unsigned int bit = 0; bit < 8; ++bit) {
      const uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return ~crc;
}

void append_u16(std::vector<uint8_t> &data, uint16_t value) {
  data.push_back(static_cast<uint8_t>(value >> 8));
  data.push_back(static_cast<uint8_t>(value));
}

void append_u32(std::vector<uint8_t> &data, uint32_t value) {
  data.push_back(static_cast<uint8_t>(value >> 24));
  data.push_back(static_cast<uint8_t>(value >> 16));
  data.push_back(static_cast<uint8_t>(value >> 8));
  data.push_back(static_cast<uint8_t>(value));
}

bool read_u16(const std::vector<uint8_t> &data, size_t &offset,
              uint16_t &value) {
  if (offset > data.size() || data.size() - offset < 2) return false;
  value = static_cast<uint16_t>(data[offset] << 8) | data[offset + 1];
  offset += 2;
  return true;
}

bool read_u32(const std::vector<uint8_t> &data, size_t &offset,
              uint32_t &value) {
  if (offset > data.size() || data.size() - offset < 4) return false;
  value = (static_cast<uint32_t>(data[offset]) << 24) |
          (static_cast<uint32_t>(data[offset + 1]) << 16) |
          (static_cast<uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
  offset += 4;
  return true;
}

bool read_aid(const std::vector<uint8_t> &data, size_t &offset,
              std::vector<uint8_t> &aid) {
  if (offset >= data.size()) return false;
  const uint8_t length = data[offset++];
  if (length < 5 || length > 16 || offset > data.size() ||
      data.size() - offset < length)
    return false;
  aid.assign(data.begin() + offset, data.begin() + offset + length);
  offset += length;
  return true;
}

bool load_host_cap_registry(const uint8_t *package_bits,
                            size_t package_bits_length) {
  const std::string registry_path = flash_filename + ".cap-registry";
  std::ifstream input(registry_path, std::ifstream::binary);
  if (!input.is_open()) {
    for (uint8_t package_id = 56; package_id <= 63; ++package_id) {
      if (package_id / 8 < package_bits_length &&
          (package_bits[package_id / 8] & (1u << (package_id % 8))) != 0 &&
          host_package_read(package_id) == nullptr) {
        std::fprintf(stderr,
                     "ERROR: saved VPCD image has package %u but registry "
                     "sidecar is missing; supply --restore-applet records\n",
                     static_cast<unsigned>(package_id));
        return false;
      }
    }
    return true;
  }

  std::vector<uint8_t> data{std::istreambuf_iterator<char>(input),
                            std::istreambuf_iterator<char>()};
  constexpr size_t kMinimumRegistrySize = 14;
  constexpr size_t kMaximumRegistrySize = 8 * (64 * 1024 + 40 * 37) + 64;
  if (data.size() < kMinimumRegistrySize ||
      data.size() > kMaximumRegistrySize ||
      std::memcmp(data.data(), "VCR1", 4) != 0) {
    std::fprintf(stderr, "ERROR: invalid VPCD CAP registry sidecar\n");
    return false;
  }
  const size_t crc_offset = data.size() - 4;
  size_t tail_offset = crc_offset;
  uint32_t stored_crc = 0;
  if (!read_u32(data, tail_offset, stored_crc) ||
      stored_crc != host_registry_crc32(data.data(), crc_offset)) {
    std::fprintf(stderr, "ERROR: VPCD CAP registry sidecar CRC mismatch\n");
    return false;
  }
  size_t offset = 4;
  const uint8_t registry_version = data[offset++];
  if (registry_version != 1 && registry_version != 2) {
    std::fprintf(stderr, "ERROR: unsupported VPCD CAP registry version\n");
    return false;
  }
  uint32_t image_crc = 0;
  if (!read_u32(data, offset, image_crc) ||
      image_crc != host_registry_crc32(flash, flash_length) ||
      offset >= crc_offset) {
    std::fprintf(stderr,
                 "ERROR: VPCD CAP registry does not match saved memory image\n");
    return false;
  }
  const uint8_t package_count = data[offset++];
  if (package_count > 8) {
    std::fprintf(stderr, "ERROR: VPCD CAP registry has too many packages\n");
    return false;
  }
  bool used_owner_ids[256]{};
  std::vector<uint8_t> seen_package_ids;
  for (uint8_t package_index = 0; package_index < package_count;
       ++package_index) {
    if (offset >= crc_offset) return false;
    const uint8_t package_id = data[offset++];
    std::vector<uint8_t> package_aid;
    uint32_t code_length = 0;
    if (package_id < 56 || package_id > 63 ||
        std::find(seen_package_ids.begin(), seen_package_ids.end(),
                  package_id) != seen_package_ids.end() ||
        !read_aid(data, offset, package_aid) ||
        !read_u32(data, offset, code_length) || code_length < 13 ||
        code_length > 64 * 1024 || offset > crc_offset ||
        code_length > crc_offset - offset) {
      std::fprintf(stderr, "ERROR: malformed VPCD CAP package record\n");
      return false;
    }
    std::vector<uint8_t> code(data.begin() + offset,
                              data.begin() + offset + code_length);
    offset += code_length;
    uint8_t instance_count = 0;
    if (offset >= crc_offset) return false;
    instance_count = data[offset++];
    if (instance_count > 40) {
      std::fprintf(stderr, "ERROR: VPCD registry instance count exceeds 40\n");
      return false;
    }
    const bool package_enabled = package_id / 8 < package_bits_length &&
        (package_bits[package_id / 8] & (1u << (package_id % 8))) != 0;
    if (package_enabled) {
      const uint8_t header_aid_length = code[12];
      const size_t header_length =
          (static_cast<size_t>(code[1]) << 8) | code[2];
      uint8_t existing_package_id = 0xff;
      if (code[0] != 1 || header_length < 13 ||
          header_length > code.size() - 3 || header_aid_length < 5 ||
          header_aid_length > 16 || header_aid_length > header_length - 10 ||
          package_aid.size() != header_aid_length ||
          std::memcmp(code.data() + 13, package_aid.data(),
                      header_aid_length) != 0 ||
          (cap_store_find_package(
               package_aid.data(), static_cast<uint8_t>(package_aid.size()),
               existing_package_id) && existing_package_id != package_id)) {
        std::fprintf(stderr, "ERROR: invalid VPCD CAP package data\n");
        return false;
      }
      if (choupi_store_cap_package(
              package_id, package_aid.data(),
              static_cast<uint8_t>(package_aid.size()), code.data(),
              static_cast<uint32_t>(code.size())) != 0) {
        std::fprintf(stderr, "ERROR: unable to restore VPCD CAP package\n");
        return false;
      }
    }
    for (uint8_t instance_index = 0; instance_index < instance_count;
         ++instance_index) {
      std::vector<uint8_t> applet_aid;
      std::vector<uint8_t> instance_aid;
      uint16_t class_index = 0xffff;
      if (!read_aid(data, offset, applet_aid) ||
          !read_aid(data, offset, instance_aid) ||
          !read_u16(data, offset, class_index) || class_index == 0xffff) {
        std::fprintf(stderr, "ERROR: malformed VPCD applet registry record\n");
        return false;
      }
      if (!package_enabled) continue;
      uint8_t existing_package_id = 0xff;
      uint8_t existing_applet_aid[16]{};
      uint8_t existing_applet_aid_length = 0;
      uint16_t existing_class_index = 0xffff;
      uint8_t owner_id = 0; // VCR1 instances used the original owner 0.
      if (registry_version == 2) {
        if (offset >= crc_offset) {
          std::fprintf(stderr, "ERROR: truncated VPCD applet owner record\n");
          return false;
        }
        owner_id = data[offset++];
        if (owner_id != 0 && used_owner_ids[owner_id]) {
          std::fprintf(stderr,
                       "ERROR: invalid or duplicate VPCD applet owner ID\n");
          return false;
        }
        if (owner_id != 0) used_owner_ids[owner_id] = true;
      }
      if (!cap_declares_applet(package_id, applet_aid.data(),
                               static_cast<uint8_t>(applet_aid.size())) ||
          cap_store_find_instance(
              instance_aid.data(), static_cast<uint8_t>(instance_aid.size()),
              existing_package_id, existing_applet_aid,
              existing_applet_aid_length, existing_class_index) ||
          store_host_cap_instance_with_owner(
              package_id, applet_aid.data(),
              static_cast<uint8_t>(applet_aid.size()), instance_aid.data(),
              static_cast<uint8_t>(instance_aid.size()), class_index,
              owner_id) != 0) {
        std::fprintf(stderr, "ERROR: invalid or duplicate VPCD applet record\n");
        return false;
      }
    }
    seen_package_ids.push_back(package_id);
  }
  if (offset != crc_offset) {
    std::fprintf(stderr, "ERROR: trailing data in VPCD CAP registry\n");
    return false;
  }
  for (uint8_t package_id = 56; package_id <= 63; ++package_id) {
    if (package_id / 8 < package_bits_length &&
        (package_bits[package_id / 8] & (1u << (package_id % 8))) != 0 &&
        host_package_read(package_id) == nullptr) {
      std::fprintf(stderr,
                   "ERROR: enabled package %u is absent from VPCD registry "
                   "sidecar\n",
                   static_cast<unsigned>(package_id));
      return false;
    }
  }
  size_t restored_instances = 0;
  for (const HostCapPackage &package : host_cap_packages)
    restored_instances += package.instances.size();
  std::fprintf(stderr,
               "VPCD:REGISTRY_AUTO_RESTORED packages=%zu instances=%zu\n",
               host_cap_packages.size(), restored_instances);
  return true;
}

bool save_host_cap_registry() {
  const std::string path = flash_filename + ".cap-registry";
  if (host_cap_packages.empty()) {
    (void)std::remove(path.c_str());
    return true;
  }
  if (host_cap_packages.size() > 8) {
    std::fprintf(stderr, "ERROR: host CAP registry exceeds eight packages\n");
    return false;
  }
  std::vector<uint8_t> data{'V', 'C', 'R', '1', 2};
  append_u32(data, host_registry_crc32(flash, flash_length));
  data.push_back(static_cast<uint8_t>(host_cap_packages.size()));
  bool used_owner_ids[256]{};
  for (const HostCapPackage &package : host_cap_packages) {
    if (package.package_id < 56 || package.package_id > 63 ||
        package.package_aid.size() < 5 || package.package_aid.size() > 16 ||
        package.code.size() < 13 || package.code.size() > 64 * 1024 ||
        package.instances.size() > 40) {
      std::fprintf(stderr, "ERROR: invalid host CAP registry state\n");
      return false;
    }
    data.push_back(package.package_id);
    data.push_back(static_cast<uint8_t>(package.package_aid.size()));
    data.insert(data.end(), package.package_aid.begin(),
                package.package_aid.end());
    append_u32(data, static_cast<uint32_t>(package.code.size()));
    data.insert(data.end(), package.code.begin(), package.code.end());
    data.push_back(static_cast<uint8_t>(package.instances.size()));
    for (const HostAppletInstance &instance : package.instances) {
      if (instance.applet_aid.size() < 5 || instance.applet_aid.size() > 16 ||
          instance.instance_aid.size() < 5 ||
          instance.instance_aid.size() > 16 ||
          instance.class_index == 0xffff ||
          (instance.owner_id != 0 && used_owner_ids[instance.owner_id])) {
        std::fprintf(stderr, "ERROR: invalid host applet registry state\n");
        return false;
      }
      if (instance.owner_id != 0) used_owner_ids[instance.owner_id] = true;
      data.push_back(static_cast<uint8_t>(instance.applet_aid.size()));
      data.insert(data.end(), instance.applet_aid.begin(),
                  instance.applet_aid.end());
      data.push_back(static_cast<uint8_t>(instance.instance_aid.size()));
      data.insert(data.end(), instance.instance_aid.begin(),
                  instance.instance_aid.end());
      append_u16(data, instance.class_index);
      data.push_back(instance.owner_id);
    }
  }
  append_u32(data, host_registry_crc32(data.data(), data.size()));
  const std::string temporary_path = path + ".tmp";
  std::ofstream output(temporary_path, std::ofstream::binary | std::ofstream::trunc);
  if (!output.is_open()) return false;
  output.write(reinterpret_cast<const char *>(data.data()),
               static_cast<std::streamsize>(data.size()));
  output.flush();
  if (!output) return false;
  output.close();
  if (std::rename(temporary_path.c_str(), path.c_str()) != 0) {
    (void)std::remove(temporary_path.c_str());
    return false;
  }
  std::fprintf(stderr, "VPCD:CAP_REGISTRY_SAVED packages=%zu bytes=%zu\n",
               host_cap_packages.size(), data.size());
  return true;
}

void append_ber_length(std::vector<uint8_t> &data, size_t length) {
  if (length < 0x80) {
    data.push_back(static_cast<uint8_t>(length));
  } else if (length <= 0xff) {
    data.push_back(0x81);
    data.push_back(static_cast<uint8_t>(length));
  } else {
    data.push_back(0x82);
    data.push_back(static_cast<uint8_t>(length >> 8));
    data.push_back(static_cast<uint8_t>(length));
  }
}

void append_lv(std::vector<uint8_t> &data, const uint8_t *value,
               uint8_t length) {
  data.push_back(length);
  data.insert(data.end(), value, value + length);
}

bool install_vpcd_applet() {
  static constexpr uint8_t isd_aid[] = {0xa0, 0x00, 0x00, 0x01,
                                        0x51, 0x00, 0x00, 0x00};
  if (vpcd_package_id < 56 || vpcd_package_id > 63 ||
      vpcd_package_aid.empty() || vpcd_applet_aid.empty() ||
      vpcd_cap_code.empty() || vpcd_install_parameters.size() > 0xff) {
    std::fprintf(stderr, "ERROR: invalid inputs for PC applet installation\n");
    return false;
  }

  std::vector<uint8_t> load_init;
  append_lv(load_init, vpcd_package_aid.data(),
            static_cast<uint8_t>(vpcd_package_aid.size()));
  append_lv(load_init, isd_aid, sizeof(isd_aid));
  load_init.insert(load_init.end(), {0x00, 0x00, 0x00});
  if (!vpcd_cap_load_session.beginInstallForLoad(load_init.data(),
                                                  load_init.size())) {
    std::fprintf(stderr, "ERROR: PC CAP LOAD initialization rejected\n");
    return false;
  }

  std::vector<uint8_t> load_block{0xc4};
  append_ber_length(load_block, vpcd_cap_code.size());
  load_block.insert(load_block.end(), vpcd_cap_code.begin(),
                    vpcd_cap_code.end());
  if (!vpcd_cap_load_session.addLoadBlock(
          0x80, 0, load_block.data(), load_block.size())) {
    std::fprintf(stderr, "ERROR: PC CAP LOAD data rejected\n");
    return false;
  }

  std::vector<uint8_t> install_data;
  append_lv(install_data, vpcd_package_aid.data(),
            static_cast<uint8_t>(vpcd_package_aid.size()));
  append_lv(install_data, vpcd_applet_aid.data(),
            static_cast<uint8_t>(vpcd_applet_aid.size()));
  append_lv(install_data, vpcd_applet_aid.data(),
            static_cast<uint8_t>(vpcd_applet_aid.size()));
  install_data.insert(install_data.end(), {0x03, 0x00, 0x00, 0x00});
  append_ber_length(install_data, vpcd_install_parameters.size());
  install_data.insert(install_data.end(), vpcd_install_parameters.begin(),
                      vpcd_install_parameters.end());
  install_data.push_back(0x00); // Empty INSTALL token LV.
  if (!vpcd_cap_load_session.installAndMakeSelectable(
          install_data.data(), install_data.size())) {
    std::fprintf(stderr,
                 "ERROR: PC applet INSTALL failed stage=%u sw=%04X vm=%04X "
                 "exception=%u opcode=%02X pc=%08X\n",
                 vpcd_cap_load_session.installFailureStage(),
                 vpcd_cap_load_session.installFailureStatusWord(),
                 vpcd_cap_load_session.installVmStatus(),
                 vpcd_cap_load_session.installVmExceptionId(),
                 vpcd_cap_load_session.installVmExceptionOpcode(),
                 vpcd_cap_load_session.installVmExceptionPc());
    return false;
  }
  if (!vpcd_cap_load_session.appletClassIndex(vpcd_applet_class_index)) {
    std::fprintf(stderr, "ERROR: PC applet install omitted class index\n");
    return false;
  }
  vpcd_package_id = vpcd_cap_load_session.packageId();
  std::fprintf(stderr,
               "VPCD:APPLET_INSTALLED package-id=%u class-index=%u\n",
               static_cast<unsigned>(vpcd_package_id),
               static_cast<unsigned>(vpcd_applet_class_index));
  return true;
}

bool parse_endpoint(const std::string &endpoint, std::string &host,
                    std::string &port) {
  size_t separator = std::string::npos;
  if (!endpoint.empty() && endpoint[0] == '[') {
    const size_t closing = endpoint.find(']');
    if (closing == std::string::npos || closing + 1 >= endpoint.size() ||
        endpoint[closing + 1] != ':') {
      return false;
    }
    host = endpoint.substr(1, closing - 1);
    port = endpoint.substr(closing + 2);
  } else {
    separator = endpoint.rfind(':');
    if (separator == std::string::npos) {
      return false;
    }
    host = endpoint.substr(0, separator);
    port = endpoint.substr(separator + 1);
  }
  unsigned int port_number = 0;
  return !host.empty() && parse_unsigned(port, 65535, port_number) &&
         port_number != 0;
}

int connect_vpcd(const std::string &endpoint) {
  std::string host;
  std::string port;
  if (!parse_endpoint(endpoint, host, port)) {
    std::fprintf(stderr, "ERROR: invalid vpcd endpoint: %s\n",
                 endpoint.c_str());
    return -1;
  }
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo *addresses = nullptr;
  const int lookup = ::getaddrinfo(host.c_str(), port.c_str(), &hints,
                                   &addresses);
  if (lookup != 0) {
    std::fprintf(stderr, "ERROR: resolving vpcd endpoint %s: %s\n",
                 endpoint.c_str(), gai_strerror(lookup));
    return -1;
  }
  constexpr unsigned int kConnectAttempts = 31;
  constexpr auto kConnectRetryDelay = std::chrono::milliseconds(100);
  int last_error = ECONNREFUSED;
  int socket_fd = -1;
  for (unsigned int attempt = 0; attempt < kConnectAttempts; ++attempt) {
    bool refused = false;
    for (addrinfo *address = addresses; address != nullptr;
         address = address->ai_next) {
      socket_fd = ::socket(address->ai_family, address->ai_socktype,
                           address->ai_protocol);
      if (socket_fd < 0) {
        last_error = errno;
        continue;
      }
      if (::connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
        ::freeaddrinfo(addresses);
        return socket_fd;
      }
      last_error = errno;
      refused |= last_error == ECONNREFUSED;
      ::close(socket_fd);
      socket_fd = -1;
    }
    if (!refused || attempt + 1 == kConnectAttempts) {
      break;
    }
    // pcscd's socket-activated VPCD listener can appear just after reader
    // enumeration. Retry refused local connections briefly instead of
    // failing the entire emulator startup on that race.
    std::this_thread::sleep_for(kConnectRetryDelay);
  }
  ::freeaddrinfo(addresses);
  std::fprintf(stderr, "ERROR: connecting to vpcd endpoint %s: %s\n",
               endpoint.c_str(), std::strerror(last_error));
  return -1;
}

void vpcd_clear_applet(void *) { choupi_vm_clear_selected_applet(); }

uint8_t vpcd_restore_applet(void *, const uint8_t *aid, uint8_t aid_length) {
  static bool recovery_skipped = false;
  const char *skip_recovery =
      std::getenv("CHOUPI_TEST_SKIP_INSTALL_RECOVERY_ONCE");
  if (skip_recovery != nullptr && skip_recovery[0] == '1' &&
      !recovery_skipped) {
    recovery_skipped = true;
    std::fprintf(stderr,
                 "VPCD:TEST_SKIPPED_INSTALL_RECOVERY_ONCE\n");
    return 1;
  }
  (void)vpcd_cap_load_session.recoverPendingInstalls();
  if (!vpcd_cap_load_session.restoreInstalledForAid(aid, aid_length)) return 1;
  uint16_t class_index = 0xffff;
  if (!vpcd_cap_load_session.appletClassIndex(class_index)) return 1;
  vpcd_package_id = vpcd_cap_load_session.packageId();
  vpcd_applet_class_index = class_index;
  return choupi_vm_restore_applet(vpcd_package_id, class_index, aid,
                                  aid_length);
}

uint16_t vpcd_select_applet(void *) { return choupi_vm_select_applet(); }

uint16_t vpcd_deselect_applet(void *, const bool selecting_same_applet) {
  return choupi_vm_deselect_applet(selecting_same_applet ? 1 : 0);
}

void vpcd_reset_card_manager(void *) {
  vpcd_secure_channel.reset();
  vpcd_pending_card_manager_response.queue(nullptr, 0);
  vpcd_get_status_first_seen = false;
  vpcd_get_status_cursor = 0;
  vpcd_get_status_search_data.clear();
  vpcd_cap_load_session.abortTransfer();
}

uint16_t vpcd_card_manager_status(uint16_t status, uint8_t *response,
                                  uint16_t response_capacity,
                                  uint16_t *response_length) {
  if (response == nullptr || response_length == nullptr ||
      response_capacity < 2) {
    return 0x6f00;
  }
  response[0] = static_cast<uint8_t>(status >> 8);
  response[1] = static_cast<uint8_t>(status);
  *response_length = 2;
  return status;
}

bool vpcd_write_get_applications_response(uint8_t *response,
                                          size_t response_capacity,
                                          size_t &response_length) {
  return gp::write_get_applications_response(
      response, response_capacity, response_length, [&](auto append_aid) {
        for (const HostCapPackage &package : host_cap_packages) {
          const uint8_t count =
              cap_store_package_instance_count(package.package_id);
          for (uint8_t index = 0; index < count; ++index) {
            uint8_t applet_aid[16]{};
            uint8_t applet_aid_length = 0;
            uint8_t instance_aid[16]{};
            uint8_t instance_aid_length = 0;
            uint16_t class_index = 0xffff;
            if (!cap_store_package_instance_at(
                    package.package_id, index, applet_aid,
                    applet_aid_length, instance_aid, instance_aid_length,
                    class_index) ||
                !append_aid(instance_aid, instance_aid_length)) {
              return false;
            }
          }
        }
        return true;
      });
}

uint16_t vpcd_get_status_page(uint8_t p1, uint8_t p2,
                              const uint8_t *search_data,
                              size_t search_length, uint8_t *response,
                              uint16_t response_capacity,
                              uint16_t *response_length) {
  if (p2 != 0x02 && p2 != 0x03) {
    return vpcd_card_manager_status(0x6a86, response, response_capacity,
                                    response_length);
  }
  if (p2 == 0x02) {
    vpcd_get_status_first_seen = false;
    vpcd_get_status_cursor = 0;
    vpcd_get_status_p1 = p1;
    if (search_length == 0) {
      vpcd_get_status_search_data.clear();
    } else if (search_data != nullptr) {
      vpcd_get_status_search_data.assign(search_data,
                                         search_data + search_length);
    } else {
      return vpcd_card_manager_status(0x6a80, response, response_capacity,
                                      response_length);
    }
  } else {
    const uint16_t next_status = gp::validate_get_status_next(
        vpcd_get_status_first_seen, p1, search_data, search_length);
    if (next_status != 0x9000) {
      return vpcd_card_manager_status(next_status, response,
                                      response_capacity, response_length);
    }
    if (!gp::get_status_query_matches(
            vpcd_get_status_p1, vpcd_get_status_search_data.data(),
            vpcd_get_status_search_data.size(), p1, search_data,
            search_length))
      return vpcd_card_manager_status(0x6985, response, response_capacity,
                                      response_length);
  }

  if (response == nullptr || response_length == nullptr ||
      response_capacity < 2) {
    return 0x6f00;
  }
  size_t output_length = 0;
  uint16_t status = 0x9000;
  if (p1 == 0x80) {
    gp::RegistryView empty{};
    status = gp::build_get_status_first_response(
        p1, search_data, search_length, empty, response,
        response_capacity - 2, output_length);
    vpcd_get_status_cursor = status == 0x9000 ? 1 : 0;
  } else if (p1 == 0x40 || p1 == 0x20 || p1 == 0x10) {
    gp::AidSearch search;
    if (!gp::parse_aid_search(search_data, search_length, search)) {
      status = 0x6a80;
    } else {
      size_t match_index = 0;
      bool page_full = false;
      const auto append_matching = [&](const uint8_t *instance_aid,
                                       uint8_t instance_aid_length,
                                       const uint8_t *package_aid,
                                       uint8_t package_aid_length,
                                       const gp::ExecutableModuleAid *modules,
                                       uint8_t module_count) {
        const uint8_t *match_aid = p1 == 0x40 ? instance_aid : package_aid;
        const uint8_t match_aid_length =
            p1 == 0x40 ? instance_aid_length : package_aid_length;
        if (!gp::aid_matches_search(search, match_aid, match_aid_length))
          return true;
        if (match_index < vpcd_get_status_cursor) {
          ++match_index;
          return true;
        }
        size_t entry_length = 0;
        const size_t capacity = response_capacity - 2 - output_length;
        const bool appended = p1 == 0x40
            ? gp::append_registry_entry(
                  response + output_length, capacity, entry_length,
                  instance_aid, instance_aid_length, package_aid,
                  package_aid_length, 0x07)
            : p1 == 0x10
                  ? gp::append_load_file_entry_modules(
                        response + output_length, capacity, entry_length,
                        package_aid, package_aid_length, modules,
                        module_count)
                  : gp::append_load_file_entry(
                        response + output_length, capacity, entry_length,
                        package_aid, package_aid_length, nullptr, 0, false);
        if (!appended) {
          if (output_length == 0) status = 0x6f00;
          else page_full = true;
          return false;
        }
        output_length += entry_length;
        ++match_index;
        return true;
      };

      for (const HostCapPackage &package : host_cap_packages) {
        gp::ExecutableModuleAid modules[127]{};
        uint8_t module_count = 0;
        if (p1 == 0x10) {
          while (module_count < 127) {
            if (!choupi_cap_package_applet_aid(
                    package.package_id, module_count,
                    modules[module_count].value,
                    &modules[module_count].length)) break;
            ++module_count;
          }
          if (module_count == 127) {
            uint8_t extra_aid[16]{};
            uint8_t extra_length = 0;
            if (choupi_cap_package_applet_aid(package.package_id, 127,
                                               extra_aid, &extra_length)) {
              status = 0x6f00;
              break;
            }
          }
        }
        uint8_t instance_aid[16]{};
        uint8_t applet_aid[16]{};
        uint8_t instance_aid_length = 0;
        uint8_t applet_aid_length = 0;
        uint16_t class_index = 0xffff;
        const uint8_t count = cap_store_package_instance_count(package.package_id);
        if (p1 == 0x40) {
          for (uint8_t index = 0; index < count; ++index) {
            if (!cap_store_package_instance_at(
                    package.package_id, index, applet_aid,
                    applet_aid_length, instance_aid,
                    instance_aid_length, class_index)) {
              status = 0x6f00;
              break;
            }
            if (!append_matching(instance_aid, instance_aid_length,
                                 package.package_aid.data(),
                                 static_cast<uint8_t>(package.package_aid.size()),
                                 modules, module_count)) break;
          }
        } else if (!append_matching(
                       nullptr, 0, package.package_aid.data(),
                       static_cast<uint8_t>(package.package_aid.size()),
                       modules, module_count)) {
          // The failed append is retained for the next GET STATUS page.
        }
        if (status != 0x9000 || page_full) break;
      }
      vpcd_get_status_cursor = static_cast<uint16_t>(match_index);
      if (status == 0x9000 && page_full) status = 0x6310;
    }
  } else {
    status = 0x6a86;
  }

  if (status != 0x9000 && status != 0x6310) {
    return vpcd_card_manager_status(status, response, response_capacity,
                                    response_length);
  }
  if (output_length + 2 > response_capacity) {
    return vpcd_card_manager_status(0x6f00, response, response_capacity,
                                    response_length);
  }
  response[output_length] = static_cast<uint8_t>(status >> 8);
  response[output_length + 1] = static_cast<uint8_t>(status);
  *response_length = static_cast<uint16_t>(output_length + 2);
  vpcd_get_status_first_seen = true;
  return status;
}

uint16_t vpcd_process_card_manager(
    void *, const uint8_t *command, uint16_t command_length,
    uint8_t *response, uint16_t response_capacity,
    uint16_t *response_length) {
  if (command == nullptr || response == nullptr || response_length == nullptr ||
      command_length < 4 || response_capacity < 2) {
    return 0x6f00;
  }
  *response_length = 0;
  const auto status_response = [&](uint16_t status) {
    return vpcd_card_manager_status(status, response, response_capacity,
                                    response_length);
  };

  const bool get_response = command_length >= 5 && command[1] == 0xc0 &&
                            (command[0] == 0x00 || command[0] == 0x84);
  if (!get_response) vpcd_pending_card_manager_response.queue(nullptr, 0);
  if (!vpcd_secure_channel.active() && command_length >= 5 &&
      command[0] == 0x00 && command[1] == 0xc0 &&
      vpcd_pending_card_manager_response.has_pending()) {
    pico2_jcvm::ShortApduCommand parsed{};
    uint16_t status = 0x6a86;
    uint16_t data_length = 0;
    if (pico2_jcvm::parse_short_apdu(command, command_length, parsed) &&
        command[2] == 0x00 && command[3] == 0x00 &&
        parsed.has_expected_response_length) {
      status = 0x9000;
      data_length = vpcd_pending_card_manager_response.take(
          parsed.expected_response_length, response,
          static_cast<uint16_t>(response_capacity - 2), status);
    }
    response[data_length++] = static_cast<uint8_t>(status >> 8);
    response[data_length++] = static_cast<uint8_t>(status);
    *response_length = data_length;
    return status;
  }

  const bool gp_get_data = command_length >= 5 && command[0] == 0x80 &&
                           command[1] == 0xca;
  const bool iso_get_data = command_length >= 5 && command[0] == 0x00 &&
                            command[1] == 0xcb;
  if (!vpcd_secure_channel.active() && (gp_get_data || iso_get_data)) {
    size_t output_length = 0;
    const bool application_list = command[2] == 0x2f && command[3] == 0x00;
    const bool valid_command = application_list
        ? command_length == 8 && command[4] == 0x02 && command[7] == 0x00 &&
              gp::valid_get_applications_tag_list(command + 5, 2)
        : command_length == 5;
    const bool written = valid_command &&
        (application_list
             ? vpcd_write_get_applications_response(
                   response, response_capacity, output_length)
             : gp::write_get_data_response(command[2], command[3], response,
                                           response_capacity, output_length,
                                           gp_get_data));
    if (!written) {
      return status_response(valid_command ? 0x6f00 : 0x6a80);
    }
    if (written && application_list) {
      const size_t data_length = output_length - 2;
      const size_t data_limit = short_apdu_response_data_limit(
          command, command_length, response_capacity - 2);
      if (data_length > data_limit) {
        const size_t remaining = data_length - data_limit;
        if (remaining > pico2_jcvm::ShortApduResponseChain::capacity ||
            !vpcd_pending_card_manager_response.queue(
                response + data_limit, static_cast<uint16_t>(remaining))) {
          return status_response(0x6f00);
        }
        const uint16_t sw2 = remaining >= 256
                                 ? 0
                                 : static_cast<uint16_t>(remaining);
        response[data_limit] = 0x61;
        response[data_limit + 1] = static_cast<uint8_t>(sw2);
        output_length = data_limit + 2;
      }
    }
    *response_length = static_cast<uint16_t>(output_length);
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(response[output_length - 2]) << 8) |
        response[output_length - 1]);
  }

  if (command[0] == 0x80 && command[1] == 0x50) {
    size_t data_length = 0;
    if (!vpcd_secure_channel.initializeUpdate(
            command, command_length, response,
            static_cast<size_t>(response_capacity - 2), data_length) ||
        data_length + 2 > response_capacity) {
      return status_response(command_length >= 5 && command[4] != 0x08
                                 ? 0x6700
                                 : 0x6a86);
    }
    response[data_length] = 0x90;
    response[data_length + 1] = 0x00;
    *response_length = static_cast<uint16_t>(data_length + 2);
    return 0x9000;
  }

  if (command[0] == 0x84 && command[1] == 0x82) {
    if (!vpcd_secure_channel.externalAuthenticate(command, command_length)) {
      return status_response(0x6982);
    }
    return status_response(0x9000);
  }

  size_t command_data_length = 0;
  if (!vpcd_secure_channel.active() ||
      !vpcd_secure_channel.verifyCommand(command, command_length,
                                         command_data_length) ||
      command_length < 5 || command_data_length > command_length - 5) {
    return status_response(0x6982);
  }
  const uint8_t instruction = command[1];
  const uint8_t p1 = command[2];
  const uint8_t p2 = command[3];
  const uint8_t *data = command + 5;
  if (instruction == 0xc0 && vpcd_pending_card_manager_response.has_pending()) {
    pico2_jcvm::ShortApduCommand parsed{};
    uint16_t status = 0x6a86;
    uint16_t data_length = 0;
    if (p1 == 0 && p2 == 0 &&
        pico2_jcvm::parse_short_apdu(command, command_length, parsed) &&
        parsed.has_expected_response_length) {
      status = 0x9000;
      data_length = vpcd_pending_card_manager_response.take(
          parsed.expected_response_length, response,
          static_cast<uint16_t>(response_capacity - 2), status);
    }
    response[data_length++] = static_cast<uint8_t>(status >> 8);
    response[data_length++] = static_cast<uint8_t>(status);
    *response_length = data_length;
    return status;
  }
  if (instruction == 0xf2) {
    const size_t response_data_limit =
        short_apdu_response_data_limit(command, command_length,
                                       response_capacity - 2);
    const auto limited_response_capacity = static_cast<uint16_t>(
        std::min<size_t>(response_capacity, response_data_limit + 2));
    return vpcd_get_status_page(p1, p2, data, command_data_length,
                                response, limited_response_capacity,
                                response_length);
  }
  if (instruction == 0xe4) {
    gp::DeleteRequest request;
    if (!gp::parse_delete_request(p1, p2, data, command_data_length,
                                  request)) {
      return status_response(0x6a80);
    }
    return status_response(vpcd_cap_load_session.deleteObject(
        request.aid, request.aid_length, request.delete_related_objects));
  }
  if (instruction == 0xe6 && p1 == 0x02 && p2 == 0x00) {
    return status_response(vpcd_cap_load_session.beginInstallForLoad(
                               data, command_data_length)
                               ? 0x9000
                               : 0x6a80);
  }
  if (instruction == 0xe8) {
    return status_response(vpcd_cap_load_session.addLoadBlock(
                               p1, p2, data, command_data_length)
                               ? 0x9000
                               : 0x6a80);
  }
  if (instruction == 0xe6 && p1 == 0x0c && p2 == 0x00) {
    if (!vpcd_cap_load_session.installAndMakeSelectable(
            data, command_data_length)) {
      std::fprintf(stderr,
                   "VPCD:INSTALL_FAIL stage=%u sw=%04X vm=%04X exc=%u "
                   "opcode=%02X pc=%08X prefix=%08X operand=%04X "
                   "stack=%u op_history=%08X depth_history=%08X "
                   "debug_stage=%u debug_context=%08X\n",
                   vpcd_cap_load_session.installFailureStage(),
                   vpcd_cap_load_session.installFailureStatusWord(),
                   vpcd_cap_load_session.installVmStatus(),
                   vpcd_cap_load_session.installVmExceptionId(),
                   vpcd_cap_load_session.installVmExceptionOpcode(),
                   vpcd_cap_load_session.installVmExceptionPc(),
                   vpcd_cap_load_session.installVmExceptionPrefix(),
                   vpcd_cap_load_session.installVmExceptionOperand(),
                   vpcd_cap_load_session.installVmExceptionStackDepth(),
                   vpcd_cap_load_session.installVmExceptionOpcodeHistory(),
                   vpcd_cap_load_session.installVmExceptionDepthHistory(),
                   vpcd_cap_load_session.installVmDebugStage(),
                   vpcd_cap_load_session.installVmDebugContext());
      return status_response(
          vpcd_cap_load_session.installFailureStatusWord());
    }
    return status_response(0x9000);
  }
  if (instruction == 0xca || instruction == 0xcb) {
    size_t output_length = 0;
    const bool application_list = p1 == 0x2f && p2 == 0x00;
    const bool valid_command = !application_list ||
        gp::valid_get_applications_tag_list(data, command_data_length);
    const bool written = valid_command &&
        (application_list
             ? vpcd_write_get_applications_response(
                   response, response_capacity, output_length)
             : gp::write_get_data_response(p1, p2, response, response_capacity,
                                           output_length,
                                           instruction == 0xca));
    if (!written) {
      return status_response(valid_command ? 0x6f00 : 0x6a80);
    }
    if (written && application_list) {
      const size_t data_length = output_length - 2;
      const size_t data_limit = short_apdu_response_data_limit(
          command, command_length, response_capacity - 2);
      if (data_length > data_limit) {
        const size_t remaining = data_length - data_limit;
        if (remaining > pico2_jcvm::ShortApduResponseChain::capacity ||
            !vpcd_pending_card_manager_response.queue(
                response + data_limit, static_cast<uint16_t>(remaining))) {
          return status_response(0x6f00);
        }
        const uint16_t sw2 = remaining >= 256
                                 ? 0
                                 : static_cast<uint16_t>(remaining);
        response[data_limit] = 0x61;
        response[data_limit + 1] = static_cast<uint8_t>(sw2);
        output_length = data_limit + 2;
      }
    }
    *response_length = static_cast<uint16_t>(output_length);
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(response[output_length - 2]) << 8) |
        response[output_length - 1]);
  }
  return status_response(0x6d00);
}

uint16_t vpcd_process_apdu(void *, const uint8_t *command,
                           uint16_t command_length, uint8_t *response,
                           uint16_t response_capacity,
                           uint16_t *response_length) {
  return choupi_vm_process_applet(command, command_length, response,
                                  response_capacity, response_length);
}

int run_vpcd_transport() {
  const int socket_fd = connect_vpcd(vpcd_endpoint);
  if (socket_fd < 0) {
    return 1;
  }
  std::fprintf(stderr, "VPCD:CONNECTED endpoint=%s\n", vpcd_endpoint.c_str());
  const choupi::vpcd::CardCallbacks callbacks{
      nullptr, vpcd_clear_applet, vpcd_restore_applet, vpcd_select_applet,
      vpcd_deselect_applet, vpcd_process_apdu, vpcd_process_card_manager,
      vpcd_reset_card_manager};
  choupi::vpcd::Session session(callbacks);
  std::vector<uint8_t> message;
  std::vector<uint8_t> response;
  int result = 0;
  auto last_apdu = std::chrono::steady_clock::now();
  while (true) {
    int timeout_ms = 0;
    if (vpcd_idle_timeout_seconds != 0) {
      const auto idle_limit = std::chrono::seconds(vpcd_idle_timeout_seconds);
      const auto elapsed = std::chrono::steady_clock::now() - last_apdu;
      if (elapsed >= idle_limit) {
        std::fprintf(stderr, "VPCD:IDLE_TIMEOUT seconds=%u\n",
                     vpcd_idle_timeout_seconds);
        break;
      }
      timeout_ms = static_cast<int>(
          std::chrono::duration_cast<std::chrono::milliseconds>(idle_limit - elapsed)
              .count());
      timeout_ms = std::max(timeout_ms, 1);
    }
    const auto read_result =
        choupi::vpcd::read_message(socket_fd, message, timeout_ms);
    if (read_result == choupi::vpcd::ReadResult::Closed) {
      break;
    }
    if (read_result == choupi::vpcd::ReadResult::Timeout) {
      std::fprintf(stderr, "VPCD:IDLE_TIMEOUT seconds=%u\n",
                   vpcd_idle_timeout_seconds);
      break;
    }
    if (read_result != choupi::vpcd::ReadResult::Message) {
      std::fprintf(stderr, "ERROR: invalid vpcd frame (%u)\n",
                   static_cast<unsigned>(read_result));
      result = 1;
      break;
    }
    const bool is_apdu = message.size() > 1;
    const auto session_result = session.handle_message(
        message.data(), message.size(), response);
    if (session_result == choupi::vpcd::SessionResult::ProtocolError) {
      std::fprintf(stderr, "ERROR: invalid vpcd control message\n");
      result = 1;
      break;
    }
    if (session_result == choupi::vpcd::SessionResult::NoResponse) {
      continue;
    }
    if (!choupi::vpcd::write_message(
            socket_fd, response.data(), static_cast<uint16_t>(response.size()))) {
      result = 1;
      break;
    }
    if (is_apdu) {
      last_apdu = std::chrono::steady_clock::now();
    }
  }
  ::close(socket_fd);
  session.disconnect();
  std::fprintf(stderr, "VPCD:DISCONNECTED result=%d\n", result);
  return result;
}

int main_pc(int argc, char *argv[]) {
  bool memory_path_present = false;
  bool save_requested = false;
  bool install_parameters_present = false;
  bool applet_aid_present = false;
  bool package_id_present = false;
  bool class_index_present = false;
  std::string cap_code_filename;
  std::string rom_mask_filename = "build/choupi-pc/flash";
  std::vector<VpcdRestoreApplet> restore_applets;
  for (int arg = 1; arg < argc; ++arg) {
    const std::string option(argv[arg]);
    if (option == "-m" || option == "--memory") {
      if (arg + 1 >= argc || argv[arg + 1][0] == '\0') {
        std::fprintf(stderr, "ERROR: %s requires a memory filename\n",
                     option.c_str());
        return EXIT_FAILURE;
      }
      flash_filename = argv[++arg];
      memory_path_present = true;
    } else if (option == "-s" || option == "--save") {
      save_requested = true;
    } else if (option == "--vpcd") {
      if (arg + 1 >= argc || argv[arg + 1][0] == '\0') {
        std::fprintf(stderr, "ERROR: --vpcd requires HOST:PORT\n");
        return EXIT_FAILURE;
      }
      vpcd_endpoint = argv[++arg];
    } else if (option == "--vpcd-idle-timeout") {
      unsigned int value = 0;
      if (arg + 1 >= argc ||
          !parse_unsigned(argv[++arg], 3600, value) || value == 0) {
        std::fprintf(stderr,
                     "ERROR: --vpcd-idle-timeout requires 1..3600 seconds\n");
        return EXIT_FAILURE;
      }
      vpcd_idle_timeout_seconds = value;
    } else if (option == "--applet-aid") {
      if (arg + 1 >= argc ||
          !parse_hex(argv[arg + 1], vpcd_applet_aid) ||
          vpcd_applet_aid.size() < 5 || vpcd_applet_aid.size() > 16) {
        std::fprintf(stderr,
                     "ERROR: --applet-aid requires 10 to 32 hex digits\n");
        return EXIT_FAILURE;
      }
      ++arg;
      applet_aid_present = true;
    } else if (option == "--package-id") {
      if (arg + 1 >= argc) {
        std::fprintf(stderr, "ERROR: --package-id requires a number\n");
        return EXIT_FAILURE;
      }
      unsigned int value = 0;
      if (!parse_unsigned(argv[++arg], 255, value)) {
        std::fprintf(stderr, "ERROR: invalid --package-id\n");
        return EXIT_FAILURE;
      }
      vpcd_package_id = static_cast<uint8_t>(value);
      package_id_present = true;
    } else if (option == "--class-index") {
      if (arg + 1 >= argc) {
        std::fprintf(stderr, "ERROR: --class-index requires a number\n");
        return EXIT_FAILURE;
      }
      unsigned int value = 0;
      if (!parse_unsigned(argv[++arg], 0xfffe, value)) {
        std::fprintf(stderr, "ERROR: invalid --class-index\n");
        return EXIT_FAILURE;
      }
      vpcd_applet_class_index = static_cast<uint16_t>(value);
      class_index_present = true;
    } else if (option == "--cap-code") {
      if (arg + 1 >= argc || argv[arg + 1][0] == '\0') {
        std::fprintf(stderr, "ERROR: --cap-code requires a filename\n");
        return EXIT_FAILURE;
      }
      cap_code_filename = argv[++arg];
    } else if (option == "--restore-applet") {
      if (arg + 5 >= argc) {
        std::fprintf(stderr,
                     "ERROR: --restore-applet requires ID CLASS APPLET_AID "
                     "INSTANCE_AID CAP_COMPONENTS\n");
        return EXIT_FAILURE;
      }
      unsigned int package_id = 0;
      unsigned int class_index = 0;
      VpcdRestoreApplet restore;
      if (!parse_unsigned(argv[arg + 1], 63, package_id) || package_id < 56 ||
          !parse_unsigned(argv[arg + 2], 0xfffe, class_index) ||
          !parse_hex(argv[arg + 3], restore.applet_aid) ||
          restore.applet_aid.size() < 5 || restore.applet_aid.size() > 16 ||
          !parse_hex(argv[arg + 4], restore.instance_aid) ||
          restore.instance_aid.size() < 5 ||
          restore.instance_aid.size() > 16 || argv[arg + 5][0] == '\0') {
        std::fprintf(stderr, "ERROR: invalid --restore-applet arguments\n");
        return EXIT_FAILURE;
      }
      restore.package_id = static_cast<uint8_t>(package_id);
      restore.class_index = static_cast<uint16_t>(class_index);
      restore.cap_code_filename = argv[arg + 5];
      restore_applets.push_back(std::move(restore));
      vpcd_restore_applet_requested = true;
      arg += 5;
    } else if (option == "--resume") {
      vpcd_registry_resume_requested = true;
    } else if (option == "--rom-mask") {
      if (arg + 1 >= argc || argv[arg + 1][0] == '\0') {
        std::fprintf(stderr, "ERROR: --rom-mask requires a filename\n");
        return EXIT_FAILURE;
      }
      rom_mask_filename = argv[++arg];
    } else if (option == "--install") {
      vpcd_install_requested = true;
    } else if (option == "--install-params") {
      if (arg + 1 >= argc ||
          !parse_hex(argv[arg + 1], vpcd_install_parameters) ||
          vpcd_install_parameters.size() > 0xff) {
        std::fprintf(stderr,
                     "ERROR: --install-params requires up to 510 hex digits\n");
        return EXIT_FAILURE;
      }
      ++arg;
      install_parameters_present = true;
#ifdef DEBUG
    } else if (option == "-h" || option == "--help") {
      std::printf("USAGE: %s [OPTION] -m MEMORY_FILENAME\n\n"
                  "CHOUPI is a secure Java Card open-source implementation.\n"
                  "This proof of concept is powered by ANSSI (www.ssi.gouv.fr).\n"
                  "Last update: %s - %s\n\n"
                  "  -m, --memory MEMORY_FILENAME\n"
                  "  -s, --save\n"
                  "  --vpcd HOST:PORT\n"
                  "  --vpcd-idle-timeout SECONDS (clean save on idle)\n"
                  "  --applet-aid HEX --package-id N --class-index N\n"
                  "  --cap-code COMPONENTS_FILENAME (dynamic package only)\n"
                  "  --restore-applet ID CLASS APPLET_AID INSTANCE_AID "
                  "CAP_COMPONENTS (repeatable)\n"
                  "  --resume (restore VPCD registry sidecar from --memory)\n"
                  "  --rom-mask ROMMASK_FILENAME\n"
                  "  --install --install-params HEX (requires --save)\n"
                  "  -h, --help\n",
                  argv[0], __DATE__, __TIME__);
      return EXIT_SUCCESS;
#endif /* DEBUG */
    } else {
      std::fprintf(stderr, "ERROR: unknown option: %s\n", option.c_str());
      return EXIT_FAILURE;
    }
  }
  if (!memory_path_present) {
    std::fprintf(stderr, "ERROR: required option --memory is missing\n");
    return EXIT_FAILURE;
  }
  if (vpcd_install_requested && !save_requested) {
    std::fprintf(stderr, "ERROR: --install requires --save\n");
    return EXIT_FAILURE;
  }
  if (!vpcd_endpoint.empty()) {
    std::string endpoint_host;
    std::string endpoint_port;
    if (!applet_aid_present ||
        (package_id_present &&
         (vpcd_package_id < 56 || vpcd_package_id > 63)) ||
        !parse_endpoint(vpcd_endpoint, endpoint_host, endpoint_port)) {
      std::fprintf(stderr,
                   "ERROR: --vpcd requires a valid endpoint and "
                   "--applet-aid\n");
      return EXIT_FAILURE;
    }
    vpcd_configured = true;
  } else if (vpcd_install_requested) {
    if (!applet_aid_present ||
        (package_id_present &&
         (vpcd_package_id < 56 || vpcd_package_id > 63))) {
      std::fprintf(stderr,
                   "ERROR: --install requires a dynamic package and an "
                   "--applet-aid\n");
      return EXIT_FAILURE;
    }
  } else if (applet_aid_present || package_id_present || class_index_present ||
             !cap_code_filename.empty() || install_parameters_present) {
    std::fprintf(stderr,
                 "ERROR: applet metadata requires --vpcd or --install\n");
    return EXIT_FAILURE;
  }
  if (vpcd_idle_timeout_seconds != 0 && !vpcd_configured) {
    std::fprintf(stderr,
                 "ERROR: --vpcd-idle-timeout requires --vpcd\n");
    return EXIT_FAILURE;
  }
  if ((vpcd_configured || vpcd_install_requested) &&
      vpcd_package_id >= 56 && vpcd_package_id <= 63 &&
      cap_code_filename.empty() && restore_applets.empty() &&
      !vpcd_registry_resume_requested) {
    std::fprintf(stderr,
                 "ERROR: dynamic package 63 requires --cap-code with the "
                 "contiguous CAP component image\n");
    return EXIT_FAILURE;
  }
  if (!restore_applets.empty() &&
      (!vpcd_configured || vpcd_install_requested || package_id_present ||
       class_index_present || !cap_code_filename.empty() ||
       vpcd_registry_resume_requested)) {
    std::fprintf(stderr,
                 "ERROR: --restore-applet requires VPCD resume mode and "
                 "cannot be combined with single-package metadata\n");
    return EXIT_FAILURE;
  }
  if (vpcd_registry_resume_requested &&
      (!vpcd_configured || vpcd_install_requested || package_id_present ||
       class_index_present || !cap_code_filename.empty())) {
    std::fprintf(stderr,
                 "ERROR: --resume requires VPCD mode and cannot be combined "
                 "with single-package metadata\n");
    return EXIT_FAILURE;
  }
  if (install_parameters_present && !vpcd_install_requested) {
    std::fprintf(stderr, "ERROR: --install-params requires --install\n");
    return EXIT_FAILURE;
  }

#ifdef DEBUG
  std::cout << "Welcome to CHOUPI implementation!" << std::endl
            << "CHOUPI is a secure Java Card open-source implementation."
            << std::endl
            << "This proof of concept is powered by ANSSI (www.ssi.gouv.fr)."
            << std::endl
            << "Last update: " << __DATE__ << " - " << __TIME__ << std::endl
            << std::endl
            << "CHOUPI is starting :)" << std::endl
            << std::endl;
#endif /* DEBUG */

  flash = flash_pointer();
  std::fill(flash, flash + kHostFlashLength, 0xff);
  auto file_memory_in = std::ifstream(flash_filename, std::ifstream::binary);

  if (file_memory_in.is_open()) {
    file_memory_in.seekg(0, std::ifstream::end);
    const std::streamoff image_length = file_memory_in.tellg();
    if (image_length < 0 || image_length > kHostFlashLength) {
      std::fprintf(stderr,
                   "ERROR: memory image is larger than %u bytes\n",
                   kHostFlashLength);
      return EXIT_FAILURE;
    }
    file_memory_in.seekg(0, std::ifstream::beg);
    if (image_length != 0 &&
        !file_memory_in.read(reinterpret_cast<char *>(flash), image_length)) {
      std::fprintf(stderr, "ERROR: unable to read memory image %s\n",
                   flash_filename.c_str());
      return EXIT_FAILURE;
    }
    flash_length = kHostFlashLength;

#ifdef DEBUG
    TRACE_JCVM_ERR("Flash length = %u Byte", flash_length);
#endif /* DEBUG */
  } else {
#ifdef DEBUG
    TRACE_JCVM_ERR("ERROR: Unable to open %s", flash_filename.c_str());
#endif /* DEBUG */
    return EXIT_FAILURE;
  }

  file_memory_in.close();

  isSaving = save_requested;

  std::ifstream rom_mask_in(rom_mask_filename, std::ifstream::binary);
  if (!rom_mask_in.is_open() && rom_mask_filename == "build/choupi-pc/flash") {
    rom_mask_in.clear();
    rom_mask_in.open("flash", std::ifstream::binary);
  }
  if (!rom_mask_in.is_open()) {
    std::fprintf(stderr, "ERROR: unable to open PC ROM mask %s\n",
                 rom_mask_filename.c_str());
    return EXIT_FAILURE;
  }
  const std::istreambuf_iterator<char> rom_mask_begin(rom_mask_in);
  const std::istreambuf_iterator<char> rom_mask_end;
  std::vector<uint8_t> rom_mask_image(rom_mask_begin, rom_mask_end);
  if (!choupi::pc::parse_rommask_packages(
          rom_mask_image.data(), rom_mask_image.size(), host_rom_packages) ||
      STARTING_JAVACARD_PACKAGE >= host_rom_packages.size()) {
    std::fprintf(stderr, "ERROR: unable to parse PC ROM mask packages\n");
    return EXIT_FAILURE;
  }
  uint8_t *program_begin = choupi_program_begin();
  const uint32_t program_size = choupi_program_size();
  size_t program_offset = 0;
  host_rom_package_data.clear();
  host_rom_package_data.reserve(host_rom_packages.size());
  for (const auto &package : host_rom_packages) {
    if (package.components.size() > program_size - program_offset) {
      std::fprintf(stderr,
                   "ERROR: PC ROM packages exceed program memory (%zu > %u)\n",
                   package.components.size(),
                   program_size - static_cast<uint32_t>(program_offset));
      return EXIT_FAILURE;
    }
    uint8_t *package_data = program_begin + program_offset;
    std::copy(package.components.begin(), package.components.end(),
              package_data);
    host_rom_package_data.push_back(package_data);
    program_offset += package.components.size();
  }
  std::fprintf(stderr, "VPCD:ROMMASK packages=%zu start=%u\n",
               host_rom_packages.size(),
               static_cast<unsigned>(STARTING_JAVACARD_PACKAGE));

  for (const VpcdRestoreApplet &restore : restore_applets) {
    std::vector<uint8_t> code;
    std::vector<uint8_t> package_aid;
    if (!read_cap_components(restore.cap_code_filename, code, package_aid)) {
      std::fprintf(stderr,
                   "ERROR: invalid CAP component image for restored package "
                   "%u: %s\n",
                   static_cast<unsigned>(restore.package_id),
                   restore.cap_code_filename.c_str());
      return EXIT_FAILURE;
    }
    uint8_t existing_id = 0xff;
    if (cap_store_find_package(package_aid.data(),
                               static_cast<uint8_t>(package_aid.size()),
                               existing_id) &&
        existing_id != restore.package_id) {
      std::fprintf(stderr,
                   "ERROR: restored package AID is assigned to a different "
                   "package ID\n");
      return EXIT_FAILURE;
    }
    const HostCapPackage *existing = host_package_read(restore.package_id);
    if (existing != nullptr &&
        (existing->package_aid != package_aid || existing->code != code)) {
      std::fprintf(stderr,
                   "ERROR: conflicting CAP images for restored package %u\n",
                   static_cast<unsigned>(restore.package_id));
      return EXIT_FAILURE;
    }
    if (choupi_store_cap_package(
            restore.package_id, package_aid.data(),
            static_cast<uint8_t>(package_aid.size()), code.data(),
            static_cast<uint32_t>(code.size())) != 0 ||
        !cap_declares_applet(restore.package_id, restore.applet_aid.data(),
                             static_cast<uint8_t>(restore.applet_aid.size()))) {
      std::fprintf(stderr,
                   "ERROR: restored applet AID is not declared by package "
                   "%u\n",
                   static_cast<unsigned>(restore.package_id));
      return EXIT_FAILURE;
    }
    uint8_t duplicate_package = 0xff;
    uint8_t duplicate_applet_aid[16]{};
    uint8_t duplicate_applet_aid_length = 0;
    uint16_t duplicate_class_index = 0xffff;
    if (cap_store_find_instance(
            restore.instance_aid.data(),
            static_cast<uint8_t>(restore.instance_aid.size()),
            duplicate_package, duplicate_applet_aid,
            duplicate_applet_aid_length, duplicate_class_index) ||
        store_host_cap_instance_with_new_owner(
            restore.package_id, restore.applet_aid.data(),
            static_cast<uint8_t>(restore.applet_aid.size()),
            restore.instance_aid.data(),
            static_cast<uint8_t>(restore.instance_aid.size()),
            restore.class_index) != 0) {
      std::fprintf(stderr,
                   "ERROR: duplicate or invalid restored applet instance\n");
      return EXIT_FAILURE;
    }
  }
  if (!restore_applets.empty()) {
    uint8_t restored_package_id = 0xff;
    uint8_t restored_applet_aid[16]{};
    uint8_t restored_applet_aid_length = 0;
    uint16_t restored_class_index = 0xffff;
    if (!cap_store_find_instance(
            vpcd_applet_aid.data(),
            static_cast<uint8_t>(vpcd_applet_aid.size()),
            restored_package_id, restored_applet_aid,
            restored_applet_aid_length, restored_class_index)) {
      std::fprintf(stderr,
                   "ERROR: --applet-aid must identify an instance in the "
                   "restored registry\n");
      return EXIT_FAILURE;
    }
    vpcd_package_id = restored_package_id;
    vpcd_applet_class_index = restored_class_index;
    vpcd_package_loaded = true;
    std::fprintf(stderr, "VPCD:REGISTRY_RESTORED packages=%zu instances=%zu\n",
                 host_cap_packages.size(), restore_applets.size());
  }

  if (!cap_code_filename.empty()) {
    if (!read_cap_components(cap_code_filename, vpcd_cap_code,
                             vpcd_package_aid)) {
      std::fprintf(stderr, "ERROR: invalid CAP component image %s\n",
                   cap_code_filename.c_str());
      return EXIT_FAILURE;
    }
    // An install must first register the package through CAP LOAD. In resume
    // mode the caller supplies the already-installed CAP image explicitly.
    vpcd_package_loaded = !vpcd_install_requested && class_index_present;
    if (!vpcd_install_requested && class_index_present) {
      // The PC VPCD host registry is process-local. Reconstruct the common
      // single-instance resume record from the explicit applet metadata so
      // GET STATUS continues to report the installed applet after restart.
      if (choupi_store_cap_package(
              vpcd_package_id, vpcd_package_aid.data(),
              static_cast<uint8_t>(vpcd_package_aid.size()),
              vpcd_cap_code.data(),
              static_cast<uint32_t>(vpcd_cap_code.size())) != 0) {
        std::fprintf(stderr, "ERROR: unable to restore VPCD package record\n");
        return EXIT_FAILURE;
      }
      vpcd_stored_applet_aid = vpcd_applet_aid;
      vpcd_stored_instance_aid = vpcd_applet_aid;
      vpcd_stored_class_index = vpcd_applet_class_index;
      if (store_host_cap_instance_with_new_owner(
              vpcd_package_id, vpcd_stored_applet_aid.data(),
              static_cast<uint8_t>(vpcd_stored_applet_aid.size()),
              vpcd_stored_instance_aid.data(),
              static_cast<uint8_t>(vpcd_stored_instance_aid.size()),
              vpcd_stored_class_index) != 0) {
        std::fprintf(stderr, "ERROR: unable to restore VPCD applet record\n");
        return EXIT_FAILURE;
      }
    }
  }

  // running emulator
  run_emulator();

  return pc_storage_initialization_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}

#ifdef __cplusplus
extern "C" {
#endif

//  The runtime_main function is called in the context 0.
int starting_jcre() {

  TRACE_JCVM_DEBUG("Starting JCRE");

  //  Call GP applet => main security domain
  uint32_t arg = (STARTING_JAVACARD_PACKAGE << 16) |
                 (STARTING_JAVACARD_CLASS << 8) | (STARTING_JAVACARD_METHOD);
  remote_call(2, arg, 0);

  if (pc_storage_initialization_failed) {
    std::fprintf(stderr,
                 "ERROR: PC storage initialization failed; refusing to "
                 "start card service\n");
    return 1;
  }

  bool installed = true;
  if (vpcd_install_requested) {
    installed = install_vpcd_applet();
  }

  if (installed && vpcd_configured) {
    if (run_vpcd_transport() != 0) {
      TRACE_JCVM_ERR("vpcd transport ended with an error");
    }
  }

  if (isSaving) {
    TRACE_JCVM_DEBUG("Saving memory in FLASH_MEMORY file");

    auto flash_memory_out =
        std::ofstream(flash_filename, std::ofstream::binary);
    std::ostream_iterator<char> output(flash_memory_out);
    std::copy(flash, flash + flash_length, output);
    flash_memory_out.close();
    if (!flash_memory_out) {
      std::fprintf(stderr, "ERROR: unable to save memory image %s\n",
                   flash_filename.c_str());
      return 1;
    }
    if (!save_host_cap_registry()) {
      std::fprintf(stderr, "ERROR: unable to save VPCD CAP registry sidecar\n");
      return 1;
    }
  }

  return installed ? 0 : 1;
}
#ifdef __cplusplus
}
#endif

#endif /* PC_VERSION */
