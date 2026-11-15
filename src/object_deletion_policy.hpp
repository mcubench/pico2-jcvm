#pragma once

namespace pico2_jcvm {

constexpr bool may_service_object_deletion(const bool request_pending,
                                           const bool stack_empty,
                                           const bool transaction_active) {
  return request_pending && stack_empty && !transaction_active;
}

} // namespace pico2_jcvm
