#include "number.h"
#include "esphome/core/defines.h"
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"

namespace esphome::number {

static const char *const TAG = "number";

// Function implementation of LOG_NUMBER macro to reduce code size
void log_number(const char *tag, const char *prefix, const char *type, Number *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());
  LOG_ENTITY_ICON(tag, prefix, *obj);
  LOG_ENTITY_UNIT_OF_MEASUREMENT(tag, prefix, *obj);
  LOG_ENTITY_DEVICE_CLASS(tag, prefix, *obj);
}

void Number::set_range(const NumberRange *range) {
  // Aligned 32-bit loads are safe from PROGMEM on ESP8266 (same as pgm_read_float_aligned).
  this->traits.set_min_value(range->min_value);
  this->traits.set_max_value(range->max_value);
  this->traits.set_step(range->step);
}

void Number::publish_state(float state) {
  this->set_has_state(true);
  this->state = state;
  ESP_LOGV(TAG, "'%s' >> %.2f", this->get_name().c_str(), state);
  this->state_callback_.call(state);
#if defined(USE_NUMBER) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_number_update(this);
#endif
}

}  // namespace esphome::number
