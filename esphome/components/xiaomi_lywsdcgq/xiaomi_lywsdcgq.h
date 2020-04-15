#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"
#include "esphome/components/xiaomi_ble/xiaomi_ble.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {
namespace xiaomi_lywsdcgq {

class XiaomiLYWSDCGQ : public Component, public esp32_ble_tracker::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { address_ = address; }

  bool parse_device(const esp32_ble_tracker::ESPBTDevice &device) override {
    if (device.address_uint64() != this->address_)
      return false;

    auto res = xiaomi_ble::parse_xiaomi(device);
    if (!res.has_value()){
      ESP_LOGVV(TAG, "Couldn't parse XIAOMI parse_xiaomi_header");
      return true; //seems wrong? We have the correct address, therefore we want to stop other processing
    }

    if ((device.get_service_data().size() < 14) || !res->has_data ) { //Not 100% sure on the sizing here. Might not be necessary any more
      ESP_LOGVV(TAG, "Xiaomi service data too short or missing");
      return true; //seems wrong? We have the correct address, therefore we want to stop other processing
    }
    uint8_t* message;
    if((*res).has_encryption){
      message=new uint8_t((*res).data_length); //I don't think we have a real length available at this point
      xiaomi_ble::decrypt_message(device,*res,(const uint8_t*)this->bindkey_,message);
    }else {
      message=(uint8_t*)reinterpret_cast<const uint8_t *>(device.get_service_data().data());
    }

    xiaomi_ble::parse_xiaomi_message(message,*res);
    if (!res.has_value())
      return false;

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
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *humidity_{nullptr};
  sensor::Sensor *battery_level_{nullptr};
};

}  // namespace xiaomi_lywsdcgq
}  // namespace esphome

#endif
