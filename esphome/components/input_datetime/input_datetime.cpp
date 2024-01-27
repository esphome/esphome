#include "input_datetime.h"

#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace input_datetime {

static const char *const TAG = "input_datetime";

InputDatetime::InputDatetime() {}

void InputDatetime::publish_state(ESPTime state) {
  this->has_state_ = true;
  this->state = state;
  ESP_LOGD(TAG, "'%s': Sending state %s has_date: %s  has_time:%s", this->get_name().c_str(),
           state.strftime(STRFTIME_FORMAT_FROM_OBJ(this, false)).c_str(), this->has_date ? "yes" : "no",
           this->has_time ? "yes" : "no");
  this->state_callback_.call(state);
}

void InputDatetime::add_on_state_callback(std::function<void(ESPTime)> &&callback) {
  this->state_callback_.add(std::move(callback));
}

}  // namespace input_datetime
}  // namespace esphome
