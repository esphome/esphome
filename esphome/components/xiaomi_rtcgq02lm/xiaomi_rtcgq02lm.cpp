#include "xiaomi_rtcgq02lm.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_rtcgq02lm {

static const char *const TAG = "xiaomi_rtcgq02lm";

static constexpr size_t RTCGQ02LM_BINDKEY_SIZE = 16;

void XiaomiRTCGQ02LM::dump_config() {
  char bindkey_hex[format_hex_pretty_size(RTCGQ02LM_BINDKEY_SIZE)];
  ESP_LOGCONFIG(TAG, "Xiaomi RTCGQ02LM");
  ESP_LOGCONFIG(TAG, "  Bindkey: %s", format_hex_pretty_to(bindkey_hex, this->bindkey_, RTCGQ02LM_BINDKEY_SIZE, '.'));
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
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  const char *addr_str = device.address_str_to(addr_buf);
  ESP_LOGVV(TAG, "parse_device(): MAC address %s found.", addr_str);

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

    if (!(xiaomi_ble::report_xiaomi_results(res, addr_str))) {
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

void XiaomiRTCGQ02LM::set_bindkey(const char *bindkey) { parse_hex(bindkey, this->bindkey_, sizeof(this->bindkey_)); }

}  // namespace xiaomi_rtcgq02lm
}  // namespace esphome

#endif
