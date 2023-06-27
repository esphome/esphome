#include "airthings_wave_mini.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_wave_mini {

static const char *const TAG = "airthings_wave_mini";

void AirthingsWaveMini::read_sensors(uint8_t *raw_value, uint16_t value_len) {
  auto *value = (WaveMiniReadings *) raw_value;

  if (sizeof(WaveMiniReadings) <= value_len) {
    if (this->humidity_sensor_ != nullptr) {
      this->humidity_sensor_->publish_state(value->humidity / 100.0f);
    }

    if (this->pressure_sensor_ != nullptr) {
      this->pressure_sensor_->publish_state(value->pressure / 50.0f);
    }

    if (this->temperature_sensor_ != nullptr) {
      this->temperature_sensor_->publish_state(value->temperature / 100.0f - 273.15f);
    }

    if ((this->tvoc_sensor_ != nullptr) && this->is_valid_voc_value_(value->voc)) {
      this->tvoc_sensor_->publish_state(value->voc);
    }

    // This instance must not stay connected
    // so other clients can connect to it (e.g. the
    // mobile app).
    this->parent()->set_enabled(false);
  }
}

void AirthingsWaveMini::dump_config() {
  // these really don't belong here, but there doesn't seem to be a
  // practical way to have the base class use LOG_SENSOR and include
  // the TAG from this component
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
}

AirthingsWaveMini::AirthingsWaveMini() {
  this->service_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(SERVICE_UUID);
  this->sensors_data_characteristic_uuid_ = esp32_ble_tracker::ESPBTUUID::from_raw(CHARACTERISTIC_UUID);
}

}  // namespace airthings_wave_mini
}  // namespace esphome

#endif  // USE_ESP32
