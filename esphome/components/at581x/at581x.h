#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#include "esphome/components/i2c/i2c.h"

namespace esphome::at581x {

class AT581XComponent final : public Component, public i2c::I2CDevice {
 public:
#ifdef USE_SWITCH
  SUB_SWITCH(rf_power)
#endif

  void setup() override;
  void dump_config() override;

  // Frequency in MHz and power consumption in µA, as listed in the datasheet; other values fail the next write
  void set_frequency(int frequency);
  void set_power_consumption(int value);
  void set_sensing_distance(int distance) { this->delta_ = 1023 - distance; }
  void set_poweron_selfcheck_time(int value) { this->self_check_time_ms_ = value; }
  void set_protect_time(int value) { this->protect_time_ms_ = value; }
  void set_trigger_base(int value) { this->trigger_base_time_ms_ = value; }
  void set_trigger_keep(int value) { this->trigger_keep_time_ms_ = value; }
  void set_stage_gain(int value) { this->gain_ = value; }

  bool i2c_write_config();
  bool reset_hardware_frontend();
  void reset_hardware_frontend_if(bool reset) {
    if (reset)
      this->reset_hardware_frontend();
  }
  bool set_rf_mode(bool enable);

 protected:
  bool write_le_(uint8_t addr, uint32_t value, uint8_t size);

  // Defaults match the at581x.settings defaults
  uint16_t self_check_time_ms_{2000};
  uint16_t protect_time_ms_{1000};
  uint16_t trigger_base_time_ms_{500};
  uint16_t trigger_keep_time_ms_{1500};
  uint16_t delta_{1023 - 823};  // 0 to 1023, the larger the value, the shorter the distance
  uint8_t freq_index_{6};       // 5800 MHz
  uint8_t power_index_{3};      // 70 µA
  uint8_t gain_{3};
};

}  // namespace esphome::at581x
