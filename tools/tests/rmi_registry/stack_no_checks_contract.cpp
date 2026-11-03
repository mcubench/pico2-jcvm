#include "../../../choupi/src/jc_config.h"

// Owner enforcement may throw even in a build that omits stack bounds checks.
#undef JCRE_STACK_OVERFLOW_PROTECTION
#undef JCVM_ARRAY_SIZE_CHECK

#include "../../../choupi/src/stack.hpp"
#include <type_traits>

static_assert(!noexcept(std::declval<jcvm::Stack &>().get_Pushed_Element(1)),
              "foreign-reference rejection must remain catchable without bounds checks");
