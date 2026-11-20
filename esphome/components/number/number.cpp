#include "number.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::number {

ESPHOME_LOG_TAG(TAG, "number");

// Function implementation of LOG_NUMBER macro to reduce code size
void log_number(const char *tag, const char *prefix, const char *type, Number *obj) {
  if (!log_entity_header(tag, prefix, type, obj)) {
    return;
  }
  LOG_ENTITY_ICON(tag, prefix, *obj);
  LOG_ENTITY_UNIT_OF_MEASUREMENT(tag, prefix, *obj);
  LOG_ENTITY_DEVICE_CLASS(tag, prefix, *obj);
}

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
void Number::publish_state(float state) {
  this->set_has_state(true);
  this->state = state;
  ESP_LOGV(TAG, "'%s' >> %.2f", LOG_STR_ARG(this->get_log_name()), state);
  this->state_callback_.call(state);
#if defined(USE_NUMBER) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_number_update(this);
#endif
}

}  // namespace esphome::number
