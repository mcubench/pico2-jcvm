#ifndef JCVM_CAP_CONSTANT_POOL_VALIDATION_HPP
#define JCVM_CAP_CONSTANT_POOL_VALIDATION_HPP

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

inline bool valid_instance_field_token(const std::size_t token,
                                      const std::size_t declared_size,
                                      const std::size_t first_reference_token,
                                      const std::size_t reference_count) noexcept {
  return token < declared_size && first_reference_token <= declared_size &&
         reference_count <= declared_size - first_reference_token;
}

inline bool valid_internal_member_offsets(const uint8_t *entries,
                                          const std::size_t count,
                                          const std::size_t static_image_size,
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
        (tag == 6 && offset >= method_size)) {
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
