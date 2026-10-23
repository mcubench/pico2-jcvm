#include "jc_cap/jc_cap_class.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "usage: rmi_array_cap_inspect Class.cap\n";
    return 2;
  }
  std::ifstream input(argv[1], std::ios::binary);
  const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input),
                                   std::istreambuf_iterator<char>()};
  if (!input.is_open() ||
      bytes.size() < sizeof(jcvm::jc_cap_class_component)) {
    std::cerr << "invalid Class.cap input\n";
    return 1;
  }
  const auto *component =
      reinterpret_cast<const jcvm::jc_cap_class_component *>(bytes.data());
  if (!component->hasValidSignaturePool()) {
    std::cerr << "Class.cap has an invalid signature pool\n";
    return 1;
  }
  const auto class_bytes = component->claz();
  uint16_t offset = component->classInfoStartOffset();
  if (offset >= class_bytes.size()) {
    std::cerr << "Class.cap has no remote interface metadata\n";
    return 1;
  }
  const auto *interface_info =
      reinterpret_cast<const jcvm::jc_cap_interface_info *>(
          class_bytes.data() + offset);
  offset = static_cast<uint16_t>(offset + interface_info->getSize());
  bool found = false;
  while (offset < class_bytes.size()) {
    const auto *class_info = reinterpret_cast<const jcvm::jc_cap_class_info *>(
        class_bytes.data() + offset);
    const uint16_t size = class_info->getSize();
    if (size == 0 || static_cast<uint32_t>(offset) + size > class_bytes.size()) {
      break;
    }
    if ((class_info->flags & 0x02) != 0) {
      const auto *remote = class_info->remoteInterfaceInfo();
      for (uint8_t index = 0; index < remote->remote_methods_count; ++index) {
        const auto &method = remote->remote_methods[index];
        const uint16_t signature_offset = method.getSignatureOffset();
        const uint8_t *signature =
            component->signaturePool() + signature_offset;
        std::cout << "RMI_METHOD hash=" << std::hex << method.getMethodHash()
                  << " signature_offset=" << signature_offset << " types=";
        const uint8_t nibble_count = signature[0];
        for (uint8_t nibble = 0; nibble < nibble_count; ++nibble) {
          const uint8_t packed = signature[1 + nibble / 2];
          std::cout << static_cast<unsigned>((nibble & 1) ? packed & 0x0f
                                                          : packed >> 4);
        }
        std::cout << std::dec << '\n';
      }
      found = true;
    }
    offset = static_cast<uint16_t>(offset + size);
  }
  return found ? 0 : 1;
}
