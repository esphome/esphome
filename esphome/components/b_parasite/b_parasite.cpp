#include "b_parasite.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::b_parasite {

ESPHOME_LOG_TAG(TAG, "b_parasite");

void BParasite::dump_config() {
  ESP_LOGCONFIG(TAG, "b_parasite");
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Humidity", this->humidity_sensor_);
  LOG_SENSOR("  ", "Soil Moisture", this->soil_moisture_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
}

bool BParasite::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str_to(addr_buf));
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

  if (data.size() < 10) {
    ESP_LOGW(TAG, "Service data too short: %zu", data.size());
    return false;
  }

  const uint8_t protocol_version = data[0] >> 4;
  if (protocol_version != 1 && protocol_version != 2) {
    ESP_LOGE(TAG, "Unsupported protocol version: %u", protocol_version);
    return false;
  }

  // Some b-parasite versions have an (optional) illuminance sensor.
  bool has_illuminance = data[0] & 0x1;

  if (has_illuminance && data.size() < 18) {
    ESP_LOGW(TAG, "Service data too short for illuminance: %zu", data.size());
    return false;
  }

  // Counter for deduplicating messages.
  uint8_t counter = data[1] & 0x0f;
  if (last_processed_counter_ == counter) {
    ESP_LOGVV(TAG, "Skipping already processed counter (%u)", counter);
    return false;
  }

  // Battery voltage in millivolts.
  uint16_t battery_millivolt = encode_value<uint16_t>(&data[2]);
  float battery_voltage = battery_millivolt / 1000.0f;

  // Temperature in 1000 * Celsius (protocol v1) or 100 * Celsius (protocol v2).
  float temp_celsius;
  if (protocol_version == 1) {
    uint16_t temp_millicelsius = encode_value<uint16_t>(&data[4]);
    temp_celsius = temp_millicelsius / 1000.0f;
  } else {
    int16_t temp_centicelsius = encode_value<int16_t>(&data[4]);
    temp_celsius = temp_centicelsius / 100.0f;
  }

  // Relative air humidity in the range [0, 2^16).
  uint16_t humidity = encode_value<uint16_t>(&data[6]);
  float humidity_percent = (100.0f * humidity) / (1 << 16);

  // Relative soil moisture in [0 - 2^16).
  uint16_t soil_moisture = encode_value<uint16_t>(&data[8]);
  float moisture_percent = (100.0f * soil_moisture) / (1 << 16);

  // Ambient light in lux.
  float illuminance = has_illuminance ? encode_value<uint16_t>(&data[16]) : 0.0f;

  if (this->battery_voltage_sensor_ != nullptr) {
    this->battery_voltage_sensor_->publish_state(battery_voltage);
  }
  if (this->temperature_sensor_ != nullptr) {
    this->temperature_sensor_->publish_state(temp_celsius);
  }
  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->publish_state(humidity_percent);
  }
  if (this->soil_moisture_sensor_ != nullptr) {
    this->soil_moisture_sensor_->publish_state(moisture_percent);
  }
  if (this->illuminance_sensor_ != nullptr) {
    if (has_illuminance) {
      this->illuminance_sensor_->publish_state(illuminance);
    } else {
      ESP_LOGE(TAG, "No lux information is present in the BLE packet");
    }
  }

  last_processed_counter_ = counter;
  return true;
}

}  // namespace esphome::b_parasite
