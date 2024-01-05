#include "xiaomi_hhccjcy10.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_hhccjcy10 {

static const char *const TAG = "xiaomi_hhccjcy10";

void XiaomiHHCCJCY10::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCJCY10");
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool XiaomiHHCCJCY10::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    if (!service_data.uuid.contains(0x50, 0xFD)) {
      ESP_LOGVV(TAG, "no tuya service data UUID.");
      continue;
    }
    if (service_data.data.size() != 9) {  // tuya alternate between two service data
      continue;
    }
    const uint8_t *data = service_data.data.data();

    if (this->temperature_ != nullptr) {
      const int16_t temperature = encode_uint16(data[1], data[2]);
      this->temperature_->publish_state((float) temperature / 10.0f);
    }

    if (this->moisture_ != nullptr)
      this->moisture_->publish_state(data[0]);

    if (this->conductivity_ != nullptr) {
      const uint16_t conductivity = encode_uint16(data[7], data[8]);
      this->conductivity_->publish_state((float) conductivity);
    }

    if (this->illuminance_ != nullptr) {
      const uint32_t illuminance = encode_uint24(data[3], data[4], data[5]);
      this->illuminance_->publish_state((float) illuminance);
    }

    if (this->battery_level_ != nullptr)
      this->battery_level_->publish_state(data[6]);
    success = true;
  }

  return success;
}

}  // namespace xiaomi_hhccjcy10
}  // namespace esphome

#endif
