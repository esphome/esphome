#include "husb238.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome::husb238 {

static const char *const TAG = "husb238";

void Husb238Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up HUSB238...");

  if (!this->read_byte(static_cast<uint8_t>(CommandRegister::PD_STATUS0), &this->registers_.raw[0])) {
    this->mark_failed(LOG_STR("Failed to read HUSB238"));
    return;
  }
}

void Husb238Component::update() {
  if (!this->is_ready()) {
    return;
  }

  bool is_changed{false};
  if (!this->read_all_(is_changed)) {
    is_changed = !this->status_has_error();
    this->status_set_error("Unable to communicate with HUSB238 chip");
    std::fill(std::begin(this->registers_.raw), std::end(this->registers_.raw), 0);
  } else {
    this->status_clear_error();
  }

  if (!is_changed) {
    return;
  }

#ifdef USE_BINARY_SENSOR
  if (this->attached_binary_sensor_ != nullptr) {
    this->attached_binary_sensor_->publish_state(this->is_attached());
  }
  if (this->cc_direction_binary_sensor_ != nullptr) {
    this->cc_direction_binary_sensor_->publish_state(this->registers_.pd_status1.cc_dir);
  }
#endif
}

void Husb238Component::dump_config() {
  ESP_LOGCONFIG(TAG, "HUSB238:");

#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "PD Attached", this->attached_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "CC Direction", this->cc_direction_binary_sensor_);
#endif
}

bool Husb238Component::is_attached() {
  if (!this->is_ready()) {
    return false;
  }
  return this->registers_.pd_status1.attached;
}

bool Husb238Component::read_all_(bool &is_changed) {
  if (!this->is_ready()) {
    ESP_LOGE(TAG, "Component not ready");
    return false;
  }
  uint8_t old_regs[REG_NUM];
  std::memcpy(old_regs, this->registers_.raw, REG_NUM);

  auto ok = this->read_bytes(static_cast<uint8_t>(CommandRegister::PD_STATUS0), &this->registers_.raw[0], REG_NUM);
  if (!ok) {
    ESP_LOGE(TAG, "Error reading HUSB238");
  }
  is_changed = std::memcmp(old_regs, this->registers_.raw, REG_NUM) != 0;

  return ok;
}

}  // namespace esphome::husb238
