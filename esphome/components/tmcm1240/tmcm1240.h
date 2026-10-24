#pragma once

#include "esphome/components/cia402/cia402.h"

namespace esphome::tmcm1240 {

class TMCM1240 : public cia402::CiA402Drive {
 public:
  using CiA402Drive::CiA402Drive;
  void setup() override;
  void dump_config() override;
  bool apply_configuration();
  bool sensorless_home();
  bool is_homed() const { return this->homed_; }
  bool is_homing() const { return this->phase_ != Phase::PHASE_IDLE; }
  static uint8_t current_register(float amps);
  void set_run_current(float amps) { this->run_current_ = current_register(amps); }
  void set_standby_current(float amps) { this->standby_current_ = current_register(amps); }
  void set_stallguard_threshold(int8_t value) { this->stallguard_threshold_ = value; }
  void set_stallguard_min_velocity(uint32_t value) { this->stallguard_min_velocity_ = value; }
  void set_homing_velocity(uint32_t value) { this->homing_velocity_ = value; }
  void set_homing_backoff(uint32_t value) { this->homing_backoff_ = value; }
  void set_homing_travel(uint32_t value) { this->homing_travel_ = value; }
  void set_homing_margin(uint32_t value) { this->homing_margin_ = value; }

 protected:
  enum class Phase : uint8_t { PHASE_IDLE, PHASE_CONFIGURE, PHASE_BACKOFF, PHASE_SEEK, PHASE_MONITOR, PHASE_ZERO };
  void append_configuration_();
  void append_profile() override;
  bool sequence_finished(cia402::DriveResult result) override;
  void received_value(uint16_t index, uint32_t value) override;
  void monitor_();
  Phase phase_{Phase::PHASE_IDLE};
  uint32_t homing_started_ms_{0};
  uint32_t saved_velocity_{0};
  uint32_t stallguard_min_velocity_{0};
  uint32_t homing_velocity_{0};
  uint32_t homing_backoff_{0};
  uint32_t homing_travel_{0};
  uint32_t homing_margin_{0};
  uint32_t minimum_velocity_readback_{0};
  int32_t seek_target_{0};
  int32_t target_readback_{0};
  int8_t threshold_readback_{0};
  int8_t stallguard_threshold_{0};
  uint8_t run_current_{0};
  uint8_t standby_current_{0};
  uint8_t driver_errors_{0};
  uint8_t stopped_samples_{0};
  bool saw_motion_{false};
  bool homed_{false};
};

}  // namespace esphome::tmcm1240
