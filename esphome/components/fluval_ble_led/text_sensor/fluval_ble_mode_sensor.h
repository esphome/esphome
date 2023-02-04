#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/fluval_ble_led/fluval_ble_led.h"

#ifdef USE_ESP32

namespace esphome {
namespace fluval_ble_led {

class FluvalBleModeSensor : public text_sensor::TextSensor, public Component, public FluvalLedClient {
  public:
    void setup() override;
    void loop() override {}
    float get_setup_priority() { return setup_priority::DATA; };
    void dump_config() override;
    void notify() override;
  
    void set_manual_mapping(const char* manual_mapping) { this->manual_mapping_ = manual_mapping; }
    void set_auto_mapping(const char* auto_mapping) { this->auto_mapping_ = auto_mapping; }
    void set_pro_mapping(const char* pro_mapping) { this->pro_mapping_ = pro_mapping; }

  private:
    const char* manual_mapping_;
    const char* auto_mapping_;
    const char* pro_mapping_;
};

}  // namespace fluval_ble_led
}  // namespace esphome

#endif
