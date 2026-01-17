#include "sy6970_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome::sy6970 {

static const char *const TAG = "sy6970.binary_sensor";

void SY6970BinarySensor::update() {
  if (this->vbus_connected_binary_sensor_ != nullptr) {
    bool vbus_connected = this->parent_->is_vbus_connected();
    this->vbus_connected_binary_sensor_->publish_state(vbus_connected);
  }

  if (this->charging_binary_sensor_ != nullptr) {
    bool charging = this->parent_->is_charging();
    this->charging_binary_sensor_->publish_state(charging);
  }

  if (this->charge_done_binary_sensor_ != nullptr) {
    bool charge_done = this->parent_->is_charge_done();
    this->charge_done_binary_sensor_->publish_state(charge_done);
  }
}

}  // namespace esphome::sy6970
