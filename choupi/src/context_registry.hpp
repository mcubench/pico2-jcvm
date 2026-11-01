#ifndef CHOUPI_CONTEXT_REGISTRY_HPP
#define CHOUPI_CONTEXT_REGISTRY_HPP

#include "context.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace jcvm {

template <std::size_t Count>
Context *findAppletContext(
    Context *selected,
    std::array<std::unique_ptr<Context>, Count> &inactive,
    const uint8_t owner, const uint8_t *aid,
    const uint8_t aid_length) noexcept {
  if (aid == nullptr || aid_length < 5 || aid_length > 16) return nullptr;

  const auto matches = [&](const Context *context) noexcept {
    if (context == nullptr || context->getAppletID() != owner) return false;
    uint8_t stored_aid[16]{};
    uint8_t stored_length = 0;
    return context->copyAppletAID(stored_aid, stored_length) &&
           stored_length == aid_length &&
           std::memcmp(stored_aid, aid, aid_length) == 0;
  };

  if (matches(selected)) return selected;
  for (auto &context : inactive) {
    if (matches(context.get())) return context.get();
  }
  return nullptr;
}

} // namespace jcvm

#endif // CHOUPI_CONTEXT_REGISTRY_HPP
