#include "xiaomi_wx08zm.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_wx08zm {

static const char *const TAG = "xiaomi_wx08zm";

void XiaomiWX08ZM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi WX08ZM");
  LOG_BINARY_SENSOR("  ", "Mosquito Repellent", this);
  LOG_SENSOR("  ", "Tablet Resource", this->tablet_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool XiaomiWX08ZM::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", device.address_str().c_str());

  bool success = false;
  for (auto &service_data : device.get_service_datas()) {
    auto res = xiaomi_ble::parse_xiaomi_header(service_data);
    if (!res.has_value()) {
      continue;
    }
    if (res->is_duplicate) {
      continue;
    }
    if (res->has_encryption) {
      ESP_LOGVV(TAG, "parse_device(): payload decryption is currently not supported on this device.");
      continue;
    }
    if (!(xiaomi_ble::parse_xiaomi_message(service_data.data, *res))) {
      continue;
    }
    if (!(xiaomi_ble::report_xiaomi_results(res, device.address_str()))) {
      continue;
    }
    if (res->is_active.has_value()) {
      this->publish_state(*res->is_active);
    }
    if (res->tablet.has_value() && this->tablet_ != nullptr)
      this->tablet_->publish_state(*res->tablet);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    success = true;
  }

  return success;
}

}  // namespace xiaomi_wx08zm
}  // namespace esphome

#endif
