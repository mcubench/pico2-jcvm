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

#include "jc_cap.hpp"
#include "../exceptions.hpp"
#include <array>
#include <cstddef>
#include <utility>

namespace jcvm {

namespace {

uint16_t read_be16(const uint8_t *bytes) {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                               bytes[1]);
}

bool validate_class_component_21(const uint8_t *payload, const size_t size,
                                 uint16_t *info_offsets = nullptr,
                                 const size_t info_capacity = 0,
                                 size_t *info_count = nullptr) {
  size_t offset = 0;
  size_t record_count = 0;
  const auto record_starts_here = [&]() {
    if (info_offsets != nullptr) {
      if (record_count >= info_capacity || offset > UINT16_MAX) return false;
      info_offsets[record_count] = static_cast<uint16_t>(offset);
    }
    ++record_count;
    return true;
  };

  // Interface records precede class records. Their flag occupies the high
  // nibble and ACC_INTERFACE is the highest bit of the first byte.
  while (offset < size && (payload[offset] & 0x80) != 0) {
    if (!record_starts_here()) return false;
    const uint8_t flags = payload[offset] >> 4;
    const size_t interfaces = payload[offset] & 0x0f;
    if ((flags & 0x02) != 0 || interfaces > 14) return false;
    const size_t record_size = 1 + interfaces * sizeof(jc_cap_class_ref);
    if (record_size > size - offset) return false;
    offset += record_size;
  }

  while (offset < size) {
    if (!record_starts_here()) return false;
    constexpr size_t class_prefix_size = 10;
    if (size - offset < class_prefix_size ||
        (payload[offset] & 0x80) != 0) {
      return false;
    }
    const uint8_t flags = payload[offset] >> 4;
    if ((flags & 0x02) != 0) return false;

    const size_t implemented_interfaces = payload[offset] & 0x0f;
    const size_t public_methods = payload[offset + 7];
    const size_t package_methods = payload[offset + 9];
    offset += class_prefix_size;

    const size_t method_bytes =
        (public_methods + package_methods) * sizeof(uint16_t);
    if (method_bytes > size - offset) return false;
    offset += method_bytes;

    for (size_t index = 0; index < implemented_interfaces; ++index) {
      if (size - offset < 3) return false;
      const size_t method_map_size = payload[offset + 2];
      const size_t record_size = 3 + method_map_size;
      if (record_size > size - offset) return false;
      offset += record_size;
    }
  }

  if (offset != size) return false;
  if (info_count != nullptr) *info_count = record_count;
  return true;
}

bool descriptor_class_refs_match_class_component_21(
    const uint8_t *descriptor, const size_t descriptor_size,
    const uint8_t *class_component, const size_t class_component_size) {
  if (descriptor_size == 0) return false;
  const size_t descriptor_class_count = descriptor[0];
  std::array<uint16_t, 255> class_info_offsets{};
  size_t class_info_count = 0;
  if (!validate_class_component_21(
          class_component, class_component_size, class_info_offsets.data(),
          class_info_offsets.size(), &class_info_count) ||
      class_info_count != descriptor_class_count) {
    return false;
  }

  size_t offset = 1;
  for (size_t index = 0; index < descriptor_class_count; ++index) {
    if (descriptor_size - offset < 9) return false;
    const uint16_t class_ref = read_be16(descriptor + offset + 2);
    if ((class_ref & 0x8000) != 0) return false;
    size_t lower = 0;
    size_t upper = class_info_count;
    while (lower < upper) {
      const size_t middle = lower + (upper - lower) / 2;
      if (class_info_offsets[middle] < class_ref) {
        lower = middle + 1;
      } else {
        upper = middle;
      }
    }
    if (lower == class_info_count || class_info_offsets[lower] != class_ref)
      return false;

    const size_t interfaces = descriptor[offset + 4];
    const size_t fields = read_be16(descriptor + offset + 5);
    const size_t methods = read_be16(descriptor + offset + 7);
    const size_t record_size =
        9 + interfaces * 2 + fields * 7 + methods * 12;
    if (record_size > descriptor_size - offset) return false;
    offset += record_size;
  }

  return true;
}

bool validate_type_descriptor(const uint8_t *type, const size_t nibble_count) {
  if (type == nullptr || nibble_count == 0) return false;
  const auto nibble_at = [type](const size_t index) {
    const uint8_t byte = type[index / 2];
    return static_cast<uint8_t>((index & 1) == 0 ? byte >> 4 : byte & 0x0f);
  };

  size_t index = 0;
  while (index < nibble_count) {
    const uint8_t value = nibble_at(index++);
    if (value == 0x1) {
      // void is only valid as the final method return type.
      if (index != nibble_count) return false;
    } else if ((value >= 0x2 && value <= 0x5) ||
               (value >= 0x0a && value <= 0x0d)) {
      continue;
    } else if (value == 0x6 || value == 0x0e) {
      if (nibble_count - index < 4) return false;
      index += 4;
    } else {
      return false;
    }
  }

  return (nibble_count & 1) == 0 || (nibble_at(nibble_count) == 0);
}

bool validate_descriptor_type_offsets(const uint8_t *descriptor,
                                      const size_t descriptor_size,
                                      const size_t class_count,
                                      const size_t constant_pool_count_offset,
                                      const size_t constant_pool_count,
                                      const size_t type_offset_base,
                                      const size_t type_descriptors_offset) {
  constexpr size_t offset_limit = 1u << 15;
  constexpr size_t chunk_bits = 4096;
  constexpr size_t bitmap_bytes = chunk_bits / 8;
  const size_t constant_pool_types_offset = constant_pool_count_offset + 2;

  for (size_t chunk_start = 0; chunk_start < offset_limit;
       chunk_start += chunk_bits) {
    uint8_t starts[bitmap_bytes] = {};
    size_t type_offset = type_descriptors_offset;
    while (type_offset < descriptor_size) {
      const size_t relative_offset = type_offset - type_offset_base;
      if (relative_offset >= chunk_start &&
          relative_offset - chunk_start < chunk_bits) {
        const size_t bit = relative_offset - chunk_start;
        starts[bit / 8] |= static_cast<uint8_t>(1u << (bit & 7));
      }
      const size_t nibble_count = descriptor[type_offset];
      type_offset += 1 + (nibble_count + 1) / 2;
    }

    const auto offset_is_type_start = [&](const size_t value) {
      if (value < chunk_start || value - chunk_start >= chunk_bits) return true;
      const size_t bit = value - chunk_start;
      return (starts[bit / 8] & static_cast<uint8_t>(1u << (bit & 7))) != 0;
    };

    size_t class_offset = 1;
    for (size_t class_index = 0; class_index < class_count; ++class_index) {
      if (descriptor_size - class_offset < 9) return false;
      const size_t interfaces = descriptor[class_offset + 4];
      const size_t fields = read_be16(descriptor + class_offset + 5);
      const size_t methods = read_be16(descriptor + class_offset + 7);
      const size_t fields_offset = class_offset + 9 + interfaces * 2;
      const size_t methods_offset = fields_offset + fields * 7;

      for (size_t field = 0; field < fields; ++field) {
        const uint16_t field_type =
            read_be16(descriptor + fields_offset + field * 7 + 5);
        if ((field_type & 0x8000) != 0) {
          const uint16_t primitive = field_type & 0x7fff;
          if (primitive < 2 || primitive > 5) return false;
        } else if (!offset_is_type_start(field_type)) {
          return false;
        }
      }

      for (size_t method = 0; method < methods; ++method) {
        const uint16_t method_type =
            read_be16(descriptor + methods_offset + method * 12 + 4);
        if ((method_type & 0x8000) != 0 ||
            !offset_is_type_start(method_type)) {
          return false;
        }
      }

      const size_t record_size =
          9 + interfaces * 2 + fields * 7 + methods * 12;
      if (record_size > descriptor_size - class_offset) return false;
      class_offset += record_size;
    }

    for (size_t index = 0; index < constant_pool_count; ++index) {
      const uint16_t cp_type = read_be16(
          descriptor + constant_pool_types_offset + index * 2);
      if (cp_type == 0xffff) continue;
      if ((cp_type & 0x8000) != 0 || !offset_is_type_start(cp_type))
        return false;
    }
  }

  return true;
}

bool validate_component_payload(const uint8_t tag, const uint8_t *payload,
                                const size_t size, const uint8_t cap_major,
                                const uint8_t cap_minor,
                                const uint8_t *class_payload,
                                const size_t class_payload_size) {
  size_t offset = 0;
  switch (tag) {
  case 1: { // Header
    // Fixed header (magic/version/flags) and package version/AID length.
    if (size < 10 || payload[9] < 5 || payload[9] > 16 ||
        static_cast<size_t>(payload[9]) > size - 10) {
      return false;
    }
    const size_t name_offset = 10 + payload[9];
    if (payload[4] >= 2) {
      if (name_offset >= size ||
          payload[name_offset] > size - name_offset - 1) {
        return false;
      }
    }
    return true;
  }
  case 2: { // Directory
    constexpr size_t fixed_size = 11 * sizeof(uint16_t) + 6 + 3;
    if (size < fixed_size) return false;
    const uint8_t custom_count = payload[fixed_size - 1];
    offset = fixed_size;
    for (uint16_t index = 0; index < custom_count; ++index) {
      if (size - offset < 4 || payload[offset] < 0x80) return false;
      const uint8_t aid_length = payload[offset + 3];
      if (aid_length < 5 || aid_length > 16 ||
          aid_length > size - offset - 4) {
        return false;
      }
      offset += 4 + aid_length;
    }
    return offset == size;
  }
  case 3: { // Applet
    if (size < 1) return false;
    const uint8_t count = payload[offset++];
    for (uint16_t index = 0; index < count; ++index) {
      if (offset >= size) return false;
      const uint8_t aid_length = payload[offset];
      const size_t record_size = 1u + aid_length + 2u;
      if (aid_length < 5 || aid_length > 16 || record_size > size - offset)
        return false;
      offset += record_size;
    }
    return offset == size;
  }
  case 4: { // Import
    if (size < 1) return false;
    const uint8_t count = payload[offset++];
    for (uint16_t index = 0; index < count; ++index) {
      if (size - offset < 3) return false;
      const uint8_t aid_length = payload[offset + 2];
      if (aid_length < 5 || aid_length > 16 ||
          aid_length > size - offset - 3) {
        return false;
      }
      offset += 3 + aid_length;
    }
    return offset == size;
  }
  case 5: { // Constant Pool
    if (size < sizeof(uint16_t)) return false;
    const size_t entries = read_be16(payload);
    return entries == (size - sizeof(uint16_t)) / 4 &&
           (size - sizeof(uint16_t)) % 4 == 0;
  }
  case 6: // Class
    // CAP format 2.1 has neither the signature pool prefix nor remote-info
    // tails. Newer layouts remain accepted but need their own versioned walk.
    return cap_major != 2 || cap_minor != 1 ||
           validate_class_component_21(payload, size);
  case 7: { // Method: exception handler table precedes method data.
    if (size < 1) return false;
    const size_t handler_bytes = static_cast<size_t>(payload[0]) * 8;
    return handler_bytes <= size - 1;
  }
  case 8: { // Static Field
    if (size < 10) return false;
    const uint16_t image_size = read_be16(payload);
    const uint16_t array_count = read_be16(payload + 4);
    offset = 6;
    for (uint16_t index = 0; index < array_count; ++index) {
      if (size - offset < 3) return false;
      const size_t value_size = read_be16(payload + offset + 1);
      if (value_size > size - offset - 3) return false;
      offset += 3 + value_size;
    }
    if (size - offset < 4) return false;
    const uint16_t default_count = read_be16(payload + offset);
    const uint16_t non_default_count = read_be16(payload + offset + 2);
    offset += 4;
    return default_count <= image_size &&
           non_default_count == size - offset;
  }
  case 9: { // Reference Location
    if (size < 4) return false;
    const size_t byte_offsets = read_be16(payload);
    offset = 2 + byte_offsets;
    if (offset > size || size - offset < 2) return false;
    const size_t word_offsets = read_be16(payload + offset);
    offset += 2;
    return word_offsets == size - offset;
  }
  case 10: { // Export
    if (size < 1) return false;
    const uint8_t count = payload[offset++];
    for (uint16_t index = 0; index < count; ++index) {
      if (size - offset < 4) return false;
      const size_t fields = payload[offset + 2];
      const size_t methods = payload[offset + 3];
      const size_t entries = (fields + methods) * sizeof(uint16_t);
      offset += 4;
      if (entries > size - offset) return false;
      offset += entries;
    }
    return offset == size;
  }
  case 11: { // Compact Descriptor
    if (size < 1) return false;
    const uint8_t count = payload[offset++];
    for (uint16_t index = 0; index < count; ++index) {
      if (size - offset < 9) return false;
      const size_t interfaces = payload[offset + 4];
      const size_t fields = read_be16(payload + offset + 5);
      const size_t methods = read_be16(payload + offset + 7);
      const size_t record_size =
          9 + interfaces * 2 + fields * 7 + methods * 12;
      if (record_size > size - offset) return false;
      offset += record_size;
    }
    if (size - offset < sizeof(uint16_t)) return false;
    const size_t constant_pool_count_offset = offset;
    const size_t constant_pool_count = read_be16(payload + offset);
    offset += sizeof(uint16_t);
    if (constant_pool_count > (size - offset) / sizeof(uint16_t))
      return false;
    offset += constant_pool_count * sizeof(uint16_t);
    while (offset < size) {
      const size_t nibble_count = payload[offset++];
      const size_t descriptor_bytes = (nibble_count + 1) / 2;
      if (descriptor_bytes > size - offset ||
          !validate_type_descriptor(payload + offset, nibble_count)) {
        return false;
      }
      offset += descriptor_bytes;
    }
    if (offset != size ||
        (!validate_descriptor_type_offsets(
             payload, size, count, constant_pool_count_offset,
             constant_pool_count, constant_pool_count_offset,
             constant_pool_count_offset + 2 + constant_pool_count * 2) &&
         !validate_descriptor_type_offsets(
             payload, size, count, constant_pool_count_offset,
             constant_pool_count, constant_pool_count_offset + 2,
             constant_pool_count_offset + 2 + constant_pool_count * 2))) {
      return false;
    }
    return cap_major != 2 || cap_minor != 1 || class_payload == nullptr ||
           descriptor_class_refs_match_class_component_21(
               payload, size, class_payload, class_payload_size);
  }
  default:
    return false;
  }
}

} // namespace

/// CAP File component tag values
#define CAP_COMPONENT_HEADER (uint8_t)1
#define CAP_COMPONENT_DIRECTORY (uint8_t)2
#define CAP_COMPONENT_APPLET (uint8_t)3
#define CAP_COMPONENT_IMPORT (uint8_t)4
#define CAP_COMPONENT_CONSTANT_POOL (uint8_t)5
#define CAP_COMPONENT_CLASS (uint8_t)6
#define CAP_COMPONENT_METHOD (uint8_t)7
#define CAP_COMPONENT_STATIC_FIELD (uint8_t)8
#define CAP_COMPONENT_REFERENCE_LOCATION (uint8_t)9
#define CAP_COMPONENT_EXPORT (uint8_t)10
#define CAP_COMPONENT_DESCRIPTOR (uint8_t)11

/**
 * Default constructor
 *
 * @param[length] CAP file length
 * @param[cap_file] A pointer to the CAP file data
 */
JC_Cap::JC_Cap(uint16_t length, const uint8_t *cap_file) {
  if (cap_file == nullptr) {
    if (length != 0) {
      throw Exceptions::SecurityException;
    }
    return;
  }

  const uint8_t *component = cap_file;
  const uint8_t *cap_file_end = cap_file + length;
  uint8_t cap_major = 2;
  uint8_t cap_minor = 1;
  const uint8_t *class_payload = nullptr;
  size_t class_payload_size = 0;

  while (component < cap_file_end) {
    const size_t remaining = static_cast<size_t>(cap_file_end - component);
    constexpr size_t component_header_size = sizeof(uint8_t) + sizeof(uint16_t);
    if (remaining < component_header_size) {
      throw Exceptions::SecurityException;
    }

    // Decode the length from bytes so an unaligned CAP buffer is safe, then
    // validate the complete frame before any component view is constructed.
    const uint16_t payload_size = static_cast<uint16_t>(
        (static_cast<uint16_t>(component[1]) << 8) | component[2]);
    if (payload_size > remaining - component_header_size) {
      throw Exceptions::SecurityException;
    }
    const uint8_t *payload = component + component_header_size;
    if (!validate_component_payload(component[0], payload, payload_size,
                                    cap_major, cap_minor, class_payload,
                                    class_payload_size)) {
      throw Exceptions::SecurityException;
    }
    if (component[0] == CAP_COMPONENT_CLASS) {
      class_payload = payload;
      class_payload_size = payload_size;
    }
    if (component[0] == CAP_COMPONENT_HEADER && payload_size >= 6) {
      cap_minor = payload[4];
      cap_major = payload[5];
    }
    const uint8_t *next_component =
        component + component_header_size + payload_size;

    uint8_t component_tag = *component;

    switch (component_tag) {
    case CAP_COMPONENT_HEADER:
      if (this->header_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->header_comp =
          reinterpret_cast<const jc_cap_header_component *>(component);
      break;

    case CAP_COMPONENT_DIRECTORY:
      if (this->directory_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->directory_comp =
          reinterpret_cast<const jc_cap_directory_component *>(component);
      break;

    case CAP_COMPONENT_APPLET:
      if (this->applet_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->applet_comp =
          reinterpret_cast<const jc_cap_applet_component *>(component);
      break;

    case CAP_COMPONENT_IMPORT:
      if (this->import_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->import_comp =
          reinterpret_cast<const jc_cap_import_component *>(component);
      break;

    case CAP_COMPONENT_CONSTANT_POOL:
      if (this->constantPool_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->constantPool_comp =
          reinterpret_cast<const jc_cap_constant_pool_component *>(component);
      break;

    case CAP_COMPONENT_CLASS:
      if (this->class_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->class_comp =
          reinterpret_cast<const jc_cap_class_component *>(component);
      break;

    case CAP_COMPONENT_METHOD:
      if (this->method_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->method_comp =
          reinterpret_cast<const jc_cap_method_component *>(component);
      break;

    case CAP_COMPONENT_STATIC_FIELD:
      if (this->staticField_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->staticField_comp =
          reinterpret_cast<const jc_cap_static_field_component *>(component);
      break;

    case CAP_COMPONENT_REFERENCE_LOCATION:
      if (this->referenceLocation_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->referenceLocation_comp =
          reinterpret_cast<const jc_cap_reference_location_component *>(
              component);
      break;

    case CAP_COMPONENT_EXPORT:
      if (this->export_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->export_comp =
          reinterpret_cast<const jc_cap_export_component *>(component);
      break;

    case CAP_COMPONENT_DESCRIPTOR:
      if (this->descriptor_comp != nullptr) {
        throw Exceptions::SecurityException;
      }

      this->descriptor_comp =
          reinterpret_cast<const jc_cap_descriptor_component *>(component);
      break;

    default: // byte unknown value
      throw Exceptions::SecurityException;
    }

    component = next_component;
  }

  return;
}

/**
 * Copy assignment operator
 */
JC_Cap &JC_Cap::operator=(const JC_Cap &cap) noexcept {
  this->header_comp = cap.header_comp;
  this->directory_comp = cap.directory_comp;
  this->import_comp = cap.import_comp;
  this->applet_comp = cap.applet_comp;
  this->class_comp = cap.class_comp;
  this->method_comp = cap.method_comp;
  this->staticField_comp = cap.staticField_comp;
  this->export_comp = cap.export_comp;
  this->constantPool_comp = cap.constantPool_comp;
  this->referenceLocation_comp = cap.referenceLocation_comp;
  this->descriptor_comp = cap.descriptor_comp;

  return *this;
}

/**
 * Move assignment operator
 */
JC_Cap &JC_Cap::operator=(const JC_Cap &&cap) noexcept {
  this->header_comp = std::move(cap.header_comp);
  this->directory_comp = std::move(cap.directory_comp);
  this->import_comp = std::move(cap.import_comp);
  this->applet_comp = std::move(cap.applet_comp);
  this->class_comp = std::move(cap.class_comp);
  this->method_comp = std::move(cap.method_comp);
  this->staticField_comp = std::move(cap.staticField_comp);
  this->export_comp = std::move(cap.export_comp);
  this->constantPool_comp = std::move(cap.constantPool_comp);
  this->referenceLocation_comp = std::move(cap.referenceLocation_comp);
  this->descriptor_comp = std::move(cap.descriptor_comp);
  return *this;
}

/**
 * Equality operator
 */
bool JC_Cap::operator==(JC_Cap &cap) const noexcept {
  return (this->getHeader()->package == cap.getHeader()->package);
}

/**
 * Get Header component
 *
 * @return Header component
 */
const jc_cap_header_component *JC_Cap::getHeader() const noexcept {
  return this->header_comp;
}

/**
 * Get Directory component
 *
 * @return Directory component
 */
const jc_cap_directory_component *JC_Cap::getDirectory() const noexcept {
  return this->directory_comp;
}

/**
 * Get Import component
 *
 * @return Import component
 */
const jc_cap_import_component *JC_Cap::getImport() const noexcept {
  return this->import_comp;
}

/**
 * Get Applet component
 *
 * @return Applet component
 */
const jc_cap_applet_component *JC_Cap::getApplet() const noexcept {
  return this->applet_comp;
}

/**
 * Get Class component
 *
 * @return Class component
 */
const jc_cap_class_component *JC_Cap::getClass() const noexcept {
  return this->class_comp;
}

/**
 * Get Method component
 *
 * @return Method component
 */
const jc_cap_method_component *JC_Cap::getMethod() const noexcept {
  return this->method_comp;
}

/**
 * Get StaticField component
 *
 * @return StaticField component
 */
const jc_cap_static_field_component *JC_Cap::getStaticField() const noexcept {
  return this->staticField_comp;
}

/**
 * Get Export component
 *
 * @return Export component
 */
const jc_cap_export_component *JC_Cap::getExport() const noexcept {
  return this->export_comp;
}

/**
 * Get ConstantPool component
 *
 * @return ConstantPool component
 */
const jc_cap_constant_pool_component *JC_Cap::getConstantPool() const noexcept {
  return this->constantPool_comp;
}

/**
 * Get ReferenceLocation component
 *
 * @return ReferenceLocation component
 */
const jc_cap_reference_location_component *JC_Cap::getReferenceLocation() const
    noexcept {
  return this->referenceLocation_comp;
}

/**
 * Get Descriptor component
 *
 * @return Descriptor component
 */
const jc_cap_descriptor_component *JC_Cap::getDescriptor() const noexcept {
  return this->descriptor_comp;
}

} // namespace jcvm
