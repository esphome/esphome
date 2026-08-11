#include "aranet4.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::aranet4 {

static const char *const TAG = "aranet4";
static constexpr uint16_t ARANET_MANUFACTURER_ID = 0x0702;
static constexpr size_t ARANET4_DATA_LENGTH = 22;

// Aranet does not publish an official advertisement specification. The layout below is corroborated by these
// independently maintained implementations:
// https://github.com/Anrijs/Aranet4-Python/blob/master/aranet4/client.py
// https://github.com/Anrijs/Aranet4-ESP32/blob/main/src/Aranet4.h

void Aranet4::dump_config() {
  ESP_LOGCONFIG(TAG, "Aranet4");
  LOG_SENSOR("  ", "Carbon Dioxide", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Measurement Interval", this->measurement_interval_sensor_);
  LOG_SENSOR("  ", "Measurement Age", this->measurement_age_sensor_);
  LOG_SENSOR("  ", "Signal Strength", this->signal_strength_sensor_);
}

bool Aranet4::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  for (const auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (manufacturer_data.uuid != ble_device_base::ESPBTUUID::from_uint16(ARANET_MANUFACTURER_ID))
      continue;

    const auto &data = manufacturer_data.data;
    if (data.size() != ARANET4_DATA_LENGTH) {
      if (!this->has_warned_invalid_length_) {
        ESP_LOGW(TAG, "Unexpected manufacturer data length: %u", static_cast<unsigned>(data.size()));
        this->has_warned_invalid_length_ = true;
      }
      return false;
    }

    if ((data[0] & 0x20) == 0) {
      if (!this->has_warned_integration_disabled_) {
        ESP_LOGW(TAG, "Smart Home integration is disabled on the Aranet4");
        this->has_warned_integration_disabled_ = true;
      }
      return false;
    }

    const uint16_t co2 = encode_uint16(data[9], data[8]);
    const float temperature = encode_uint16(data[11], data[10]) / 20.0f;
    const float pressure = encode_uint16(data[13], data[12]) / 10.0f;
    const uint8_t humidity = data[14];
    const uint8_t battery_level = data[15];
    const uint16_t measurement_interval = encode_uint16(data[18], data[17]);
    const uint16_t measurement_age = encode_uint16(data[20], data[19]);
    const uint8_t measurement_counter = data[21];

    if (this->measurement_age_sensor_ != nullptr)
      this->measurement_age_sensor_->publish_state(measurement_age);
    if (this->signal_strength_sensor_ != nullptr)
      this->signal_strength_sensor_->publish_state(device.get_rssi());

    if (this->has_measurement_ && measurement_counter == this->last_measurement_counter_)
      return true;
    this->last_measurement_counter_ = measurement_counter;
    this->has_measurement_ = true;

    if (this->co2_sensor_ != nullptr)
      this->co2_sensor_->publish_state(co2);
    if (this->temperature_sensor_ != nullptr)
      this->temperature_sensor_->publish_state(temperature);
    if (this->humidity_sensor_ != nullptr)
      this->humidity_sensor_->publish_state(humidity);
    if (this->pressure_sensor_ != nullptr)
      this->pressure_sensor_->publish_state(pressure);
    if (this->battery_level_sensor_ != nullptr)
      this->battery_level_sensor_->publish_state(battery_level);
    if (this->measurement_interval_sensor_ != nullptr)
      this->measurement_interval_sensor_->publish_state(measurement_interval);

    ESP_LOGD(TAG, "CO2: %u ppm, temperature: %.1f C, humidity: %u%%, pressure: %.1f hPa", co2, temperature, humidity,
             pressure);
    return true;
  }

  return false;
}

}  // namespace esphome::aranet4
