#include "airthings_wave_plus.h"

#ifdef USE_ESP32

namespace esphome {
namespace airthings_wave_plus {

static const char *const TAG = "airthings_wave_plus";

void AirthingsWavePlus::read_sensors(uint8_t *raw_value, uint16_t value_len) {
  auto *value = (WavePlusReadings *) raw_value;

  if (sizeof(WavePlusReadings) <= value_len) {
    ESP_LOGD(TAG, "version = %d", value->version);

    if (value->version == 1) {
      if (this->humidity_sensor_ != nullptr) {
        this->humidity_sensor_->publish_state(value->humidity / 2.0f);
      }

      if ((this->radon_sensor_ != nullptr) && this->is_valid_radon_value_(value->radon)) {
        this->radon_sensor_->publish_state(value->radon);
      }

      if ((this->radon_long_term_sensor_ != nullptr) && this->is_valid_radon_value_(value->radon_lt)) {
        this->radon_long_term_sensor_->publish_state(value->radon_lt);
      }

      if (this->temperature_sensor_ != nullptr) {
        this->temperature_sensor_->publish_state(value->temperature / 100.0f);
      }

      if (this->pressure_sensor_ != nullptr) {
        this->pressure_sensor_->publish_state(value->pressure / 50.0f);
      }

      if ((this->co2_sensor_ != nullptr) && this->is_valid_co2_value_(value->co2)) {
        this->co2_sensor_->publish_state(value->co2);
      }

      if ((this->tvoc_sensor_ != nullptr) && this->is_valid_voc_value_(value->voc)) {
        this->tvoc_sensor_->publish_state(value->voc);
      }

      if (this->illuminance_sensor_ != nullptr) {
        this->illuminance_sensor_->publish_state(value->ambientLight);
      }
    } else {
      ESP_LOGE(TAG, "Invalid payload version (%d != 1, newer version or not a Wave Plus?)", value->version);
    }
  }

  this->response_received_();
}

bool AirthingsWavePlus::is_valid_radon_value_(uint16_t radon) { return radon <= 16383; }

bool AirthingsWavePlus::is_valid_co2_value_(uint16_t co2) { return co2 <= 16383; }

void AirthingsWavePlus::dump_config() {
  // these really don't belong here, but there doesn't seem to be a
  // practical way to have the base class use LOG_SENSOR and include
  // the TAG from this component
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "TVOC", this->tvoc_sensor_);
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);

  LOG_SENSOR("  ", "Radon", this->radon_sensor_);
  LOG_SENSOR("  ", "Radon Long Term", this->radon_long_term_sensor_);
  LOG_SENSOR("  ", "CO2", this->co2_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
}

AirthingsWavePlus::AirthingsWavePlus() {
  this->service_uuid_ = espbt::ESPBTUUID::from_raw(SERVICE_UUID);
  this->sensors_data_characteristic_uuid_ = espbt::ESPBTUUID::from_raw(CHARACTERISTIC_UUID);
  this->access_control_point_characteristic_uuid_ =
      espbt::ESPBTUUID::from_raw(ACCESS_CONTROL_POINT_CHARACTERISTIC_UUID);
}

}  // namespace airthings_wave_plus
}  // namespace esphome

#endif  // USE_ESP32
