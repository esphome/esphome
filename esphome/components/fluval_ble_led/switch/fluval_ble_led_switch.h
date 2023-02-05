#pragma once

#include "esphome/core/component.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/fluval_ble_led/fluval_ble_led.h"
#include <string>

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

class FluvalBleLedSwitch : public switch_::Switch, public Component, public FluvalLedClient {
  public:
    void setup() override;
    void loop() override {}
    float get_setup_priority() { return setup_priority::DATA; };
    void dump_config() override;
    void notify() override;

  protected:
    void write_state(bool state) override;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
