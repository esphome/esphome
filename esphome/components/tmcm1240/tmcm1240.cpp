#include "tmcm1240.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include "esphome/core/log.h"

namespace esphome::tmcm1240 {

ESPHOME_LOG_TAG(TAG, "tmcm1240");

void TMCM1240::dump_config() {
  ESP_LOGCONFIG(TAG, "TMCM-1240 node %u", this->node_);
  ESP_LOGCONFIG(TAG, "  Run/standby current registers: %u/%u", this->run_current_, this->standby_current_);
  LOG_UPDATE_INTERVAL(this);
}

uint8_t TMCM1240::current_register(float amps) {
  if (!std::isfinite(amps) || amps < 0.072f || amps > 2.0f)
    return 0;
  const int32_t level = std::clamp<int32_t>(static_cast<int32_t>(std::lround(amps * 32.0f * std::numbers::sqrt2_v<float> / 3.250f)), 1, 32);
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

}  // namespace esphome::tmcm1240
