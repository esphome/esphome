#include "ruuvitag.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace ruuvitag {

static const char *TAG = "ruuvitag";

void RuuviTag::dump_config() {
  ESP_LOGCONFIG(TAG, "RuuviTag");
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Pressure", this->pressure_);  
  LOG_SENSOR("  ", "Acceleration X", this->acceleration_x_);  
  LOG_SENSOR("  ", "Acceleration Y", this->acceleration_y_);  
  LOG_SENSOR("  ", "Acceleration Z", this->acceleration_z_);  
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
}

}  // namespace ruuvitag
}  // namespace esphome

#endif
