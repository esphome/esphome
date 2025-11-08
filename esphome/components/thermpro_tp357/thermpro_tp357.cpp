#include "thermpro_tp357.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace thermpro_tp357 {

static const char *const TAG = "thermpro_tp357";

void ThermProTP357::dump_config() {
  ESP_LOGCONFIG(TAG, "ThermoPro TP357");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool ThermProTP357::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }

  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_manufacturer_datas()) {
    // first uuid byte is magic, second is temp low byte
    esp_bt_uuid_t uuid = service_data.uuid.get_uuid();
    const uint8_t *data = service_data.data.data();

    if (uuid.uuid.uuid128[0] != 0xc2) {
      ESP_LOGVV(TAG, "parse_device(): invalid service data UUID magic byte.");
      continue;
    }

    if (service_data.data.size() != 5) {
      ESP_LOGVV(TAG, "parse_device(): payload has wrong size (%d)!", service_data.data.size());
      continue;
    }

    ESP_LOGD(TAG, "Got ThermPro TP357 (%s):", device.address_str().c_str());

    // temperature, 2 bytes, 16-bit signed integer, 0.1 °C
    float temperature = float(uint16_t(uuid.uuid.uuid128[1]) | (uint16_t(data[0]) << 8)) * 0.1f;
    ESP_LOGD(TAG, "  Temperature: %.1f °C", temperature);
    if (this->temperature_ != nullptr)
      this->temperature_->publish_state(temperature);

    // humidity, 1 byte, 8-bit unsigned integer, 1.0 %
    float humidity = float(data[1]);
    ESP_LOGD(TAG, "  Humidity: %.0f %%", humidity);
    if (this->humidity_ != nullptr)
      this->humidity_->publish_state(humidity);

    // battery level, 2 bits (0-2)
    float battery_level = float(data[2] & 0x3) * 50.0;
    ESP_LOGD(TAG, "  Battery Level: %.0f %%", battery_level);
    if (this->battery_level_ != nullptr)
      this->battery_level_->publish_state(battery_level);

    success = true;
  }

  float signal_strength = float(device.get_rssi());
  ESP_LOGD(TAG, "  Signal Strength: %.0f dBm", signal_strength);
  if (this->signal_strength_ != nullptr)
    this->signal_strength_->publish_state(signal_strength);

  return success;
}

}  // namespace thermpro_tp357
}  // namespace esphome

#endif
