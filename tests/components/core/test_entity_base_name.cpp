#include <gtest/gtest.h>

#include "esphome/core/entity_base.h"

#include <type_traits>
#include <utility>

namespace esphome::core::testing {

// External components return get_name() from `const StringRef &` getters, so it must stay a reference
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static_assert(std::is_same_v<decltype(std::declval<const EntityBase &>().get_name()), const StringRef &>);
#pragma GCC diagnostic pop

}  // namespace esphome::core::testing
