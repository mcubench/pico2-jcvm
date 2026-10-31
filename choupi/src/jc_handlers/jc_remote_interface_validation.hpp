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
** FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
*/

#ifndef JC_REMOTE_INTERFACE_VALIDATION_HPP
#define JC_REMOTE_INTERFACE_VALIDATION_HPP

#include "../exceptions.hpp"
#include "../jc_cap/jc_cap_class.hpp"

namespace jcvm {

// Walk Class.cap's remote metadata and ask the caller to resolve each
// external remote-interface reference. The resolver returns true only when
// the target exists and is an exported interface. Keeping traversal here
// makes install-time linking and its host regression test share the same path.
template <typename ExternalInterfaceResolver>
void validateRemoteInterfaceTargets(
    const jc_cap_class_component *component,
    ExternalInterfaceResolver &&is_exported_interface) {
  if (component == nullptr) {
    return;
  }

  const auto class_bytes = component->claz();
  uint16_t offset = component->classInfoStartOffset();
  if (offset > class_bytes.size()) {
    throw Exceptions::SecurityException;
  }

  while (offset < class_bytes.size()) {
    const uint8_t *record = class_bytes.data() + offset;
    if (IS_INTERFACE(record)) {
      const auto *interface_info =
          reinterpret_cast<const jc_cap_interface_info *>(record);
      const uint16_t record_size = interface_info->getSize();
      if (record_size == 0 || record_size > class_bytes.size() - offset) {
        throw Exceptions::SecurityException;
      }
      offset = static_cast<uint16_t>(offset + record_size);
      continue;
    }

    const auto *class_info =
        reinterpret_cast<const jc_cap_class_info *>(record);
    const uint16_t record_size = class_info->getSize();
    if (record_size == 0 || record_size > class_bytes.size() - offset) {
      throw Exceptions::SecurityException;
    }
    if (const auto *remote_info = class_info->remoteInterfaceInfo();
        remote_info != nullptr) {
      for (uint8_t index = 0; index < remote_info->remoteInterfacesCount();
           ++index) {
        const jc_cap_class_ref reference =
            remote_info->remoteInterfaces()[index];
        if (reference.isExternalClassRef() &&
            !is_exported_interface(reference)) {
          throw Exceptions::SecurityException;
        }
      }
    }
    offset = static_cast<uint16_t>(offset + record_size);
  }
  if (offset != class_bytes.size()) {
    throw Exceptions::SecurityException;
  }
}

} // namespace jcvm

#endif // JC_REMOTE_INTERFACE_VALIDATION_HPP
