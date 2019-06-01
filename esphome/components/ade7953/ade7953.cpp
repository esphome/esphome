#include "ade7953.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ade7953 {

static const char *TAG = "ade7953";

void ADE7953::dump_config() {
  ESP_LOGCONFIG(TAG, "ADE7953:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Voltage Sensor", this->voltage_sensor_);
  LOG_SENSOR("  ", "Current A Sensor", this->current_a_sensor_);
  LOG_SENSOR("  ", "Current B Sensor", this->current_b_sensor_);
  LOG_SENSOR("  ", "Active Power A Sensor", this->active_power_a_sensor_);
  LOG_SENSOR("  ", "Active Power B Sensor", this->active_power_b_sensor_);
}

}  // namespace ade7953
}  // namespace esphome
