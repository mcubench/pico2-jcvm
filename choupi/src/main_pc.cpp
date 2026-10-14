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
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "vpcd_protocol.hpp"
#include "vpcd_session.hpp"

bool isSaving = false;
uint8_t *flash = nullptr;
uint32_t flash_length = 0;
std::string flash_filename;
std::string vpcd_endpoint;
std::vector<uint8_t> vpcd_applet_aid;
std::vector<uint8_t> vpcd_cap_code;
uint8_t vpcd_package_id = 0;
uint16_t vpcd_applet_class_index = 0xffff;
bool vpcd_configured = false;

extern "C" uint8_t choupi_vm_restore_applet(uint8_t package_id,
                                             uint16_t class_index,
                                             const uint8_t *aid,
                                             uint8_t aid_length);
extern "C" void choupi_vm_clear_selected_applet(void);
extern "C" uint16_t choupi_vm_select_applet(void);
extern "C" uint16_t choupi_vm_deselect_applet(void);
extern "C" uint16_t choupi_vm_process_applet(const uint8_t *command,
                                              uint16_t command_length,
                                              uint8_t *response,
                                              uint16_t response_capacity,
                                              uint16_t *response_length);

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
  int socket_fd = -1;
  for (addrinfo *address = addresses; address != nullptr;
       address = address->ai_next) {
    socket_fd = ::socket(address->ai_family, address->ai_socktype,
                         address->ai_protocol);
    if (socket_fd < 0) {
      continue;
    }
    if (::connect(socket_fd, address->ai_addr, address->ai_addrlen) == 0) {
      break;
    }
    ::close(socket_fd);
    socket_fd = -1;
  }
  ::freeaddrinfo(addresses);
  if (socket_fd < 0) {
    std::fprintf(stderr, "ERROR: connecting to vpcd endpoint %s: %s\n",
                 endpoint.c_str(), std::strerror(errno));
  }
  return socket_fd;
}

void vpcd_clear_applet(void *) { choupi_vm_clear_selected_applet(); }

uint8_t vpcd_restore_applet(void *, const uint8_t *aid, uint8_t aid_length) {
  return choupi_vm_restore_applet(vpcd_package_id, vpcd_applet_class_index,
                                  aid, aid_length);
}

uint16_t vpcd_select_applet(void *) { return choupi_vm_select_applet(); }

uint16_t vpcd_deselect_applet(void *) { return choupi_vm_deselect_applet(); }

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
      vpcd_deselect_applet, vpcd_process_apdu};
  choupi::vpcd::Session session(vpcd_applet_aid, callbacks);
  std::vector<uint8_t> message;
  std::vector<uint8_t> response;
  int result = 0;
  while (true) {
    const auto read_result = choupi::vpcd::read_message(socket_fd, message);
    if (read_result == choupi::vpcd::ReadResult::Closed) {
      break;
    }
    if (read_result != choupi::vpcd::ReadResult::Message) {
      std::fprintf(stderr, "ERROR: invalid vpcd frame (%u)\n",
                   static_cast<unsigned>(read_result));
      result = 1;
      break;
    }
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
  }
  ::close(socket_fd);
  session.disconnect();
  std::fprintf(stderr, "VPCD:DISCONNECTED result=%d\n", result);
  return result;
}

int main_pc(int argc, char *argv[]) {
  bool memory_path_present = false;
  bool save_requested = false;
  bool applet_aid_present = false;
  bool package_id_present = false;
  bool class_index_present = false;
  std::string cap_code_filename;
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
#ifdef DEBUG
    } else if (option == "-h" || option == "--help") {
      std::printf("USAGE: %s [OPTION] -m MEMORY_FILENAME\n\n"
                  "CHOUPI is a secure Java Card open-source implementation.\n"
                  "This proof of concept is powered by ANSSI (www.ssi.gouv.fr).\n"
                  "Last update: %s - %s\n\n"
                  "  -m, --memory MEMORY_FILENAME\n"
                  "  -s, --save\n"
                  "  --vpcd HOST:PORT\n"
                  "  --applet-aid HEX --package-id N --class-index N\n"
                  "  --cap-code COMPONENTS_FILENAME (dynamic package only)\n"
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
  if (!vpcd_endpoint.empty()) {
    std::string endpoint_host;
    std::string endpoint_port;
    if (!applet_aid_present || !package_id_present || !class_index_present ||
        !parse_endpoint(vpcd_endpoint, endpoint_host, endpoint_port)) {
      std::fprintf(stderr,
                   "ERROR: --vpcd requires a valid endpoint, --applet-aid, "
                   "--package-id, and --class-index\n");
      return EXIT_FAILURE;
    }
    vpcd_configured = true;
  } else if (applet_aid_present || package_id_present || class_index_present ||
             !cap_code_filename.empty()) {
    std::fprintf(stderr,
                 "ERROR: applet metadata and --cap-code require --vpcd\n");
    return EXIT_FAILURE;
  }
  if (vpcd_configured && vpcd_package_id == 63 && cap_code_filename.empty()) {
    std::fprintf(stderr,
                 "ERROR: dynamic package 63 requires --cap-code with the "
                 "contiguous CAP component image\n");
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

  auto file_memory_in = std::ifstream(flash_filename, std::ifstream::binary);
  flash = flash_pointer();

  if (file_memory_in.is_open()) {
    uint8_t *last = std::copy(std::istreambuf_iterator<char>(file_memory_in),
                              std::istreambuf_iterator<char>(), flash);
    flash_length = (last - flash);

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

  if (!cap_code_filename.empty()) {
    std::ifstream cap_code_in(cap_code_filename, std::ifstream::binary);
    if (!cap_code_in.is_open()) {
      std::fprintf(stderr, "ERROR: unable to open CAP code %s\n",
                   cap_code_filename.c_str());
      return EXIT_FAILURE;
    }
    vpcd_cap_code.assign(std::istreambuf_iterator<char>(cap_code_in),
                         std::istreambuf_iterator<char>());
    if (vpcd_cap_code.empty() || vpcd_cap_code.size() > 28 * 1024) {
      std::fprintf(stderr,
                   "ERROR: CAP component image must contain 1..28672 bytes\n");
      return EXIT_FAILURE;
    }
  }

  // running emulator
  run_emulator();

  return 0;
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

  if (vpcd_configured) {
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
  }

  return 0;
}
#ifdef __cplusplus
}
#endif

#endif /* PC_VERSION */
