#include "parasite.h"
#include "esphome/core/log.h"

// #ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace parasite {

static const char *TAG = "parasite";

void Parasite::dump_config() {
  ESP_LOGCONFIG(TAG, "Parasite");
  LOG_SENSOR("  ", "Humidity", this->humidity_);
}

bool Parasite::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (const esp32_ble_tracker::ServiceData &service_data : device.get_service_datas()) {
    ESP_LOGD(TAG, "Service data:");
    for (const uint8_t byte : service_data.data) {
      ESP_LOGD(TAG, "0x%02x", byte);
    }
    const auto &data = service_data.data;
    uint16_t raw_moisture = data[0] << 8 | data[1];
    ESP_LOGD(TAG, "Moisture: %u", raw_moisture);
  }
  // TODO(rbaron): publish value (for MQTT).
  // this->humidity_->publish_state();
  // success = true;

  return true;
}

}  // namespace parasite
}  // namespace esphome

// #endif
