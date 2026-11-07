#include "context.hpp"
#include "heap.hpp"
#include "jc_types/jc_array.hpp"

#include <iostream>
#include <memory>
#include <vector>

namespace jcvm {
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
    jshort_t, jbyte_t);
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(jshort_t,
                                                                   jbyte_t);
#ifdef JCVM_INT_SUPPORTED
std::shared_ptr<JC_Array>
fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(jshort_t,
                                                                  jbyte_t);
#endif
jref_t fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(jbyte_t,
                                                                    jshort_t);
}

namespace {
template <typename Operation>
bool expectSystemReason(jcvm::Context &context, const char *name,
                        const Operation &operation, const uint16_t reason) {
  try {
    operation();
  } catch (jcvm::Exceptions error) {
    const uint16_t actual = context.consumePendingSystemExceptionReason();
    if (error == jcvm::Exceptions::SystemException && actual == reason) {
      return true;
    }
    std::cerr << "TEST:FAIL " << name << " exception="
              << static_cast<int>(error)
              << " reason=" << actual << " expected=" << reason << "\n";
    return false;
  }
  std::cerr << "TEST:FAIL " << name << " unexpectedly succeeded\n";
  return false;
}
} // namespace

int main() {
  jcvm::Context context(0, 0);
  jcvm::Context::setActiveContext(&context);
  constexpr auto clear_on_reset =
      static_cast<jcvm::jbyte_t>(jcvm::CLEAR_ON_RESET);

#ifdef JCVM_INT_SUPPORTED
  if (!expectSystemReason(context, "local_int_backing_limit", [&] {
        (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientIntArray(
            16384, clear_on_reset);
      }, 2) ||
      !expectSystemReason(context, "global_int_backing_limit", [&] {
        (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(
            4, 16384);
      }, 2)) {
    return 1;
  }
#else
  auto local_max =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientObjectArray(
          32767, clear_on_reset);
  const jcvm::jref_t global_max =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(5,
                                                                         32767);
  if (!local_max || local_max->size() != 32767 || global_max.isNullPointer()) {
    std::cerr << "TEST:FAIL maximum int-disabled transient array length\n";
    return 1;
  }
  jcvm::Heap::releaseGlobalArray(global_max);
  if (!expectSystemReason(context, "global_int_disabled", [&] {
        (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(
            4, 1);
      }, 1)) {
    return 1;
  }
#endif
  try {
    (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
        -1, clear_on_reset);
    std::cerr << "TEST:FAIL negative_local_length unexpectedly succeeded\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NegativeArraySizeException) {
      std::cerr << "TEST:FAIL negative_local_length exception="
                << static_cast<int>(error) << "\n";
      return 1;
    }
  }
  try {
    (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(5,
                                                                            -1);
    std::cerr << "TEST:FAIL negative_global_length unexpectedly succeeded\n";
    return 1;
  } catch (jcvm::Exceptions error) {
    if (error != jcvm::Exceptions::NegativeArraySizeException) {
      std::cerr << "TEST:FAIL negative_global_length exception="
                << static_cast<int>(error)
                << "\n";
      return 1;
    }
  }

  auto valid_local =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
          1, clear_on_reset);
  const jcvm::jref_t local_ref =
      context.getHeap().addTransientArrayResult(*valid_local);
  if (!context.getHeap().getArray(local_ref) ||
      context.getHeap().getArray(local_ref)->size() != 1) {
    std::cerr << "TEST:FAIL local_registration_after_rejection\n";
    return 1;
  }
  const jcvm::jref_t valid_global =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(5, 1);
  if (valid_global.isNullPointer()) {
    std::cerr << "TEST:FAIL global_registration_after_rejection\n";
    return 1;
  }
  jcvm::Heap::releaseGlobalArray(valid_global);

  jcvm::Context full_context(1, 0);
  jcvm::Context::setActiveContext(&full_context);
  jcvm::Heap &full_heap = full_context.getHeap();
  jcvm::jref_t last_handle;
  for (uint16_t index = 0; index < 0x3fff; ++index) {
    last_handle = full_heap.addArray(0, jcvm::JAVA_ARRAY_T_BYTE);
  }
  auto unregistered =
      jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeTransientByteArray(
          1, clear_on_reset);
  if (!expectSystemReason(full_context, "local_handle_table_exhaustion", [&] {
        (void)full_heap.addTransientArrayResult(*unregistered);
      }, 2) ||
      !full_heap.getArray(last_handle)) {
    std::cerr << "TEST:FAIL failed registration damaged existing handles\n";
    return 1;
  }

  std::vector<jcvm::jref_t> global_handles;
  global_handles.reserve(0x3fff);
  for (uint16_t index = 0; index < 0x3fff; ++index) {
    global_handles.push_back(
        jcvm::Heap::addGlobalArray(0, jcvm::JAVA_ARRAY_T_BYTE));
  }
  if (!expectSystemReason(full_context, "global_handle_table_exhaustion", [&] {
        (void)jcvm::fr_gouv_ssi_nativeimpl_NativeImplementation_makeGlobalArray(
            2, 0);
      }, 2) ||
      !jcvm::Heap::getGlobalArray(global_handles.back())) {
    std::cerr << "TEST:FAIL global registration failure damaged existing handles\n";
    return 1;
  }

  std::cout << "TEST:PASS transient_backing_limit_exact_reasons\n";
  std::cout << "TEST:PASS transient_negative_length_reasons\n";
  std::cout << "TEST:PASS rejected_transient_allocation_no_leaked_global_slot\n";
  std::cout << "TEST:PASS transient_handle_exhaustion_exact_reason\n";
  std::cout << "TEST:PASS transient_global_handle_exhaustion_exact_reason\n";
  return 0;
}
