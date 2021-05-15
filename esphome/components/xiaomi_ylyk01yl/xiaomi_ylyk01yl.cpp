#include "xiaomi_ylyk01yl.h"
#include "esphome/core/log.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_ylyk01yl {

static const char *TAG = "xiaomi_ylyk01yl";

void XiaomiYLYK01YL::dump_config() {
  ESP_LOGCONFIG(TAG, "Xiaomi YLYK01YL");
  LOG_SENSOR("  ", "Keycode", this->keycode_);
}

bool XiaomiYLYK01YL::parse_device(const esp32_ble_tracker::ESPBTDevice &device) {
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
    if (res->keycode.has_value()) {
      if(this->keycode_ != nullptr)
        this->keycode_->publish_state(*res->keycode);

      this->receive_callback_.call(*res->keycode);
    }
    success = true;
  }

  if (!success) {
    return false;
  }

  return true;
}

void XiaomiYLYK01YL::add_on_receive_callback(std::function<void(int)> &&callback) {
  this->receive_callback_.add(std::move(callback));
}

}  // namespace xiaomi_ylyk01yl
}  // namespace esphome

#endif
