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

#ifndef _JC_CAP_APPLET_HPP
#define _JC_CAP_APPLET_HPP

#include "../jc_utils.hpp"
#include "../jcvm_types/jcvmarray.hpp"
#include "../types.hpp"

namespace jcvm {

struct __attribute__((__packed__)) jc_cap_app {
  uint8_t AID_length; /* Applet AID length = [5,16] */
  // {
  //   uint8_t AID [/* AID_length */]; /* Applet AID */
  //   uint16_t install_method_offset; /* Applet install method offset in the
  //   Method
  //                                 component */
  // }
  uint8_t data[];

  const JCVMArray<const uint8_t> AID() const noexcept {
    // AID_length is a single byte in the CAP Applet component. Do not apply
    // the 16-bit CAP byte-order conversion here: that also reads the first
    // AID byte and produces an invalid length.
    return JCVMArray<const uint8_t>(AID_length, data);
  }

  uint16_t install_method_offset() const noexcept {
    return BYTES_TO_SHORT(this->data[AID_length], this->data[AID_length + 1]);
  }
};

struct __attribute__((__packed__)) jc_cap_applet_component {
  uint8_t tag;   /* Component tag: COMPONENT_Applet (3) */
  uint16_t size; /* Component size */
  uint8_t count; /* Number of applet defined in this package */
  jc_cap_app applets[/* count */]; /* Applet defined in this package */

  // Applet records are variable length (AID length, AID, method offset), so
  // they cannot be indexed as a flat array of jc_cap_app structures.
  const jc_cap_app *findByAID(const uint8_t *aid,
                              const uint8_t aid_length) const noexcept {
    if (aid == nullptr || aid_length < 5 || aid_length > 16) {
      return nullptr;
    }

    const uint16_t component_size = NTOHS(size);
    if (component_size == 0) {
      return nullptr;
    }

    const uint8_t *cursor = reinterpret_cast<const uint8_t *>(applets);
    uint16_t remaining = component_size - 1; // count byte precedes applets
    for (uint8_t index = 0; index < count; ++index) {
      if (remaining < 1) {
        return nullptr;
      }
      const uint8_t record_aid_length = cursor[0];
      const uint16_t record_size =
          static_cast<uint16_t>(1u + record_aid_length + 2u);
      if (record_aid_length < 5 || record_aid_length > 16 ||
          remaining < record_size) {
        return nullptr;
      }

      bool equal = (record_aid_length == aid_length);
      for (uint8_t byte = 0; equal && byte < aid_length; ++byte) {
        equal = cursor[1u + byte] == aid[byte];
      }
      if (equal) {
        return reinterpret_cast<const jc_cap_app *>(cursor);
      }

      cursor += record_size;
      remaining = static_cast<uint16_t>(remaining - record_size);
    }
    return nullptr;
  }

  const JCVMArray<const jc_cap_app> applet() const noexcept {
    return JCVMArray<const jc_cap_app>(NTOHS(count), applets);
  }
};

} // namespace jcvm

#endif /* _JC_CAP_APPLET_HPP */
