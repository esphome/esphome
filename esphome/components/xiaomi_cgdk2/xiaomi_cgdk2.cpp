#include "xiaomi_cgdk2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_cgdk2 {

static const char *const TAG = "xiaomi_cgdk2";

static constexpr size_t CGDK2_BINDKEY_SIZE = 16;

void XiaomiCGDK2::dump_config() {
  char bindkey_hex[format_hex_pretty_size(CGDK2_BINDKEY_SIZE)];
  ESP_LOGCONFIG(TAG,
                "Xiaomi CGDK2\n"
                "  Bindkey: %s",
                format_hex_pretty_to(bindkey_hex, this->bindkey_, CGDK2_BINDKEY_SIZE, '.'));
  LOG_SENSOR("  ", "Temperature", this->temperature_);
  LOG_SENSOR("  ", "Humidity", this->humidity_);
  LOG_SENSOR("  ", "Battery Level", this->battery_level_);
}

bool XiaomiCGDK2::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
  if (device.address_uint64() != this->address_) {
    ESP_LOGVV(TAG, "parse_device(): unknown MAC address.");
    return false;
  }
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  const char *addr_str = device.address_str_to(addr_buf);
  ESP_LOGV(TAG, "parse_device(): MAC address %s found.", addr_str);

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
    if (res->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*res->temperature);
    if (res->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*res->humidity);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    success = true;
  }

  return success;
}

void XiaomiCGDK2::set_bindkey(const char *bindkey) { parse_hex(bindkey, this->bindkey_, sizeof(this->bindkey_)); }

}  // namespace xiaomi_cgdk2
}  // namespace esphome

#endif
