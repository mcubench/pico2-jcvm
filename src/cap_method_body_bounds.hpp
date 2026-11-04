#ifndef CHOUPI_CAP_METHOD_BODY_BOUNDS_HPP
#define CHOUPI_CAP_METHOD_BODY_BOUNDS_HPP

#include <cstddef>
#include <cstdint>

namespace jcvm::cap_method_body_bounds {

struct Range {
  std::size_t begin;
  std::size_t end;
};

inline uint16_t read_be16(const uint8_t *bytes) noexcept {
  return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                               static_cast<uint16_t>(bytes[1]));
}

// Descriptor payload starts at its class_count byte. Method offsets use CAP
// Method-component coordinates (handler_count + handlers + method stream).
inline bool find(const uint8_t *descriptor_payload,
                 const std::size_t descriptor_size,
                 const uint8_t *methods, const std::size_t methods_size,
                 const std::size_t handler_table_bytes,
                 const uint16_t method_component_offset,
                 Range &range) noexcept {
  if (descriptor_payload == nullptr || methods == nullptr ||
      descriptor_size == 0 || method_component_offset <= handler_table_bytes) {
    return false;
  }

  const std::size_t method_index =
      static_cast<std::size_t>(method_component_offset) -
      handler_table_bytes - 1;
  if (method_index >= methods_size) return false;

  const std::size_t class_count = descriptor_payload[0];
  std::size_t offset = 1;
  bool found = false;
  Range candidate{};
  std::size_t next_method_index = methods_size;

  for (std::size_t class_index = 0; class_index < class_count;
       ++class_index) {
    if (offset > descriptor_size || descriptor_size - offset < 9) {
      return false;
    }
    const std::size_t interfaces = descriptor_payload[offset + 4];
    const std::size_t fields = read_be16(descriptor_payload + offset + 5);
    const std::size_t method_count =
        read_be16(descriptor_payload + offset + 7);
    const std::size_t interface_bytes = interfaces * 2;
    const std::size_t field_bytes = fields * 7;
    const std::size_t method_bytes = method_count * 12;
    if (interface_bytes > descriptor_size - offset - 9 ||
        field_bytes > descriptor_size - offset - 9 - interface_bytes ||
        method_bytes >
            descriptor_size - offset - 9 - interface_bytes - field_bytes) {
      return false;
    }

    const std::size_t method_records =
        offset + 9 + interface_bytes + field_bytes;
    for (std::size_t method_index_in_class = 0;
         method_index_in_class < method_count; ++method_index_in_class) {
      const uint8_t *record =
          descriptor_payload + method_records + method_index_in_class * 12;
      const uint16_t record_method_offset = read_be16(record + 2);
      if (record_method_offset <= handler_table_bytes) continue;
      const std::size_t record_method_index =
          static_cast<std::size_t>(record_method_offset) -
          handler_table_bytes - 1;
      if (record_method_index >= methods_size) return false;
      if (record_method_index > method_index &&
          record_method_index < next_method_index) {
        next_method_index = record_method_index;
      }
      if (record_method_offset != method_component_offset) continue;
      if (found) return false;

      const std::size_t header_index =
          static_cast<std::size_t>(method_component_offset) -
          handler_table_bytes - 1;
      const std::size_t header_size =
          (methods[header_index] & 0x80u) != 0 ? 5u : 2u;
      if (header_size > methods_size - header_index) return false;
      const std::size_t body_begin = header_index + header_size;
      const std::size_t bytecode_count = read_be16(record + 6);
      if (bytecode_count == 0 || bytecode_count > methods_size - body_begin) {
        return false;
      }
      candidate = {body_begin, body_begin + bytecode_count};
      found = true;
    }

    offset = method_records + method_bytes;
  }

  if (!found) return false;
  if (candidate.end > next_method_index) {
    if (candidate.begin >= next_method_index) return false;
    candidate.end = next_method_index;
  }
  range = candidate;
  return true;
}

} // namespace jcvm::cap_method_body_bounds

#endif // CHOUPI_CAP_METHOD_BODY_BOUNDS_HPP
