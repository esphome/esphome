#include "ntc.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ntc {

static const char *const TAG = "ntc";

void NTC::setup() {
  this->sensor_->add_on_state_callback([this](float value) { this->process_(value); });
  if (this->sensor_->has_state())
    this->process_(this->sensor_->state);
}
void NTC::dump_config() { LOG_SENSOR("", "NTC Sensor", this); }
void NTC::process_(float value) {
  if (std::isnan(value)) {
    this->publish_state(NAN);
    return;
  }

  double lr = log(double(value));
  double v = this->a_ + this->b_ * lr + this->c_ * lr * lr * lr;
  auto temp = float(1.0 / v - 273.15);

  ESP_LOGV(TAG, "'%s' - Temperature: %.1f°C", this->name_.c_str(), temp);
  this->publish_state(temp);
}

}  // namespace ntc
}  // namespace esphome
