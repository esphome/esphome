#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/fluval_ble_led/fluval_ble_led.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

class FluvalBleChannelSensor : public sensor::Sensor, public Component, public FluvalLedClient {
 public:
  void setup() override;
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_zero_if_off(bool zero_if_off) { this->zero_if_off_ = zero_if_off; }
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };
  void notify() override;

 protected:
  uint8_t channel_;
  bool zero_if_off_;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
