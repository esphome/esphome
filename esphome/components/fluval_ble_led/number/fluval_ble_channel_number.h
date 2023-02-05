#pragma once

#include "esphome/core/component.h"
#include "esphome/components/number/number.h"
#include "esphome/components/fluval_ble_led/fluval_ble_led.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

class FluvalBleChannelNumber : public number::Number, public Component, public FluvalLedClient {
public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; };

  void set_zero_if_off(bool zero_if_off) { this->zero_if_off_ = zero_if_off; }
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void notify() override;
protected:
  void control(float value) override;
  uint8_t channel_;
  bool zero_if_off_;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
