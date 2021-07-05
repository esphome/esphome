#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace hcs301 {

// Configuration Struct
struct HCSConfig {
  bool auto_shutoff = true;
  bool bsl0 = false;
  bool bsl1 = false;
  bool voltage_low = false;
  bool overflow_0 = false;
  bool overflow_1 = false;
  bool envelope_encryption = false;
};

enum class HCS301_Voltage {
  Vcc_9_12,
  Vcc_6,
};

class HCS301 : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_power_pin(GPIOPin *power_pin) { this->power_pin_ = power_pin; };
  void set_clock_pin(GPIOPin *clock_pin) { this->clock_pin_ = clock_pin; };
  void set_pwm_pin(GPIOPin *pwm_pin) { this->pwm_pin_ = pwm_pin; };
  void set_hcs301_voltage(const HCS301_Voltage voltage) { this->hcs301_voltage_ = voltage; }
  bool program(uint32_t serial, uint16_t sequence, uint64_t key);
  void transmitS2();

 protected:
  void config_eeprom_buffer_(uint16_t *eeprom_buffer, uint64_t key, uint16_t sync, uint32_t serial, uint32_t seed, struct HCSConfig config);
  void write_eeprom_buffer(uint16_t *eeprom_buffer, uint16_t *vbuffer);

  GPIOPin *power_pin_{nullptr}; 
  GPIOPin *clock_pin_{nullptr}; 
  GPIOPin *pwm_pin_{nullptr};
  HCS301_Voltage hcs301_voltage_;
};


}  // namespace hcs301
}  // namespace esphome