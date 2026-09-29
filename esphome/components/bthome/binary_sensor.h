#pragma once

#ifdef USE_BTHOME_BINARY_SENSOR

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/core/component.h"

#include "button_event.h"

namespace esphome::bthome {

class BTHomeButtonBinarySensor final : public binary_sensor::BinarySensor,
                                       public Component,
                                       public ble_device_base::ESPBTDeviceListener {
 public:
  void set_address(uint64_t address) { this->address_ = address; }
  void set_index(uint8_t index) { this->index_ = index; }
  void set_event(uint8_t event) { this->event_ = event; }
  void set_pulse_length(uint32_t pulse_length_ms) { this->pulse_length_ms_ = pulse_length_ms; }

  void setup() override;
  void dump_config() override;
  bool parse_device(const ble_device_base::ESPBTDevice &device) override;

 protected:
  uint32_t pulse_length_ms_{200};
  uint64_t address_{0};
  PacketDedup dedup_{};
  uint8_t index_{1};
  uint8_t event_{0x01};
  bool encrypted_logged_{false};
};

}  // namespace esphome::bthome

#endif
