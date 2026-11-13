#ifndef JCVM_CAP_CONSTANT_POOL_VALIDATION_HPP
#define JCVM_CAP_CONSTANT_POOL_VALIDATION_HPP

#include <array>
#include <cstddef>
#include <cstdint>

namespace jcvm::cap_constant_pool_validation {

// CAP ConstantPool entries have a one-byte tag and a three-byte payload.
// The component validator calls this before creating the packed runtime view,
// so malformed tags cannot reach the interpreter's tag-specific unions.
inline bool valid_payload(const uint8_t *payload,
                          const std::size_t size) noexcept {
  if (payload == nullptr || size < 2) return false;

  const std::size_t count =
      (static_cast<std::size_t>(payload[0]) << 8) | payload[1];
  if (count != (size - 2) / 4 || (size - 2) % 4 != 0) return false;

  for (std::size_t index = 0; index < count; ++index) {
    const uint8_t tag = payload[2 + index * 4];
    if (tag < 1 || tag > 6) return false;
  }
  return true;
}

// In all six constant-pool records, an external reference stores its package
// token in the first byte of the three-byte payload and sets bit 7. For tags
// 1–4 that byte is the class-ref discriminator; for tags 5–6 it is the
// static-reference discriminator. The low seven bits index Import.packages.
inline bool valid_import_tokens(const uint8_t *entries,
                                const std::size_t count,
                                const std::size_t import_count) noexcept {
  if (entries == nullptr && count != 0) return false;
  for (std::size_t index = 0; index < count; ++index) {
    const uint8_t *entry = entries + index * 4;
    if (entry[0] < 1 || entry[0] > 6) return false;
    const uint8_t package_token = entry[1];
    if ((package_token & 0x80u) != 0 &&
        (package_token & 0x7fu) >= import_count) {
      return false;
    }
  }
  return true;
}

inline bool valid_export_class_token(const std::size_t token,
                                     const std::size_t export_count) noexcept {
  return token < export_count;
}

inline bool valid_export_member_token(const std::size_t token,
                                      const std::size_t member_count) noexcept {
  return token < member_count;
}

// Export.class_offset addresses a class_info/interface_info record in the
// Class component. `class_record_starts` is built while parsing that complete
// component. The record flags use the CAP Class-component high nibble.
inline bool valid_export_class_offsets(
    const uint8_t *payload, const std::size_t payload_size,
    const uint8_t *class_record_starts,
    const uint8_t *class_component_payload,
    const std::size_t class_component_size,
    const bool applet_package, const uint8_t *descriptor_payload = nullptr,
    const std::size_t descriptor_size = 0) noexcept {
  if (payload == nullptr || payload_size <= 1 ||
      class_component_payload == nullptr || class_component_size == 0) {
    return false;
  }
  const std::size_t class_count = payload[0];
  if (class_count == 0 || class_record_starts == nullptr) return false;

  std::size_t offset = 1;
  for (std::size_t index = 0; index < class_count; ++index) {
    if (offset > payload_size || payload_size - offset < 4) return false;
    const std::size_t class_offset =
        (static_cast<std::size_t>(payload[offset]) << 8) | payload[offset + 1];
    std::size_t previous = 1;
    for (std::size_t prior_index = 0; prior_index < index; ++prior_index) {
      const std::size_t previous_class_offset =
          (static_cast<std::size_t>(payload[previous]) << 8) |
          payload[previous + 1];
      if (previous_class_offset == class_offset) return false;
      previous += 4 +
                  (static_cast<std::size_t>(payload[previous + 2]) +
                   payload[previous + 3]) *
                      sizeof(uint16_t);
    }
    if (class_offset >= class_component_size ||
        (class_record_starts[class_offset / 8] &
         static_cast<uint8_t>(1u << (class_offset & 7))) == 0) {
      return false;
    }

    const std::size_t fields = payload[offset + 2];
    const std::size_t methods = payload[offset + 3];
    const uint8_t class_flags = class_component_payload[class_offset] >> 4;
    const bool is_interface = (class_flags & 0x08) != 0;
    const bool is_shareable = (class_flags & 0x04) != 0;
    if ((is_interface && (fields != 0 || methods != 0)) ||
        (applet_package && (!is_interface || !is_shareable))) {
      return false;
    }
    offset += 4;
    const std::size_t entries = (fields + methods) * sizeof(uint16_t);
    if (entries > payload_size - offset) return false;
    offset += entries;
  }
  if (offset != payload_size) return false;
  if (descriptor_payload == nullptr) return descriptor_size == 0;
  if (descriptor_size == 0) return false;
  if (applet_package) return true;

  // Library packages export every public class and interface. Descriptor
  // records identify each class token and its Class-component offset, so
  // require a complete token-to-record mapping when that metadata is present.
  std::array<uint16_t, 256> exported_class_offsets{};
  exported_class_offsets.fill(UINT16_MAX);
  std::size_t export_cursor = 1;
  for (std::size_t token = 0; token < class_count; ++token) {
    if (export_cursor > payload_size || payload_size - export_cursor < 4) {
      return false;
    }
    exported_class_offsets[token] = static_cast<uint16_t>(
        (static_cast<uint16_t>(payload[export_cursor]) << 8) |
        payload[export_cursor + 1]);
    const std::size_t fields = payload[export_cursor + 2];
    const std::size_t methods = payload[export_cursor + 3];
    export_cursor += 4 + (fields + methods) * sizeof(uint16_t);
  }

  const auto read_be16 = [](const uint8_t *bytes) {
    return static_cast<std::size_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
  };
  std::array<uint8_t, 256> seen_tokens{};
  std::size_t public_class_count = 0;
  std::size_t descriptor_cursor = 1;
  const std::size_t descriptor_class_count = descriptor_payload[0];
  for (std::size_t index = 0; index < descriptor_class_count; ++index) {
    if (descriptor_cursor > descriptor_size ||
        descriptor_size - descriptor_cursor < 9) {
      return false;
    }
    const std::size_t token = descriptor_payload[descriptor_cursor];
    const uint8_t access_flags = descriptor_payload[descriptor_cursor + 1];
    const std::size_t class_offset =
        read_be16(descriptor_payload + descriptor_cursor + 2);
    const std::size_t interfaces = descriptor_payload[descriptor_cursor + 4];
    const std::size_t fields =
        read_be16(descriptor_payload + descriptor_cursor + 5);
    const std::size_t methods =
        read_be16(descriptor_payload + descriptor_cursor + 7);
    const std::size_t method_start =
        descriptor_cursor + 9 + interfaces * 2 + fields * 7;
    if (method_start > descriptor_size ||
        methods > (descriptor_size - method_start) / 12) {
      return false;
    }
    if ((access_flags & 0x01u) != 0) {
      if ((class_offset & 0x8000u) != 0 ||
          class_offset >= class_component_size ||
          (class_record_starts[class_offset / 8] &
           static_cast<uint8_t>(1u << (class_offset & 7))) == 0 ||
          (((access_flags & 0x40u) != 0) !=
           ((class_component_payload[class_offset] & 0x80u) != 0)) ||
          token >= class_count || seen_tokens[token] != 0 ||
          exported_class_offsets[token] != class_offset) {
        return false;
      }
      seen_tokens[token] = 1;
      ++public_class_count;
    }
    descriptor_cursor = method_start + methods * 12;
  }
  return descriptor_cursor <= descriptor_size &&
         public_class_count == class_count;
}

inline bool valid_instance_field_token(const std::size_t token,
                                      const std::size_t declared_size,
                                      const std::size_t first_reference_token,
                                      const std::size_t reference_count) noexcept {
  if (token >= declared_size) return false;
  // CAP encodes 0xFF when the class has no reference fields. This is valid
  // even when the class has primitive instance fields; do not interpret the
  // sentinel as a layout index.
  if (reference_count == 0) {
    return first_reference_token == 0xff ||
           first_reference_token <= declared_size;
  }
  return first_reference_token <= declared_size &&
         reference_count <= declared_size - first_reference_token;
}

inline bool valid_internal_member_offsets(const uint8_t *entries,
                                          const std::size_t count,
                                          const std::size_t static_image_size,
                                          const std::size_t method_offset_base,
                                          const std::size_t method_size) noexcept {
  if (entries == nullptr && count != 0) return false;
  for (std::size_t index = 0; index < count; ++index) {
    const uint8_t *entry = entries + index * 4;
    const uint8_t tag = entry[0];
    if (tag != 5 && tag != 6) continue;
    if ((entry[1] & 0x80u) != 0) continue;
    const std::size_t offset =
        (static_cast<std::size_t>(entry[2]) << 8) | entry[3];
    if ((tag == 5 && offset >= static_image_size) ||
        (tag == 6 &&
         (offset <= method_offset_base ||
          offset - method_offset_base > method_size))) {
      return false;
    }
  }
  return true;
}

inline bool valid_internal_class_token(const std::size_t token,
                                      const std::size_t component_size,
                                      const bool record_start,
                                      const bool is_interface,
                                      const bool requires_class) noexcept {
  return token < component_size && record_start &&
         (!requires_class || !is_interface);
}

inline bool valid_descriptor_static_method_offsets(
    const uint8_t *entries, const std::size_t count,
    const uint8_t *descriptor_payload,
    const std::size_t descriptor_size) noexcept {
  if ((entries == nullptr && count != 0) || descriptor_payload == nullptr ||
      descriptor_size == 0) {
    return false;
  }
  const auto read_be16 = [](const uint8_t *bytes) {
    return static_cast<std::size_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
  };

  for (std::size_t entry_index = 0; entry_index < count; ++entry_index) {
    const uint8_t *entry = entries + entry_index * 4;
    if (entry[0] != 6 || (entry[1] & 0x80u) != 0) continue;
    const std::size_t target = read_be16(entry + 2);
    std::size_t matches = 0;
    std::size_t offset = 1;
    const std::size_t class_count = descriptor_payload[0];
    for (std::size_t class_index = 0; class_index < class_count;
         ++class_index) {
      if (offset > descriptor_size || descriptor_size - offset < 9) {
        return false;
      }
      const std::size_t interfaces = descriptor_payload[offset + 4];
      const std::size_t fields = read_be16(descriptor_payload + offset + 5);
      const std::size_t methods = read_be16(descriptor_payload + offset + 7);
      const std::size_t method_start = offset + 9 + interfaces * 2 + fields * 7;
      if (method_start > descriptor_size ||
          methods > (descriptor_size - method_start) / 12) {
        return false;
      }
      for (std::size_t method_index = 0; method_index < methods;
           ++method_index) {
        if (read_be16(descriptor_payload + method_start + method_index * 12 +
                      2) == target) {
          ++matches;
        }
      }
      offset = method_start + methods * 12;
    }
    if (matches != 1) return false;
  }
  return true;
}

inline bool descriptor_method_offsets(
    const uint8_t *descriptor_payload, const std::size_t descriptor_size,
    std::array<uint8_t, 1u << 13> &offsets) noexcept {
  if (descriptor_payload == nullptr || descriptor_size == 0) return false;
  offsets.fill(0);
  const auto read_be16 = [](const uint8_t *bytes) {
    return static_cast<std::size_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
  };

  std::size_t offset = 1;
  const std::size_t class_count = descriptor_payload[0];
  for (std::size_t class_index = 0; class_index < class_count;
       ++class_index) {
    if (offset > descriptor_size || descriptor_size - offset < 9)
      return false;
    const std::size_t interfaces = descriptor_payload[offset + 4];
    const std::size_t fields = read_be16(descriptor_payload + offset + 5);
    const std::size_t methods = read_be16(descriptor_payload + offset + 7);
    const std::size_t method_start = offset + 9 + interfaces * 2 + fields * 7;
    if (method_start > descriptor_size ||
        methods > (descriptor_size - method_start) / 12) {
      return false;
    }
    for (std::size_t method_index = 0; method_index < methods;
         ++method_index) {
      const std::size_t method_offset = read_be16(
          descriptor_payload + method_start + method_index * 12 + 2);
      offsets[method_offset / 8] |=
          static_cast<uint8_t>(1u << (method_offset & 7));
    }
    offset = method_start + methods * 12;
  }
  // The class records are followed by ConstantPool type indexes and encoded
  // type descriptors, so the class-record cursor does not consume the whole
  // Descriptor payload.
  return offset <= descriptor_size;
}

inline bool descriptor_export_method_records(
    const uint8_t *descriptor_payload, const std::size_t descriptor_size,
    const std::size_t class_token, const std::size_t class_offset,
    const uint8_t *&method_records,
    std::size_t &method_count,
    uint8_t &class_access_flags) noexcept {
  if (descriptor_payload == nullptr || descriptor_size == 0) return false;
  const auto read_be16 = [](const uint8_t *bytes) {
    return static_cast<std::size_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
  };

  method_records = nullptr;
  method_count = 0;
  class_access_flags = 0;
  std::size_t matches = 0;
  std::size_t offset = 1;
  const std::size_t descriptor_class_count = descriptor_payload[0];
  for (std::size_t index = 0; index < descriptor_class_count; ++index) {
    if (offset > descriptor_size || descriptor_size - offset < 9) {
      return false;
    }
    const std::size_t token = descriptor_payload[offset];
    const std::size_t reference = read_be16(descriptor_payload + offset + 2);
    const std::size_t interfaces = descriptor_payload[offset + 4];
    const std::size_t fields = read_be16(descriptor_payload + offset + 5);
    const std::size_t methods = read_be16(descriptor_payload + offset + 7);
    const std::size_t method_start = offset + 9 + interfaces * 2 + fields * 7;
    if (method_start > descriptor_size ||
        methods > (descriptor_size - method_start) / 12) {
      return false;
    }
    if (token == class_token) {
      if ((reference & 0x8000u) != 0 || reference != class_offset) {
        return false;
      }
      ++matches;
      method_records = descriptor_payload + method_start;
      method_count = methods;
      class_access_flags = descriptor_payload[offset + 1];
    }
    offset = method_start + methods * 12;
  }
  return offset <= descriptor_size && matches == 1;
}

inline bool valid_export_methods_with_descriptor(
    const uint8_t *export_payload, const std::size_t export_payload_size,
    const uint8_t *descriptor_payload,
    const std::size_t descriptor_size) noexcept {
  if (export_payload == nullptr || export_payload_size == 0 ||
      descriptor_payload == nullptr || descriptor_size == 0) {
    return false;
  }
  const auto read_be16 = [](const uint8_t *bytes) {
    return static_cast<std::size_t>(
        (static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
  };

  std::size_t export_offset = 1;
  const std::size_t class_count = export_payload[0];
  for (std::size_t class_token = 0; class_token < class_count; ++class_token) {
    if (export_offset > export_payload_size ||
        export_payload_size - export_offset < 4) {
      return false;
    }
    const std::size_t class_offset = read_be16(export_payload + export_offset);
    const std::size_t field_count = export_payload[export_offset + 2];
    const std::size_t method_count = export_payload[export_offset + 3];
    const std::size_t field_bytes = field_count * 2;
    if (field_bytes > export_payload_size - export_offset - 4) return false;
    export_offset += 4 + field_bytes;
    const std::size_t method_bytes = method_count * 2;
    if (method_bytes > export_payload_size - export_offset) return false;
    const uint8_t *exported_methods = export_payload + export_offset;

    const uint8_t *descriptor_methods = nullptr;
    std::size_t descriptor_method_count = 0;
    uint8_t class_access_flags = 0;
    if (!descriptor_export_method_records(
            descriptor_payload, descriptor_size, class_token, class_offset,
            descriptor_methods, descriptor_method_count,
            class_access_flags) ||
        (class_access_flags & 0x01u) == 0) {
      return false;
    }
    std::array<uint8_t, 256> seen_tokens{};
    std::size_t exported_descriptor_methods = 0;
    for (std::size_t index = 0; index < descriptor_method_count; ++index) {
      const uint8_t *method = descriptor_methods + index * 12;
      const std::size_t token = method[0];
      const uint8_t flags = method[1];
      const bool visible = (flags & (0x01u | 0x04u)) != 0;
      const bool exportable = (flags & (0x08u | 0x80u)) != 0;
      if (!visible || !exportable) continue;
      if (token >= method_count || seen_tokens[token] != 0) return false;
      seen_tokens[token] = 1;
      const std::size_t target = read_be16(method + 2);
      if (read_be16(exported_methods + token * 2) != target) return false;
      ++exported_descriptor_methods;
    }
    if (exported_descriptor_methods != method_count) return false;
    export_offset += method_bytes;
  }
  return export_offset == export_payload_size;
}

// Validate every offset in an Export component, including entries not
// currently referenced by this package's ConstantPool. Without Descriptor,
// method offsets can only be checked for component bounds and a complete
// method header; Descriptor provides the exact method-start relation.
inline bool valid_export_offsets(
    const uint8_t *payload, const std::size_t payload_size,
    const std::size_t static_image_size,
    const std::size_t method_offset_base, const uint8_t *methods,
    const std::size_t method_size, const uint8_t *descriptor_payload = nullptr,
    const std::size_t descriptor_size = 0) noexcept {
  if (payload == nullptr || payload_size == 0 ||
      (methods == nullptr && method_size != 0)) {
    return false;
  }

  static std::array<uint8_t, 1u << 13> descriptor_methods{};
  static std::array<uint8_t, 1u << 13> exported_field_offsets{};
  static std::array<uint8_t, 1u << 13> exported_method_offsets{};
  exported_field_offsets.fill(0);
  exported_method_offsets.fill(0);
  if (descriptor_payload != nullptr &&
      !descriptor_method_offsets(descriptor_payload, descriptor_size,
                                 descriptor_methods)) {
    return false;
  }
  if (descriptor_payload != nullptr &&
      !valid_export_methods_with_descriptor(
          payload, payload_size, descriptor_payload, descriptor_size)) {
    return false;
  }

  const std::size_t class_count = payload[0];
  std::size_t offset = 1;
  for (std::size_t class_index = 0; class_index < class_count;
       ++class_index) {
    if (offset > payload_size || payload_size - offset < 4) return false;
    const std::size_t field_count = payload[offset + 2];
    const std::size_t method_count = payload[offset + 3];
    offset += 4;
    const std::size_t field_bytes = field_count * 2;
    const std::size_t method_bytes = method_count * 2;
    if (field_bytes > payload_size - offset) return false;
    for (std::size_t field_index = 0; field_index < field_count;
         ++field_index) {
      const std::size_t field_offset =
          (static_cast<std::size_t>(payload[offset + field_index * 2]) << 8) |
          payload[offset + field_index * 2 + 1];
      if (field_offset >= static_image_size ||
          (exported_field_offsets[field_offset / 8] &
           static_cast<uint8_t>(1u << (field_offset & 7))) != 0) {
        return false;
      }
      exported_field_offsets[field_offset / 8] |=
          static_cast<uint8_t>(1u << (field_offset & 7));
    }
    offset += field_bytes;
    if (method_bytes > payload_size - offset) return false;
    for (std::size_t method_index = 0; method_index < method_count;
         ++method_index) {
      const std::size_t method_offset =
          (static_cast<std::size_t>(payload[offset + method_index * 2]) << 8) |
          payload[offset + method_index * 2 + 1];
      if (method_offset <= method_offset_base ||
          (exported_method_offsets[method_offset / 8] &
           static_cast<uint8_t>(1u << (method_offset & 7))) != 0) {
        return false;
      }
      exported_method_offsets[method_offset / 8] |=
          static_cast<uint8_t>(1u << (method_offset & 7));
      const std::size_t method_index_in_stream =
          method_offset - method_offset_base - 1;
      if (method_index_in_stream >= method_size) return false;
      const std::size_t header_size =
          (methods[method_index_in_stream] & 0x80u) != 0 ? 4u : 2u;
      if (header_size > method_size - method_index_in_stream) return false;
      if (descriptor_payload != nullptr &&
          (descriptor_methods[method_offset / 8] &
           static_cast<uint8_t>(1u << (method_offset & 7))) == 0) {
        return false;
      }
    }
    offset += method_bytes;
  }
  return offset == payload_size;
}

template <typename ResolveExportToken>
bool validate_external_class_tokens(const uint8_t *entries,
                                    const std::size_t count,
                                    ResolveExportToken resolve) {
  if (entries == nullptr && count != 0) return false;
  for (std::size_t index = 0; index < count; ++index) {
    const uint8_t *entry = entries + index * 4;
    if (entry[0] < 1 || entry[0] > 6) return false;
    if ((entry[1] & 0x80u) != 0 &&
        !resolve(entry[0], entry[1] & 0x7fu, entry[2], entry[3])) {
      return false;
    }
  }
  return true;
}

} // namespace jcvm::cap_constant_pool_validation

#endif // JCVM_CAP_CONSTANT_POOL_VALIDATION_HPP
