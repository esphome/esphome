#include "b_parasite.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace b_parasite {

static const char *const TAG = "b_parasite";

void BParasite::dump_config() {
  ESP_LOGCONFIG(TAG, "b_parasite");
  LOG_SENSOR("  ", "Battery Voltage", this->battery_voltage_);
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Soil Moisture", this->soil_moisture_);
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

  // Counter for deduplicating messages.
  uint8_t counter = data[1] & 0x0f;
  if (last_processed_counter_ == counter) {
    ESP_LOGVV(TAG, "Skipping already processed counter (%u)", counter);
    return false;
  }

  // Battery voltage in millivolts.
  uint16_t battery_millivolt = data[2] << 8 | data[3];
  float battery_voltage = battery_millivolt / 1000.0f;

  // Temperature in 1000 * Celsius.
  uint16_t temp_millicelcius = data[4] << 8 | data[5];
  float temp_celcius = temp_millicelcius / 1000.0f;

  // Relative air humidity in the range [0, 2^16).
  uint16_t humidity = data[6] << 8 | data[7];
  float humidity_percent = (100.0f * humidity) / (1 << 16);

  // Relative soil moisture in [0 - 2^16).
  uint16_t soil_moisture = data[8] << 8 | data[9];
  float moisture_percent = (100.0f * soil_moisture) / (1 << 16);

  if (battery_voltage_ != nullptr) {
    battery_voltage_->publish_state(battery_voltage);
  }
  if (temperature_ != nullptr) {
    temperature_->publish_state(temp_celcius);
  }
  if (humidity_ != nullptr) {
    humidity_->publish_state(humidity_percent);
  }
  if (soil_moisture_ != nullptr) {
    soil_moisture_->publish_state(moisture_percent);
  }

  last_processed_counter_ = counter;
  return true;
}

}  // namespace b_parasite
}  // namespace esphome

#endif  // ARDUINO_ARCH_ESP32
