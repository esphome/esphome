#include "tmcm1240.h"
#include <algorithm>
#include <cmath>
#include "esphome/core/log.h"

namespace esphome::tmcm1240 {

ESPHOME_LOG_TAG(TAG, "tmcm1240");
using cia402::DriveResult;

void TMCM1240::setup() {
  CiA402Drive::setup();
  this->add_on_heartbeat_callback([this](uint8_t state) {
    if (state != 5)
      this->homed_ = false;
  });
}

void TMCM1240::dump_config() {
  ESP_LOGCONFIG(TAG, "TMCM-1240 node %u", this->node_);
  ESP_LOGCONFIG(TAG, "  Run/standby current registers: %u/%u", this->run_current_, this->standby_current_);
  LOG_UPDATE_INTERVAL(this);
}

uint8_t TMCM1240::current_register(float amps) {
  if (!std::isfinite(amps) || amps < 0.072f || amps > 2.0f)
    return 0;
  const long level = std::clamp(std::lround(amps * 32.0f * std::sqrt(2.0f) / 3.250f), 1L, 32L);
  return static_cast<uint8_t>((level - 1) * 8);
}

void TMCM1240::append_configuration_() {
  this->read_(0x606C, 4, UINT32_MAX, 0);
  this->write_(0x2003, this->run_current_, 1);
  this->write_(0x2004, this->standby_current_, 1);
  // StallGuard needs SpreadCycle and fixed current for repeatable sensitivity.
  this->write_(0x20B0, 0, 4);
  this->write_(0x209C, 0, 1);
  this->write_(0x209D, 0, 1);
  this->write_(0x209E, static_cast<uint8_t>(this->stallguard_threshold_), 1);
  this->write_(0x20A7, 1, 1);
  this->write_(0x20A4, this->stallguard_min_velocity_, 4);
}

bool TMCM1240::apply_configuration() {
  if (!this->begin_())
    return false;
  this->append_configuration_();
  return true;
}

void TMCM1240::append_profile() {
  CiA402Drive::append_profile();
  // Both sections of the six-point ramp use the requested acceleration/deceleration.
  this->write_(0x2011, this->acceleration_, 4);
  this->write_(0x2013, this->deceleration_, 4);
}

bool TMCM1240::sensorless_home() {
  if (this->homing_velocity_ <= this->stallguard_min_velocity_ || this->stallguard_min_velocity_ == 0 ||
      this->homing_travel_ == 0 || this->homing_margin_ >= this->homing_travel_ || this->homing_backoff_ == 0) {
    this->result_callback_.call(DriveResult::DRIVE_RESULT_REJECTED);
    return false;
  }
  if (!this->begin_())
    return false;
  this->homed_ = false;
  this->phase_ = Phase::PHASE_CONFIGURE;
  this->homing_started_ms_ = millis();
  this->saved_velocity_ = this->profile_velocity_;
  this->profile_velocity_ = this->homing_velocity_;
  // Configure in its own sequence to keep the shared fixed step array small.
  this->append_configuration_();
  return true;
}

void TMCM1240::received_value(uint16_t index, uint32_t value) {
  switch (index) {
    case 0x6041:
      if (value & 8)
        this->homed_ = false;
      break;
    case 0x6064:
      if (this->phase_ == Phase::PHASE_SEEK) {
        const int64_t target = static_cast<int64_t>(sign_extend<32>(value)) - this->homing_travel_;
        if (target >= INT32_MIN && target <= INT32_MAX)
          this->seek_target_ = target;
      }
      break;
    case 0x607A:
      this->target_readback_ = sign_extend<32>(value);
      break;
    case 0x2102:
      this->driver_errors_ = value;
      break;
    case 0x209E:
      this->threshold_readback_ = sign_extend<8>(value);
      break;
    case 0x20A4:
      this->minimum_velocity_readback_ = value;
      break;
    default:
      break;
  }
  CiA402Drive::received_value(index, value);
}

void TMCM1240::monitor_() {
  this->read_(0x6041, 2);
  this->read_(0x606C, 4);
  this->read_(0x6064, 4);
  this->read_(0x2102, 1);
  this->read_(0x209E, 1);
  this->read_(0x20A4, 4);
  this->read_(0x20A7, 1, 0xFF, 1);
  this->read_(0x607A, 4);
}

bool TMCM1240::sequence_finished(DriveResult result) {
  if (this->phase_ == Phase::PHASE_IDLE)
    return true;
  if (result != DriveResult::DRIVE_RESULT_SUCCESS) {
    this->phase_ = Phase::PHASE_IDLE;
    this->profile_velocity_ = this->saved_velocity_;
    this->homed_ = false;
    return true;
  }
  if (millis() - this->homing_started_ms_ >= this->operation_timeout_ms_) {
    this->fail_(DriveResult::DRIVE_RESULT_TIMEOUT);
    return false;
  }
  if (this->phase_ == Phase::PHASE_ZERO) {
    if (this->position_ != 0) {
      this->fail_(DriveResult::DRIVE_RESULT_INVALID_RESPONSE);
      return false;
    }
    this->phase_ = Phase::PHASE_IDLE;
    this->profile_velocity_ = this->saved_velocity_;
    this->homed_ = true;
    return true;
  }
  if (!this->begin_()) {
    this->phase_ = Phase::PHASE_IDLE;
    this->profile_velocity_ = this->saved_velocity_;
    return false;
  }
  if (this->phase_ == Phase::PHASE_CONFIGURE) {
    // Configuration is complete. A positive relative move must finish before the negative seek.
    this->phase_ = Phase::PHASE_BACKOFF;
    this->append_move_(this->homing_backoff_, true);
    return false;
  }
  if (this->phase_ == Phase::PHASE_BACKOFF) {
    this->phase_ = Phase::PHASE_SEEK;
    this->saw_motion_ = false;
    this->stopped_samples_ = 0;
    this->append_move_(-static_cast<int32_t>(this->homing_travel_), true);
    // Replace target-reached waiting with bounded stop-on-stall observation.
    this->step_count_ -= 3;
    return false;
  }
  if (this->phase_ == Phase::PHASE_SEEK) {
    this->phase_ = Phase::PHASE_MONITOR;
    this->monitor_();
    return false;
  }
  if (this->velocity_ < -static_cast<int32_t>(this->stallguard_min_velocity_))
    this->saw_motion_ = true;
  const bool early_stop =
      this->velocity_ == 0 && this->saw_motion_ && !(this->statusword_ & 0x408) &&
      static_cast<int64_t>(this->position_) > static_cast<int64_t>(this->seek_target_) + this->homing_margin_ &&
      this->target_readback_ == this->seek_target_ && this->driver_errors_ == 0 &&
      this->threshold_readback_ == this->stallguard_threshold_ &&
      this->minimum_velocity_readback_ == this->stallguard_min_velocity_;
  this->stopped_samples_ = early_stop ? this->stopped_samples_ + 1 : 0;
  if (this->stopped_samples_ >= 2) {
    this->phase_ = Phase::PHASE_ZERO;
    this->append_home_(35, 0);
    this->write_(0x607A, 0, 4);
    this->write_(0x6060, 1, 1);
    this->read_(0x6061, 1, 0xFF, 1, true);
    return false;
  }
  if (this->driver_errors_ != 0 || (this->statusword_ & 0x408) ||
      static_cast<int64_t>(this->position_) <= static_cast<int64_t>(this->seek_target_) + this->homing_margin_) {
    this->fail_(DriveResult::DRIVE_RESULT_FAULT);
    return false;
  }
  this->monitor_();
  return false;
}

}  // namespace esphome::tmcm1240
