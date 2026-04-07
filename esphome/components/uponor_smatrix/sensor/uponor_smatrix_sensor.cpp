#include "uponor_smatrix_sensor.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace uponor_smatrix {

static const char *const TAG = "uponor_smatrix.sensor";

void UponorSmatrixSensor::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Uponor Smatrix Sensor\n"
                "  Device address: 0x%08" PRIX32,
                this->address_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "External Temperature", this->external_temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Target Temperature", this->target_temperature_sensor_);
}

void UponorSmatrixSensor::loop() {
  if (this->target_temperature_sensor_ == nullptr)
    return;

  // Publish state after all update packets are processed
  const uint32_t now = App.get_loop_component_start_time();
  if (this->last_data_ != 0 && (now - this->last_data_ > 100) && this->target_temperature_raw_ != 0) {
    // Apply offsets that are currently applied by the thermostat based on the current mode
    uint16_t temp_raw = this->target_temperature_raw_;
    if (this->cooling_) {
      temp_raw += this->heating_cooling_offset_raw_;
      if (this->eco_mode_)
        temp_raw += this->eco_setback_value_raw_;
    } else {
      if (this->eco_mode_)
        temp_raw -= this->eco_setback_value_raw_;
    }
    // Convert raw value to actual temperature shown on the thermostat
    this->target_temperature_sensor_->publish_state(roundf(raw_to_celsius(temp_raw) / 0.5) * 0.5);
    this->last_data_ = 0;
  }
}

void UponorSmatrixSensor::on_device_data(const UponorSmatrixData *data, size_t data_len) {
  for (size_t i = 0; i < data_len; i++) {
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
        // Ignore invalid values here as they are used by the controller to explicitely request the setpoint from a
        // thermostat
        if (data[i].value != UPONOR_INVALID_VALUE)
          this->target_temperature_raw_ = data[i].value;
        break;
      case UPONOR_ID_HEATING_COOLING_OFFSET:
        this->heating_cooling_offset_raw_ = data[i].value;
        break;
      case UPONOR_ID_ECO_SETBACK:
        this->eco_setback_value_raw_ = data[i].value;
        break;
      case UPONOR_ID_DEMAND:
        this->cooling_ = (data[i].value & 0x1000) != 0;
        break;
      case UPONOR_ID_MODE1:
        this->eco_mode_ = (data[i].value & 0x0008) != 0;
        break;
    }
  }
}

}  // namespace uponor_smatrix
}  // namespace esphome
