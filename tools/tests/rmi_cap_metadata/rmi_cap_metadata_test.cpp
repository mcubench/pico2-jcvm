#include "jc_cap/jc_cap_class.hpp"
#include "jc_cap/jc_cap_header.hpp"
#include "jc_handlers/jc_remote_interface_validation.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>

int main() {
  constexpr uint8_t emitted_header_bytes[] = {
      0x01, 0x00, 0x1e, 0xde, 0xca, 0xff, 0xed, 0x02, 0x02, 0x04, 0x00,
      0x01, 0x08, 0xa0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x01, 0x0b,
      'r',  'm',  'i',  '/',  'f',  'i',  'x',  't',  'u',  'r',  'e'};
  const auto *emitted_header =
      reinterpret_cast<const jcvm::jc_cap_header_component *>(
          emitted_header_bytes);
  assert(emitted_header->minor_version == 2);
  assert(emitted_header->packageNameLength() == 11);
  assert(std::memcmp(emitted_header->packageName(), "rmi/fixture", 11) == 0);

  // Header.cap emitted by the Java Card 3.0.5 converter for the
  // remote-enabled rmi.fixture test package (CAP format 2.2).
  constexpr uint8_t package_aid[] = {
      0xa0, 0x00, 0x00, 0x01, 0x51, 0x00, 0x00, 0x01};
  constexpr uint8_t package_name[] = {
      'r', 'm', 'i', '/', 'f', 'i', 'x', 't', 'u', 'r', 'e'};
  constexpr uint16_t header_payload_size = static_cast<uint16_t>(
      sizeof(uint32_t) + 3 + offsetof(jcvm::jc_cap_package_info, AID) +
      sizeof(package_aid) + 1 + sizeof(package_name));
  std::array<uint8_t, sizeof(jcvm::jc_cap_header_component) +
                          sizeof(package_aid) + 1 + sizeof(package_name)>
      header_bytes{};
  auto *header = reinterpret_cast<jcvm::jc_cap_header_component *>(
      header_bytes.data());
  header->tag = 1;
  header->size = HTONS(header_payload_size);
  const uint8_t magic[] = {0xde, 0xca, 0xff, 0xed};
  std::memcpy(&header->magic, magic, sizeof(magic));
  header->minor_version = 2;
  header->major_version = 2;
  header->flags = 0x04;
  header->package.minor_version = 0;
  header->package.major_version = 1;
  header->package.AID_length = sizeof(package_aid);
  std::memcpy(header->package.AID, package_aid, sizeof(package_aid));
  auto *name_length = const_cast<uint8_t *>(header->packageNameLengthAddress());
  assert(name_length != nullptr);
  *name_length = sizeof(package_name);
  std::memcpy(name_length + 1, package_name, sizeof(package_name));
  assert(header->packageNameLength() == sizeof(package_name));
  assert(header->packageName() != nullptr);
  assert(std::memcmp(header->packageName(), package_name,
                     sizeof(package_name)) == 0);

  header->minor_version = 1;
  assert(header->packageNameLengthAddress() == nullptr);
  assert(header->packageNameLength() == 0);

  header->minor_version = 2;
  header->size = HTONS(static_cast<uint16_t>(header_payload_size - 1));
  assert(header->packageNameLengthAddress() != nullptr);
  assert(header->packageNameLength() == 0);
  assert(header->packageName() == nullptr);

  header->size = HTONS(header_payload_size);
  *name_length = static_cast<uint8_t>(sizeof(package_name) + 1);
  assert(header->packageNameLength() == 0);
  assert(header->packageName() == nullptr);

  // Class.cap component emitted by the same JCDK 3.0.5 converter fixture.
  // It contains a CAP 2.2 signature pool, a remote interface, and a remote
  // class implementing that interface.
  constexpr uint8_t class_component_bytes[] = {
      0x06, 0x00, 0x4e, 0x00, 0x04, 0x01, 0x40, 0x02, 0x44, 0xa1, 0x81,
      0x00, 0x07, 'C',  'o',  'u',  'n',  't',  'e',  'r',  0x22, 0x82,
      0x05, 0x01, 0xff, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x08, 0x00,
      0x15, 0x81, 0x00, 0x00, 0x00, 0x06, 0x02, 0x01, 0x02, 0x02, 0x26,
      0x61,
      0x00, 0x00, 0x02, 0x7c, 0x38, 0x00, 0x02, 0x01, 0x00, 0x0b, 'C',
      'o',  'u',  'n',  't',  'e',  'r',  'I',  'm',  'p',  'l',  0x01,
      0x00, 0x06, 0x00, 0x83, 0x03, 0x01, 0x00, 0x01, 0x07, 0x01, 0x00,
      0x00, 0x00, 0x51};
  const auto *class_component =
      reinterpret_cast<const jcvm::jc_cap_class_component *>(
          class_component_bytes);
  assert(class_component->signaturePoolLength() == 4);
  constexpr uint8_t expected_signatures[] = {0x01, 0x40, 0x02, 0x44};
  assert(std::memcmp(class_component->signaturePool(), expected_signatures,
                     sizeof(expected_signatures)) == 0);
  assert(class_component->classInfoStartOffset() == 6);

  const auto *remote_interface =
      reinterpret_cast<const jcvm::jc_cap_interface_info *>(
          class_component->infos + class_component->classInfoStartOffset());
  assert((remote_interface->flags & 0x02) != 0);
  assert(remote_interface->interface_count == 1);
  assert(remote_interface->getSize() == 11);
  assert(remote_interface->interfaceNameLength() == 7);
  assert(std::memcmp(remote_interface->interfaceName(), "Counter", 7) == 0);

  const auto *remote_class = reinterpret_cast<const jcvm::jc_cap_class_info *>(
      class_component->infos + class_component->classInfoStartOffset() +
      remote_interface->getSize());
  assert(remote_class->interface_count == 2);
  assert((remote_class->flags & 0x02) != 0);
  assert(remote_class->getSize() == 49);
  const auto *emitted_remote_info = remote_class->remoteInterfaceInfo();
  assert(emitted_remote_info != nullptr);
  assert(emitted_remote_info->remote_methods_count == 2);
  assert(emitted_remote_info->remote_methods[0].getMethodHash() == 0x2661);
  assert(emitted_remote_info->remote_methods[0].getSignatureOffset() == 0);
  assert(emitted_remote_info->remote_methods[0].virtual_method_token == 2);
  assert(emitted_remote_info->remote_methods[1].getMethodHash() == 0x7c38);
  assert(emitted_remote_info->remote_methods[1].getSignatureOffset() == 2);
  assert(emitted_remote_info->remote_methods[1].virtual_method_token == 1);
  assert(emitted_remote_info->classNameLength() == 11);
  assert(std::memcmp(emitted_remote_info->className(), "CounterImpl", 11) == 0);
  assert(emitted_remote_info->remoteInterfacesCount() == 1);
  assert(NTOHS(emitted_remote_info->remoteInterfaces()[0].internal_classref) ==
         6);

  uint8_t remote_reference[64]{};
  const uint16_t remote_reference_length =
      jcvm::encodeRemoteReferenceWithClass(
          0x1234, reinterpret_cast<const uint8_t *>("rmi/fixture"), 11,
          *emitted_remote_info, remote_reference, sizeof(remote_reference));
  constexpr uint8_t expected_remote_reference[] = {
      0x12, 0x34, 0x00, 0x0b, 'r', 'm', 'i', '/', 'f', 'i',
      'x',  't',  'u',  'r',  'e', 0x0b, 'C', 'o', 'u', 'n',
      't',  'e',  'r',  'I',  'm',  'p', 'l'};
  assert(remote_reference_length == sizeof(expected_remote_reference));
  assert(std::memcmp(remote_reference, expected_remote_reference,
                     sizeof(expected_remote_reference)) == 0);
  assert(jcvm::encodeRemoteReferenceWithClass(
             0xffff, reinterpret_cast<const uint8_t *>("rmi/fixture"), 11,
             *emitted_remote_info, remote_reference, sizeof(remote_reference)) ==
         0);
  assert(jcvm::encodeRemoteReferenceWithClass(
             0x1234, reinterpret_cast<const uint8_t *>("rmi/fixture"), 11,
             *emitted_remote_info, remote_reference,
             remote_reference_length - 1) == 0);

  const jcvm::jc_cap_remote_interface_name interface_name{
      reinterpret_cast<const uint8_t *>("rmi/fixture"), 11,
      reinterpret_cast<const uint8_t *>("Counter"), 7};
  const uint16_t interface_reference_length =
      jcvm::encodeRemoteReferenceWithInterfaces(
          0x1234, nullptr, 0, &interface_name, 1, remote_reference,
          sizeof(remote_reference));
  constexpr uint8_t expected_interface_reference[] = {
      0x12, 0x34, 0x00, 0x01, 0x0b, 'r', 'm', 'i', '/', 'f', 'i',
      'x',  't',  'u',  'r',  'e', 0x07, 'C', 'o', 'u', 'n', 't',
      'e',  'r'};
  assert(interface_reference_length == sizeof(expected_interface_reference));
  assert(std::memcmp(remote_reference, expected_interface_reference,
                     sizeof(expected_interface_reference)) == 0);
  const jcvm::jc_cap_remote_interface_name invalid_first_interface{
      nullptr, 0, reinterpret_cast<const uint8_t *>("Counter"), 7};
  assert(jcvm::encodeRemoteReferenceWithInterfaces(
             0x1234, nullptr, 0, &invalid_first_interface, 1,
             remote_reference, sizeof(remote_reference)) == 0);

  constexpr uint8_t malformed_class_component[] = {0x06, 0x00, 0x03,
                                                    0x00, 0x02, 0xaa};
  const auto *malformed_component =
      reinterpret_cast<const jcvm::jc_cap_class_component *>(
          malformed_class_component);
  assert(!malformed_component->hasValidSignaturePool());
  assert(malformed_component->signaturePool() == nullptr);
  assert(malformed_component->classInfoStartOffset() == 0);

  constexpr size_t remote_info_size = 16;
  std::array<uint8_t, sizeof(jcvm::jc_cap_class_info) + remote_info_size> bytes{};
  auto *class_info =
      reinterpret_cast<jcvm::jc_cap_class_info *>(bytes.data());
  class_info->flags = 0x02;

  auto *remote_info = reinterpret_cast<uint8_t *>(class_info->data);
  remote_info[0] = 1;       // remote_methods_count
  remote_info[1] = 0x12;    // remote_method_hash
  remote_info[2] = 0x34;
  remote_info[3] = 0x45;    // signature_offset
  remote_info[4] = 0x67;
  remote_info[5] = 9;       // virtual_method_token
  remote_info[6] = 1;       // hash_modifier_length
  remote_info[7] = 'h';
  remote_info[8] = 4;       // class_name_length
  remote_info[9] = 'I';
  remote_info[10] = 'm';
  remote_info[11] = 'p';
  remote_info[12] = 'l';
  remote_info[13] = 1;      // remote_interfaces_count
  remote_info[14] = 0x80;   // external class_ref package token
  remote_info[15] = 0x02;   // external class_ref class token

  assert(class_info->getSize() == sizeof(jcvm::jc_cap_class_info) +
                                      remote_info_size);
  const auto *parsed = class_info->remoteInterfaceInfo();
  assert(parsed != nullptr);
  assert(parsed->remote_methods_count == 1);
  assert(parsed->remote_methods[0].getMethodHash() == 0x1234);
  assert(parsed->remote_methods[0].getSignatureOffset() == 0x4567);
  assert(parsed->remote_methods[0].virtual_method_token == 9);
  assert(parsed->hashModifierLength() == 1 && parsed->hashModifier()[0] == 'h');
  assert(parsed->classNameLength() == 4);
  assert(parsed->className()[0] == 'I' && parsed->className()[3] == 'l');
  assert(parsed->remoteInterfacesCount() == 1);
  assert(parsed->remoteInterfaces()[0].isExternalClassRef());
  assert(parsed->remoteInterfaces()[0].external_classref.package_token == 0x80);
  assert(parsed->remoteInterfaces()[0].external_classref.class_token == 0x02);

  // Run the exact Class-component traversal used during INSTALL with an
  // external target. The production resolver supplies this predicate from
  // installed Export components; the host cases cover valid interface,
  // exported class, and missing export outcomes.
  auto external_class_bytes =
      std::array<uint8_t, sizeof(class_component_bytes)>{};
  std::memcpy(external_class_bytes.data(), class_component_bytes,
              sizeof(class_component_bytes));
  const size_t external_reference_offset =
      static_cast<size_t>(reinterpret_cast<const uint8_t *>(
                              emitted_remote_info->remoteInterfaces()) -
                          class_component_bytes);
  external_class_bytes[external_reference_offset] = 0x80;
  external_class_bytes[external_reference_offset + 1] = 0x02;
  const auto *external_component =
      reinterpret_cast<const jcvm::jc_cap_class_component *>(
          external_class_bytes.data());
  bool resolver_called = false;
  jcvm::validateRemoteInterfaceTargets(
      external_component, [&resolver_called](const jcvm::jc_cap_class_ref ref) {
        resolver_called = true;
        return ref.isExternalClassRef() &&
               ref.external_classref.package_token == 0x80 &&
               ref.external_classref.class_token == 0x02;
      });
  assert(resolver_called); // matching exported interface is accepted

  bool rejected_exported_class = false;
  try {
    jcvm::validateRemoteInterfaceTargets(
        external_component, [](const jcvm::jc_cap_class_ref) { return false; });
  } catch (const jcvm::Exceptions error) {
    rejected_exported_class = error == jcvm::Exceptions::SecurityException;
  }
  assert(rejected_exported_class);

  bool rejected_missing_export = false;
  try {
    jcvm::validateRemoteInterfaceTargets(
        external_component, [](const jcvm::jc_cap_class_ref) { return false; });
  } catch (const jcvm::Exceptions error) {
    rejected_missing_export = error == jcvm::Exceptions::SecurityException;
  }
  assert(rejected_missing_export);

  class_info->flags = 0;
  assert(class_info->remoteInterfaceInfo() == nullptr);
  assert(class_info->getSize() == sizeof(jcvm::jc_cap_class_info));
  std::puts("RMI_CAP_METADATA:PASS package_name=bounded signature_pool=4 remote_methods=2 interface=Counter class=CounterImpl");
  return 0;
}
