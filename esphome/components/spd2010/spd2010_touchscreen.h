#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/touchscreen/touchscreen.h"

namespace esphome::spd2010 {

class SPD2010Touchscreen final : public touchscreen::Touchscreen, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void set_interrupt_pin(InternalGPIOPin *pin) { this->interrupt_pin_ = pin; }
  void set_reset_pin(GPIOPin *pin) { this->reset_pin_ = pin; }

 protected:
  void initialize_();
  void update_touches() override;
  bool read_data_();
  bool read_register_(uint16_t reg, uint8_t *data, size_t length);
  bool write_command_(uint16_t reg, uint16_t value);
  bool start_controller_(uint8_t status);
  bool finish_report_();

  InternalGPIOPin *interrupt_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
  uint8_t setup_attempts_{0};
  bool ready_{false};
};

}  // namespace esphome::spd2010
