#pragma once

#include "esphome/core/component.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/one_wire/one_wire.h"

namespace esphome {
namespace dallas_ibutton {

class DallasIbuttonComponent : public text_sensor::TextSensor, public PollingComponent, public one_wire::OneWireDevice {
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

  /// @brief The time after which the sensor value will be reset
  void set_reset_value_after(uint32_t reset_value_after) { this->reset_value_after_ = reset_value_after; }

  const char *get_device_type(uint8_t family_code);

 protected:
  uint32_t reset_value_after_;

 private:
  char last_address_[17];
  uint32_t last_timestamp_;
};

}  // namespace dallas_ibutton
}  // namespace esphome
