#include "ruuvitag.h"
#include "esphome/core/log.h"

namespace esphome::ruuvitag {

ESPHOME_LOG_TAG(TAG, "ruuvitag");

void RuuviTag::dump_config() {
  ESP_LOGCONFIG(TAG, "RuuviTag");
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Acceleration", this->acceleration_sensor_);
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_sensor_);
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_sensor_);
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "TX Power", this->tx_power_sensor_);
  LOG_SENSOR("  ", "Movement Counter", this->movement_counter_sensor_);
  LOG_SENSOR("  ", "Measurement Sequence Number", this->measurement_sequence_number_sensor_);
}

}  // namespace esphome::ruuvitag
