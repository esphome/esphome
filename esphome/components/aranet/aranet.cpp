#include "aranet.h"

#include <cinttypes>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::aranet {

ESPHOME_LOG_TAG(TAG, "aranet");
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

  const auto manufacturer_uuid = ble_device_base::ESPBTUUID::from_uint16(ARANET_MANUFACTURER_ID);
  for (const auto &manufacturer_data : device.get_manufacturer_datas()) {
    if (manufacturer_data.uuid != manufacturer_uuid)
      continue;

    const auto &data = manufacturer_data.data;
    if (data.size() != ARANET4_DATA_LENGTH && data.size() != OTHER_ARANET_DATA_LENGTH) {
      if ((this->warned_ & WARNED_INVALID_LENGTH) == 0) {
        ESP_LOGW(TAG, "Unexpected manufacturer data length: %u", static_cast<unsigned>(data.size()));
        this->warned_ |= WARNED_INVALID_LENGTH;
      }
      return false;
    }

    // Aranet4 has no device type byte, so its flags and trailer start one byte earlier
    const bool is_aranet4 = data.size() == ARANET4_DATA_LENGTH;
    if ((data[is_aranet4 ? 0 : 1] & 0x20) == 0) {
      if ((this->warned_ & WARNED_INTEGRATION_DISABLED) == 0) {
        ESP_LOGW(TAG, "Smart Home integration is disabled on the Aranet device");
        this->warned_ |= WARNED_INTEGRATION_DISABLED;
      }
      return false;
    }

    if (!is_aranet4 && (data[0] == 0 || data[0] > 3)) {
      if ((this->warned_ & WARNED_UNSUPPORTED_TYPE) == 0) {
        ESP_LOGW(TAG, "Unsupported Aranet device type: %u", data[0]);
        this->warned_ |= WARNED_UNSUPPORTED_TYPE;
      }
      return false;
    }
    const auto device_type =
        is_aranet4 ? AranetDeviceType::ARANET_DEVICE_TYPE_ARANET4 : static_cast<AranetDeviceType>(data[0]);

    // Battery, measurement interval, measurement age and measurement counter
    const uint8_t *trailer = &data[is_aranet4 ? 15 : 17];
    const uint16_t measurement_interval = encode_uint16(trailer[3], trailer[2]);
    const uint16_t measurement_age = encode_uint16(trailer[5], trailer[4]);
    const uint8_t measurement_counter = trailer[6];

    PUBLISH_STATE(this->measurement_age_sensor_, measurement_age);
    PUBLISH_STATE(this->signal_strength_sensor_, device.get_rssi());

    if (measurement_counter == this->last_measurement_counter_)
      return true;
    this->last_measurement_counter_ = measurement_counter;

    // Not meaningful for Aranet Radiation, which stores its dose duration in these bytes
    const float temperature = encode_uint16(data[11], data[10]) / 20.0f;
    const float pressure = encode_uint16(data[13], data[12]) / 10.0f;
    switch (device_type) {
      case AranetDeviceType::ARANET_DEVICE_TYPE_ARANET4: {
        const uint16_t co2 = encode_uint16(data[9], data[8]);
        PUBLISH_STATE(this->co2_sensor_, co2);
        PUBLISH_STATE(this->temperature_sensor_, temperature);
        PUBLISH_STATE(this->pressure_sensor_, pressure);
        PUBLISH_STATE(this->humidity_sensor_, data[14]);
        ESP_LOGD(TAG, "Aranet4 CO2: %u ppm, temperature: %.1f C, humidity: %u%%, pressure: %.1f hPa", co2, temperature,
                 data[14], pressure);
        break;
      }
      case AranetDeviceType::ARANET_DEVICE_TYPE_ARANET2: {
        const float humidity = encode_uint16(data[15], data[14]) / 10.0f;
        PUBLISH_STATE(this->temperature_sensor_, temperature);
        PUBLISH_STATE(this->humidity_sensor_, humidity);
        ESP_LOGD(TAG, "Aranet2 temperature: %.1f C, humidity: %.1f%%", temperature, humidity);
        break;
      }
      case AranetDeviceType::ARANET_DEVICE_TYPE_RADIATION: {
        const uint32_t radiation_total = encode_uint32(data[9], data[8], data[7], data[6]);
        const uint32_t radiation_duration = encode_uint32(data[13], data[12], data[11], data[10]);
        const uint16_t radiation_rate = encode_uint16(data[15], data[14]);
        PUBLISH_STATE(this->radiation_rate_sensor_, radiation_rate / 1000.0f);
        PUBLISH_STATE(this->radiation_total_sensor_, radiation_total);
        PUBLISH_STATE(this->radiation_duration_sensor_, radiation_duration);
        ESP_LOGD(TAG, "Aranet Radiation dose rate: %u nSv/h, total dose: %" PRIu32 " nSv", radiation_rate,
                 radiation_total);
        break;
      }
      case AranetDeviceType::ARANET_DEVICE_TYPE_RADON: {
        const uint16_t radon = encode_uint16(data[9], data[8]);
        const float humidity = encode_uint16(data[15], data[14]) / 10.0f;
        PUBLISH_STATE(this->radon_sensor_, radon);
        // Aranet Radon One uses zero temperature and humidity to indicate fields not supported by that model.
        if (temperature != 0)
          PUBLISH_STATE(this->temperature_sensor_, temperature);
        PUBLISH_STATE(this->pressure_sensor_, pressure);
        if (humidity != 0)
          PUBLISH_STATE(this->humidity_sensor_, humidity);
        ESP_LOGD(TAG, "Aranet Radon: %u Bq/m3, temperature: %.1f C, humidity: %.1f%%, pressure: %.1f hPa", radon,
                 temperature, humidity, pressure);
        break;
      }
      default:
        break;
    }
    PUBLISH_STATE(this->battery_level_sensor_, trailer[0]);
    PUBLISH_STATE(this->measurement_interval_sensor_, measurement_interval);

    return true;
  }

  return false;
}

}  // namespace esphome::aranet
