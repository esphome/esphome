#include "b_parasite.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace b_parasite {

static const char *const TAG = "b_parasite";

void BParasite::dump_config() {
  ESP_LOGCONFIG(TAG, "b_parasite");
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Soil Moisture", this->soil_moisture_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
}

bool BParasite::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());
  const auto &service_datas = device.get_service_datas();
  if (service_datas.size() != 1) {
    ESP_LOGE(TAG, "Unexpected service_datas size (%d)", service_datas.size());
    return false;
  }
  const auto &service_data = service_datas[0];

  ESP_LOGVV(TAG, "Service data:");
  for (const uint8_t byte : service_data.data) {
    ESP_LOGVV(TAG, "0x%02x", byte);
  }

  const auto &data = service_data.data;

  const uint8_t protocol_version = data[0] >> 4;
  if (protocol_version != 1 && protocol_version != 2) {
    ESP_LOGE(TAG, "Unsupported protocol version: %u", protocol_version);
    return false;
  }

  // Some b-parasite versions have an (optional) illuminance sensor.
  bool has_illuminance = data[0] & 0x1;

  // Counter for deduplicating messages.
  uint8_t counter = data[1] & 0x0f;
  if (last_processed_counter_ == counter) {
    ESP_LOGVV(TAG, "Skipping already processed counter (%u)", counter);
    return false;
  }

  // Battery voltage in millivolts.
  uint16_t battery_millivolt = data[2] << 8 | data[3];
  float battery_voltage = battery_millivolt / 1000.0f;

  // Temperature in 1000 * Celsius (protocol v1) or 100 * Celsius (protocol v2).
  float temp_celsius;
  if (protocol_version == 1) {
    uint16_t temp_millicelsius = data[4] << 8 | data[5];
    temp_celsius = temp_millicelsius / 1000.0f;
  } else {
    int16_t temp_centicelsius = data[4] << 8 | data[5];
    temp_celsius = temp_centicelsius / 100.0f;
  }

  // Relative air humidity in the range [0, 2^16).
  uint16_t humidity = data[6] << 8 | data[7];
  float humidity_percent = (100.0f * humidity) / (1 << 16);

  // Relative soil moisture in [0 - 2^16).
  uint16_t soil_moisture = data[8] << 8 | data[9];
  float moisture_percent = (100.0f * soil_moisture) / (1 << 16);

  // Ambient light in lux.
  float illuminance = has_illuminance ? data[16] << 8 | data[17] : 0.0f;

  if (battery_voltage_ != nullptr) {
    battery_voltage_->publish_state(battery_voltage);
  }
  if (temperature_ != nullptr) {
    temperature_->publish_state(temp_celsius);
  }
  if (humidity_ != nullptr) {
    humidity_->publish_state(humidity_percent);
  }
  if (soil_moisture_ != nullptr) {
    soil_moisture_->publish_state(moisture_percent);
  }
  if (illuminance_ != nullptr) {
    if (has_illuminance) {
      illuminance_->publish_state(illuminance);
    } else {
      ESP_LOGE(TAG, "No lux information is present in the BLE packet");
    }
  }

  last_processed_counter_ = counter;
  return true;
}

}  // namespace b_parasite
}  // namespace esphome

#endif  // USE_ESP32
