#include "max17043.h"
#include "esphome/core/log.h"

namespace esphome::max17043 {

// MAX17043 is a 1-Cell Fuel Gauge with ModelGauge and Low-Battery Alert
// Consult the datasheet at https://www.analog.com/en/products/max17043.html

ESPHOME_LOG_TAG(TAG, "max17043");

void MAX17043Component::update() {
  if (!this->ready_for_update_())
    return;

  uint16_t raw;
  bool ok = true;

  if (this->voltage_sensor_ != nullptr) {
    if (this->read_byte_16(max170xx::REG_VCELL, &raw)) {
      // 12 bits in the top of the word at 1.25 mV
      this->voltage_sensor_->publish_state((raw >> 4) * 1.25e-3f);
    } else {
      ok = false;
    }
  }
  if (this->battery_remaining_sensor_ != nullptr) {
    if (this->read_byte_16(max170xx::REG_SOC, &raw)) {
      float percent = (float) ((raw >> 8) + 0.003906f * (raw & 0x00ff));
      this->battery_remaining_sensor_->publish_state(percent);
    } else {
      ok = false;
    }
  }

  this->status_comm_result(ok, LOG_STR("Register read failed"));
}

void MAX17043Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MAX17043:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Battery Voltage", this->voltage_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_remaining_sensor_);
}

}  // namespace esphome::max17043
