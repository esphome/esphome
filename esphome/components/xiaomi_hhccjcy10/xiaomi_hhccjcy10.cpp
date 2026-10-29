#include "xiaomi_hhccjcy10.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::xiaomi_hhccjcy10 {

ESPHOME_LOG_TAG(TAG, "xiaomi_hhccjcy10");

void XiaomiHHCCJCY10::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCJCY10");
  LOG_SENSOR("  ", "Temperature", this->temperature_sensor_);
  LOG_SENSOR("  ", "Moisture", this->moisture_sensor_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_sensor_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_sensor_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_sensor_);
}

bool XiaomiHHCCJCY10::parse_device(const ble_device_base::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str_to(addr_buf));

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

    if (this->temperature_sensor_ != nullptr) {
      const int16_t temperature = encode_value<int16_t>(&data[1]);
      this->temperature_sensor_->publish_state((float) temperature / 10.0f);
    }

    if (this->moisture_sensor_ != nullptr)
      this->moisture_sensor_->publish_state(data[0]);

    if (this->conductivity_sensor_ != nullptr) {
      const uint16_t conductivity = encode_uint16(data[7], data[8]);
      this->conductivity_sensor_->publish_state((float) conductivity);
    }

    if (this->illuminance_sensor_ != nullptr) {
      const uint32_t illuminance = encode_uint24(data[3], data[4], data[5]);
      this->illuminance_sensor_->publish_state((float) illuminance);
    }

    if (this->battery_level_sensor_ != nullptr)
      this->battery_level_sensor_->publish_state(data[6]);
    success = true;
  }

  return success;
}

}  // namespace esphome::xiaomi_hhccjcy10
