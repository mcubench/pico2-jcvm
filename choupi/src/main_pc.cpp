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
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

bool isSaving = false;
uint8_t *flash = nullptr;
uint32_t flash_length = 0;
std::string flash_filename;

int main_pc(int argc, char *argv[]) {
  bool memory_path_present = false;
  bool save_requested = false;
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
#ifdef DEBUG
    } else if (option == "-h" || option == "--help") {
      std::printf("USAGE: %s [OPTION] -m MEMORY_FILENAME\n\n"
                  "CHOUPI is a secure Java Card open-source implementation.\n"
                  "This proof of concept is powered by ANSSI (www.ssi.gouv.fr).\n"
                  "Last update: %s - %s\n\n"
                  "  -m, --memory MEMORY_FILENAME\n"
                  "  -s, --save\n"
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
