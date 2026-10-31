#pragma once

#include "esphome/components/cia402/cia402.h"

namespace esphome::tmcm1240 {

class TMCM1240 : public cia402::CiA402Drive {
 public:
  using CiA402Drive::CiA402Drive;
  void dump_config() override;
  bool apply_configuration();
  static uint8_t current_register(float amps);
  void set_run_current(float amps) { this->run_current_ = current_register(amps); }
  void set_standby_current(float amps) { this->standby_current_ = current_register(amps); }
  void set_stallguard_threshold(int8_t value) { this->stallguard_threshold_ = value; }
  void set_stallguard_min_velocity(uint32_t value) { this->stallguard_min_velocity_ = value; }

 protected:
  void append_configuration_();
  void append_profile() override;
  uint32_t stallguard_min_velocity_{0};
  int8_t stallguard_threshold_{0};
  uint8_t run_current_{0};
  uint8_t standby_current_{0};
};

}  // namespace esphome::tmcm1240
