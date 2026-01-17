#include "sy6970_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sy6970 {

static const char *const TAG = "sy6970.sensor";

void SY6970Sensor::update() {
  if (this->vbus_voltage_sensor_ != nullptr) {
    uint16_t vbus_mv = this->parent_->get_vbus_voltage();
    this->vbus_voltage_sensor_->publish_state(vbus_mv / 1000.0f);
  }

  if (this->battery_voltage_sensor_ != nullptr) {
    uint16_t battery_mv = this->parent_->get_battery_voltage();
    this->battery_voltage_sensor_->publish_state(battery_mv / 1000.0f);
  }

  if (this->system_voltage_sensor_ != nullptr) {
    uint16_t system_mv = this->parent_->get_system_voltage();
    this->system_voltage_sensor_->publish_state(system_mv / 1000.0f);
  }

  if (this->charge_current_sensor_ != nullptr) {
    uint16_t charge_ma = this->parent_->get_charge_current();
    this->charge_current_sensor_->publish_state(charge_ma);
  }

  if (this->precharge_current_sensor_ != nullptr) {
    uint16_t precharge_ma = this->parent_->get_precharge_current();
    this->precharge_current_sensor_->publish_state(precharge_ma);
  }
}

}  // namespace sy6970
}  // namespace esphome
