#include "xiaomi_mjyd02yla.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_mjyd02yla {

static const char *const TAG = "xiaomi_mjyd02yla";

void XiaomiMJYD02YLA::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi MJYD02YL-A");
  LOG_BINARY_SENSOR("  ", "Motion", this);
  LOG_BINARY_SENSOR("  ", "Light", this->is_light_);
  LOG_SENSOR("  ", "Idle Time", this->idle_time_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
  LOG_SENSOR("  ", "Illuminance", this->illuminance_);
}

bool XiaomiMJYD02YLA::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->idle_time.has_value() && this->idle_time_ != nullptr)
      this->idle_time_->publish_state(*res->idle_time);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    if (res->illuminance.has_value() && this->illuminance_ != nullptr)
      this->illuminance_->publish_state(*res->illuminance);
    if (res->is_light.has_value() && this->is_light_ != nullptr)
      this->is_light_->publish_state(*res->is_light);
    if (res->has_motion.has_value())
      this->publish_state(*res->has_motion);
    success = true;
  }

  return success;
}

void XiaomiMJYD02YLA::set_bindkey(const std::string &bindkey) {
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

}  // namespace xiaomi_mjyd02yla
}  // namespace esphome

#endif
