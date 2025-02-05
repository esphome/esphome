#include "uponor_smatrix_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uponor_smatrix {

static const char *const TAG = "uponor_smatrix.sensor";

void UponorSmatrixSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Uponor Smatrix Sensor");
  ESP_LOGCONFIG(TAG, "  Device address: 0x%04X", this->address_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "External Temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Target Temperature", this->target_temperature_sensor_);
}

void UponorSmatrixSensor::on_device_data(const UponorSmatrixData *data, size_t data_len) {
  for (int i = 0; i < data_len; i++) {
    switch (data[i].id) {
      case UPONOR_ID_ROOM_TEMP:
        if (this->temperature_sensor_ != nullptr)
          this->temperature_sensor_->publish_state(raw_to_celsius(data[i].value));
        break;
      case UPONOR_ID_EXTERNAL_TEMP:
        if (this->external_temperature_sensor_ != nullptr)
          this->external_temperature_sensor_->publish_state(raw_to_celsius(data[i].value));
        break;
      case UPONOR_ID_HUMIDITY:
        if (this->humidity_sensor_ != nullptr)
          this->humidity_sensor_->publish_state(data[i].value & 0x00FF);
        break;
      case UPONOR_ID_TARGET_TEMP:
        if (this->target_temperature_sensor_ != nullptr)
          this->target_temperature_sensor_->publish_state(raw_to_celsius(data[i].value));
        break;
    }
  }
}

}  // namespace uponor_smatrix
}  // namespace esphome
