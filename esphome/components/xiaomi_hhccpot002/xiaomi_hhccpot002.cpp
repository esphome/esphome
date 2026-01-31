#include "xiaomi_hhccpot002.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome {
namespace xiaomi_hhccpot002 {

static const char *const TAG = "xiaomi_hhccpot002";

void XiaomiHHCCPOT002 ::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi HHCCPOT002");
  LOG_SENSOR("  ", "Moisture", this->moisture_);
  LOG_SENSOR("  ", "Conductivity", this->conductivity_);
}

bool XiaomiHHCCPOT002::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->has_encryption) {
      ESP_LOGVV(TAG, "parse_device(): payload decryption is currently not supported on this device.");
      continue;
    }
    if (!(xiaomi_ble::parse_xiaomi_message(service_data.data, *res))) {
      continue;
    }
    if (!(xiaomi_ble::report_xiaomi_results(res, addr_str))) {
      continue;
    }
    if (res->moisture.has_value() && this->moisture_ != nullptr)
      this->moisture_->publish_state(*res->moisture);
    if (res->conductivity.has_value() && this->conductivity_ != nullptr)
      this->conductivity_->publish_state(*res->conductivity);
    success = true;
  }

  return success;
}

}  // namespace xiaomi_hhccpot002
}  // namespace esphome

#endif
