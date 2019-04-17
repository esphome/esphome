#include "text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace text_sensor {

static const char *TAG = "text_sensor";

TextSensor::TextSensor() : TextSensor("") {}
TextSensor::TextSensor(const std::string &name) : Nameable(name) {}

void TextSensor::publish_state(std::string state) {
  this->state = state;
  this->has_state_ = true;
  ESP_LOGD(TAG, "'%s': Sending state '%s'", this->name_.c_str(), state.c_str());
  this->callback_.call(state);
}
void TextSensor::set_icon(const std::string &icon) { this->icon_ = icon; }
void TextSensor::add_on_state_callback(std::function<void(std::string)> callback) {
  this->callback_.add(std::move(callback));
}
std::string TextSensor::get_icon() {
  if (this->icon_.has_value())
    return *this->icon_;
  return this->icon();
}
std::string TextSensor::icon() { return ""; }
std::string TextSensor::unique_id() { return ""; }
bool TextSensor::has_state() { return this->has_state_; }
uint32_t TextSensor::hash_base() { return 334300109UL; }

}  // namespace text_sensor
}  // namespace esphome
