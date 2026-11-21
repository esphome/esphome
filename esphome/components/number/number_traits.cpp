#include "esphome/core/log.h"
#include "number_traits.h"

namespace esphome::number {

ESPHOME_LOG_TAG(TAG, "number");

void NumberTraits::set_range(const NumberRange *range) {
  if (this->owns_range_) {
    progmem_memcpy(this->writable_range_(), range, sizeof(NumberRange));
    return;
  }
  this->range_ = range;
}

NumberRange *NumberTraits::writable_range_() {
  if (!this->owns_range_) {
    auto *copy = new NumberRange;  // NOLINT(cppcoreguidelines-owning-memory)
    progmem_memcpy(copy, this->range_, sizeof(NumberRange));
    this->range_ = copy;
    this->owns_range_ = true;
  }
  // owns_range_ means range_ points at RAM
  return const_cast<NumberRange *>(this->range_);  // NOLINT(cppcoreguidelines-pro-type-const-cast)
}

}  // namespace esphome::number
