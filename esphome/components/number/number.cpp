#include "number.h"
#include "esphome/core/log.h"

namespace esphome {
namespace number {

static const char *const TAG = "number";

// Function implementation of LOG_NUMBER macro to reduce code size
void log_number(const char *tag, const char *prefix, const char *type, Number *obj) {
  if (obj == nullptr) {
    return;
  }

  ESP_LOGCONFIG(tag, "%s%s '%s'", prefix, type, obj->get_name().c_str());

  if (!obj->get_icon().empty()) {
    ESP_LOGCONFIG(tag, "%s  Icon: '%s'", prefix, obj->get_icon().c_str());
  }

  if (!obj->traits.get_unit_of_measurement().empty()) {
    ESP_LOGCONFIG(tag, "%s  Unit of Measurement: '%s'", prefix, obj->traits.get_unit_of_measurement().c_str());
  }

  if (!obj->traits.get_device_class().empty()) {
    ESP_LOGCONFIG(tag, "%s  Device Class: '%s'", prefix, obj->traits.get_device_class().c_str());
  }
}

void Number::publish_state(float state) {
  this->set_has_state(true);
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %f", this->get_name().c_str(), state);
  this->state_callback_.call(state);
}

void Number::add_on_state_callback(std::function<void(float)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace number
}  // namespace esphome
