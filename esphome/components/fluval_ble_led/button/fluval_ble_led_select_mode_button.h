#pragma once

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"
#include "esphome/components/fluval_ble_led/fluval_ble_led.h"
#include <string>

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

class FluvalBleLedSelectModeButton : public button::Button, public Component, public FluvalLedClient {
public:
 void setup() override;
 void loop() override {}
 float get_setup_priority() { return setup_priority::DATA; };
 void dump_config() override;
 void notify() override {};
 void set_trigger_mode(uint8_t trigger_mode) { this->trigger_mode_ = trigger_mode; };

protected:
 void press_action() override;
 uint8_t trigger_mode_;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
