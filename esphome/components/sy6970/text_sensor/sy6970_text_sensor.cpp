#include "sy6970_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sy6970 {

static const char *const TAG = "sy6970.text_sensor";

void SY6970TextSensor::update() {
  if (this->bus_status_text_sensor_ != nullptr) {
    const char *bus_status = this->parent_->get_bus_status_string();
    this->bus_status_text_sensor_->publish_state(bus_status);
  }

  if (this->charge_status_text_sensor_ != nullptr) {
    const char *charge_status = this->parent_->get_charge_status_string();
    this->charge_status_text_sensor_->publish_state(charge_status);
  }

  if (this->ntc_status_text_sensor_ != nullptr) {
    const char *ntc_status = this->parent_->get_ntc_status_string();
    this->ntc_status_text_sensor_->publish_state(ntc_status);
  }
}

}  // namespace sy6970
}  // namespace esphome
