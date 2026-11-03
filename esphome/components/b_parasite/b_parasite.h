#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"

namespace esphome::b_parasite {

class BParasite final : public Component, public ble_device_base::ESPBTDeviceListener {
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(soil_moisture)
  SUB_SENSOR(illuminance)

 public:
  void set_address(uint64_t address) { address_ = address; };
  void set_bindkey(const std::string &bindkey);

  bool parse_device(const ble_device_base::ESPBTDevice &device) override;
  void dump_config() override;

 protected:
  // The received advertisement packet contains an unsigned 4 bits wrap-around counter
  // for deduplicating messages.
  int8_t last_processed_counter_ = -1;
  uint64_t address_;
};

}  // namespace esphome::b_parasite
