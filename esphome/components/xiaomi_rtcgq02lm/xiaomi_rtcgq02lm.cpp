#include "xiaomi_rtcgq02lm.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_rtcgq02lm {

static const char *const TAG = "xiaomi_rtcgq02lm";

void XiaomiRTCGQ02LM::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi RTCGQ02LM");
  ESP_LOGCONFIG(TAG, "  Bindkey: %s", format_hex_pretty(this->bindkey_, 16).c_str());
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Motion", this->motion_);
  LOG_BINARY_SENSOR("  ", "Light", this->light_);
  LOG_BINARY_SENSOR("  ", "Button", this->button_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
#endif
}

bool XiaomiRTCGQ02LM::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->has_encryption &&
        (!(xiaomi_ble::decrypt_xiaomi_payload(const_cast<std::vector<uint8_t> &>(service_data.data), this->bindkey_,
                                              this->address_)))) {
      continue;
    }
    if (!(xiaomi_ble::parse_xiaomi_message(service_data.data, *res))) {
      continue;
    }

    if (!(xiaomi_ble::report_xiaomi_results(res, device.address_str()))) {
      continue;
    }
#ifdef USE_BINARY_SENSOR
    if (res->has_motion.has_value() && this->motion_ != nullptr) {
      this->motion_->publish_state(*res->has_motion);
      this->set_timeout("motion_timeout", this->motion_timeout_,
                        [this, res]() { this->motion_->publish_state(false); });
    }
    if (res->is_light.has_value() && this->light_ != nullptr)
      this->light_->publish_state(*res->is_light);
    if (res->button_press.has_value() && this->button_ != nullptr) {
      this->button_->publish_state(*res->button_press);
      this->set_timeout("button_timeout", this->button_timeout_,
                        [this, res]() { this->button_->publish_state(false); });
    }
#endif
#ifdef USE_SENSOR
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
#endif
    success = true;
  }

  return success;
}

void XiaomiRTCGQ02LM::set_bindkey(const std::string &bindkey) {
  memset(bindkey_, 0, 16);
  if (bindkey.size() != 32) {
    return;
  }
  char temp[3] = {0};
  for (int i = 0; i < 16; i++) {
    strncpy(temp, &(bindkey.c_str()[i * 2]), 2);
    bindkey_[i] = std::strtoul(temp, nullptr, 16);
  }
}

}  // namespace xiaomi_rtcgq02lm
}  // namespace esphome

#endif
