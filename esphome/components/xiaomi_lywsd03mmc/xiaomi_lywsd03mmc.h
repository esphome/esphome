#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"
#include "esphome/core/log.h"
#include <stdlib.h>
#include <string>
#include <sstream>

template<class T> inline std::string to_string(const uint64_t &t) {
  std::stringstream ss;
  ss << t;
  return ss.str();
}

#ifdef ARDUINO_ARCH_ESP32
static const char *TAG = "xiaomi_lywsd03mmc";

namespace esphome {
namespace xiaomi_lywsd03mmc {

class XiaomiLYWSD03MMC : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_bindkey(const std::string &bindkey) { /*bindkey_ = std::string(bindkey); */
    char temp[3];
    for (int i = 0; i < 16; i++) {
      strncpy(temp, &(bindkey.c_str()[i * 2]), 2);
      bindkey_[i] = std::strtoul(temp, NULL, 16);
    }
  }

  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    ESP_LOGVV(TAG, "Got device packet");

    if (device.address_uint64() != this->address_) {
      ESP_LOGVV(TAG, "Address didn't match");
      ESP_LOGVV(TAG, to_string((this->address_)).c_str());
      ESP_LOGVV(TAG, to_string((device.address_uint64())).c_str());
      return false;  // Address didn't match, so send false to allow for other listeners
    }
    ESP_LOGVV(TAG, "Address matched matched matched");

    /*optional<xiaomi_ble::XiaomiParseResult>*/
    auto res = xiaomi_ble::parse_xiaomi_header(device);

    if (!res.has_value()) {
      ESP_LOGVV(TAG, "Couldn't parse XIAOMI parse_xiaomi_header");
      return true;  // seems wrong? We have the correct address, therefore we want to stop other processing
    }

    if ((device.get_service_data().size() < 14) ||
        !res->has_data) {  // Not 100% sure on the sizing here. Might not be necessary any more
      ESP_LOGVV(TAG, "Xiaomi service data too short or missing");
      return true;  // seems wrong? We have the correct address, therefore we want to stop other processing
    }
    uint8_t *message;
    if ((*res).has_encryption) {
      message = new uint8_t((*res).data_length);  // I don't think we have a real length available at this point
      xiaomi_ble::decrypt_message(device, *res, (const uint8_t *) this->bindkey_, message);
    } else {
      message = (uint8_t *) reinterpret_cast<const uint8_t *>(device.get_service_data().data());
    }

    xiaomi_ble::parse_xiaomi_message(message, *res);
    if (!res.has_value()) {
      ESP_LOGVV(TAG, "Couldn't parse XIAOMI parse_xiaomi_message");
      return true;  // seems wrong? We have the correct address, therefore we want to stop other processing
    }

    ESP_LOGVV(TAG, "Completed parse of XIAOMI message");
    if (res->temperature.has_value() && this->temperature_ != nullptr)
      this->temperature_->publish_state(*res->temperature);
    if (res->humidity.has_value() && this->humidity_ != nullptr)
      this->humidity_->publish_state(*res->humidity);
    if (res->battery_level.has_value() && this->battery_level_ != nullptr)
      this->battery_level_->publish_state(*res->battery_level);
    return true;
  }

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_humidity(sensor::Sensor *humidity) { humidity_ = humidity; }
  void set_battery_level(sensor::Sensor *battery_level) { battery_level_ = battery_level; }

 protected:
  uint64_t address_;
  uint8_t bindkey_[16];
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace xiaomi_lywsd03mmc
}  // namespace esphome

#endif
