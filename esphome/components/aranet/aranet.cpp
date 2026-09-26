#include "aranet.h"

#include <cinttypes>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::aranet {

static const char *const TAG = "aranet";
static constexpr uint16_t ARANET_MANUFACTURER_ID = 0x0702;
static constexpr size_t ARANET4_DATA_LENGTH = 22;
static constexpr size_t OTHER_ARANET_DATA_LENGTH = 24;

enum class AranetDeviceType : uint8_t {
  ARANET_DEVICE_TYPE_ARANET4 = 0,
  ARANET_DEVICE_TYPE_ARANET2 = 1,
  ARANET_DEVICE_TYPE_RADIATION = 2,
  ARANET_DEVICE_TYPE_RADON = 3,
};

// Aranet does not publish an official advertisement specification. The layout below is corroborated by these
// independently maintained implementations:
// https://github.com/Anrijs/Aranet4-Python/blob/master/aranet4/client.py
// https://github.com/Anrijs/Aranet4-ESP32/blob/main/src/Aranet4.h

void Aranet::dump_config() {
  ESP_LOGCONFIG(TAG, "Aranet");
  LOG_SENSOR("  ", "Carbon Dioxide", this->co2_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Pressure", this->pressure_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Measurement Interval", this->measurement_interval_sensor_);
  LOG_SENSOR("  ", "Measurement Age", this->measurement_age_sensor_);
  LOG_SENSOR("  ", "Signal Strength", this->signal_strength_sensor_);
  LOG_SENSOR("  ", "Radon Concentration", this->radon_sensor_);
  LOG_SENSOR("  ", "Radiation Dose Rate", this->radiation_rate_sensor_);
  LOG_SENSOR("  ", "Radiation Total Dose", this->radiation_total_sensor_);
  LOG_SENSOR("  ", "Radiation Dose Duration", this->radiation_duration_sensor_);
}

bool Aranet::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_)
    return false;

  for (const auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (manufacturer_data.uuid != ble_device_base::ESPBTUUID::from_uint16(ARANET_MANUFACTURER_ID))
      continue;

    const auto &data = manufacturer_data.data;
    if (data.size() != ARANET4_DATA_LENGTH && data.size() != OTHER_ARANET_DATA_LENGTH) {
      if (!this->has_warned_invalid_length_) {
        ESP_LOGW(TAG, "Unexpected manufacturer data length: %u", static_cast<unsigned>(data.size()));
        this->has_warned_invalid_length_ = true;
      }
      return false;
    }

    const bool is_aranet4 = data.size() == ARANET4_DATA_LENGTH;
    const size_t header_offset = is_aranet4 ? 0 : 1;
    if ((data[header_offset] & 0x20) == 0) {
      if (!this->has_warned_integration_disabled_) {
        ESP_LOGW(TAG, "Smart Home integration is disabled on the Aranet device");
        this->has_warned_integration_disabled_ = true;
      }
      return false;
    }

    const auto device_type =
        is_aranet4 ? AranetDeviceType::ARANET_DEVICE_TYPE_ARANET4 : static_cast<AranetDeviceType>(data[0]);
    if (!is_aranet4 && device_type != AranetDeviceType::ARANET_DEVICE_TYPE_ARANET2 &&
        device_type != AranetDeviceType::ARANET_DEVICE_TYPE_RADIATION &&
        device_type != AranetDeviceType::ARANET_DEVICE_TYPE_RADON) {
      if (!this->has_warned_unsupported_type_) {
        ESP_LOGW(TAG, "Unsupported Aranet device type: %u", data[0]);
        this->has_warned_unsupported_type_ = true;
      }
      return false;
    }

    const uint16_t measurement_interval = encode_uint16(data[18 + 2 * header_offset], data[17 + 2 * header_offset]);
    const uint16_t measurement_age = encode_uint16(data[20 + 2 * header_offset], data[19 + 2 * header_offset]);
    const uint8_t measurement_counter = data[21 + 2 * header_offset];

    if (this->measurement_age_sensor_ != nullptr)
      this->measurement_age_sensor_->publish_state(measurement_age);
    if (this->signal_strength_sensor_ != nullptr)
      this->signal_strength_sensor_->publish_state(device.get_rssi());

    if (this->has_measurement_ && measurement_counter == this->last_measurement_counter_)
      return true;
    this->last_measurement_counter_ = measurement_counter;
    this->has_measurement_ = true;

    if (device_type == AranetDeviceType::ARANET_DEVICE_TYPE_ARANET4) {
      const uint16_t co2 = encode_uint16(data[9], data[8]);
      const float temperature = encode_uint16(data[11], data[10]) / 20.0f;
      const float pressure = encode_uint16(data[13], data[12]) / 10.0f;
      if (this->co2_sensor_ != nullptr)
        this->co2_sensor_->publish_state(co2);
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->pressure_sensor_ != nullptr)
        this->pressure_sensor_->publish_state(pressure);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(data[14]);
      ESP_LOGD(TAG, "Aranet4 CO2: %u ppm, temperature: %.1f C, humidity: %u%%, pressure: %.1f hPa", co2, temperature,
               data[14], pressure);
    } else if (device_type == AranetDeviceType::ARANET_DEVICE_TYPE_ARANET2) {
      const float temperature = encode_uint16(data[11], data[10]) / 20.0f;
      const float humidity = encode_uint16(data[15], data[14]) / 10.0f;
      if (this->temperature_sensor_ != nullptr)
        this->temperature_sensor_->publish_state(temperature);
      if (this->humidity_sensor_ != nullptr)
        this->humidity_sensor_->publish_state(humidity);
      ESP_LOGD(TAG, "Aranet2 temperature: %.1f C, humidity: %.1f%%", temperature, humidity);
    } else if (device_type == AranetDeviceType::ARANET_DEVICE_TYPE_RADON) {
      const uint16_t radon = encode_uint16(data[9], data[8]);
      const float temperature = encode_uint16(data[11], data[10]) / 20.0f;
      const float pressure = encode_uint16(data[13], data[12]) / 10.0f;
      const float humidity = encode_uint16(data[15], data[14]) / 10.0f;
      if (this->radon_sensor_ != nullptr)
        this->radon_sensor_->publish_state(radon);
      // Aranet Radon One uses zero temperature and humidity to indicate fields not supported by that model.
      if (this->temperature_sensor_ != nullptr && temperature != 0)
        this->temperature_sensor_->publish_state(temperature);
      if (this->pressure_sensor_ != nullptr)
        this->pressure_sensor_->publish_state(pressure);
      if (this->humidity_sensor_ != nullptr && humidity != 0)
        this->humidity_sensor_->publish_state(humidity);
      ESP_LOGD(TAG, "Aranet Radon: %u Bq/m3, temperature: %.1f C, humidity: %.1f%%, pressure: %.1f hPa", radon,
               temperature, humidity, pressure);
    } else {
      const uint32_t radiation_total = encode_uint32(data[9], data[8], data[7], data[6]);
      const uint32_t radiation_duration = encode_uint32(data[13], data[12], data[11], data[10]);
      const uint16_t radiation_rate = encode_uint16(data[15], data[14]);
      if (this->radiation_rate_sensor_ != nullptr)
        this->radiation_rate_sensor_->publish_state(radiation_rate / 1000.0f);
      if (this->radiation_total_sensor_ != nullptr)
        this->radiation_total_sensor_->publish_state(radiation_total);
      if (this->radiation_duration_sensor_ != nullptr)
        this->radiation_duration_sensor_->publish_state(radiation_duration);
      ESP_LOGD(TAG, "Aranet Radiation dose rate: %u nSv/h, total dose: %" PRIu32 " nSv", radiation_rate,
               radiation_total);
    }
    if (this->battery_level_sensor_ != nullptr)
      this->battery_level_sensor_->publish_state(data[15 + 2 * header_offset]);
    if (this->measurement_interval_sensor_ != nullptr)
      this->measurement_interval_sensor_->publish_state(measurement_interval);

    return true;
  }

  return false;
}

}  // namespace esphome::aranet
