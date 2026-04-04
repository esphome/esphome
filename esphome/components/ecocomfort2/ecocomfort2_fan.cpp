#ifdef USE_ESP32

#include "ecocomfort2_fan.h"
#include "esphome/core/log.h"

#include <cstring>

namespace esphome {
namespace ecocomfort2 {

static const char *const TAG = "ecocomfort2.fan";

void Ecocomfort2Fan::setup() {
  auto preset = this->parent_->get_desired_preset();
  auto speed = this->parent_->get_desired_speed();
  bool auto_mode = this->parent_->get_desired_auto_mode();

  this->state = (preset != OPER_OFF);
  this->speed = this->device_speed_to_fan_(speed);
  const char *pm = this->mode_to_preset_name_(preset, auto_mode);
  if (pm != nullptr) {
    this->set_preset_mode_(pm);
  } else {
    this->clear_preset_mode_();
  }
  // Wait for the first BLE readback before publishing a state guess.
}

void Ecocomfort2Fan::dump_config() { LOG_FAN("", "Ecocomfort2 Fan", this); }

fan::FanTraits Ecocomfort2Fan::get_traits() {
  fan::FanTraits traits;
  traits.set_speed(true);
  traits.set_supported_speed_count(4);  // Sleep, Vel1, Vel2, Vel3
  traits.set_supported_preset_modes({PRESET_IN, PRESET_OUT, PRESET_IN_OUT, PRESET_SENSOR, PRESET_AUTO});
  return traits;
}

void Ecocomfort2Fan::control(const fan::FanCall &call) {
  // Skip-send guard: don't write back during readback updates
  if (this->parent_->skip_send_)
    return;

  if (!this->parent_->is_ready()) {
    ESP_LOGW(TAG, "Not ready, cannot handle control call");
    return;
  }

  // Handle state change (on/off)
  if (call.get_state().has_value()) {
    this->state = *call.get_state();
    if (!this->state) {
      // Turning off
      this->parent_->send_operation_command(OPER_OFF, SPEED_OFF, false);
      this->publish_state();
      return;
    }
  }

  uint8_t preset = this->parent_->get_desired_preset();
  uint8_t speed = this->parent_->get_desired_speed();
  bool auto_mode = this->parent_->get_desired_auto_mode();

  if (call.has_preset_mode()) {
    const char *preset_name = call.get_preset_mode();
    preset = this->preset_name_to_mode_(preset_name);
    auto_mode = std::strcmp(preset_name, PRESET_AUTO) == 0;
    this->set_preset_mode_(preset_name);
  }

  if (call.get_speed().has_value()) {
    int fan_speed = *call.get_speed();
    speed = this->fan_speed_to_device_(fan_speed);
    this->speed = fan_speed;

    if (preset == OPER_SENSOR_OR_AUTO && auto_mode) {
      auto_mode = false;
      this->set_preset_mode_(PRESET_SENSOR);
    }
  }

  if (preset != OPER_SENSOR_OR_AUTO) {
    auto_mode = false;
  }

  if (preset == OPER_SENSOR_OR_AUTO && !auto_mode && speed == 0) {
    uint8_t fallback_speed = this->parent_->get_actual_speed();
    if (fallback_speed == 0) {
      fallback_speed = this->parent_->get_desired_speed();
    }
    if (fallback_speed == 0) {
      fallback_speed = SPEED_VEL1;
    }
    speed = fallback_speed;
    this->speed = this->device_speed_to_fan_(speed);
  }

  this->state = (preset != OPER_OFF);
  this->parent_->send_operation_command(preset, speed, auto_mode);
  this->publish_state();
}

void Ecocomfort2Fan::on_status() {
  if (!this->parent_->has_oper_data()) {
    return;
  }

  uint8_t mode = this->parent_->get_actual_mode();
  uint8_t actual_speed = this->parent_->get_actual_speed();
  bool boost = this->parent_->get_boost_active();

  bool new_state = (mode != OPER_OFF);
  int new_speed = new_state ? this->device_speed_to_fan_(actual_speed) : 0;
  if (new_state && boost) {
    new_speed = 4;  // Vel3
  }

  // Determine preset from readback using auto_active_ flag
  bool auto_active = this->parent_->get_auto_active();
  const char *new_preset = this->mode_to_preset_name_(mode, auto_active);

  // Use skip-send guard to prevent feedback loop
  this->parent_->skip_send_ = true;
  this->state = new_state;
  this->speed = new_speed;
  if (new_preset != nullptr) {
    this->set_preset_mode_(new_preset);
  } else {
    this->clear_preset_mode_();
  }
  this->publish_state();
  this->parent_->skip_send_ = false;
}

void Ecocomfort2Fan::on_connect(bool connected) { (void) connected; }

uint8_t Ecocomfort2Fan::preset_name_to_mode_(const char *preset) const {
  if (preset == nullptr) {
    return OPER_IN_OUT;
  }
  if (std::strcmp(preset, PRESET_IN) == 0)
    return OPER_IN;
  if (std::strcmp(preset, PRESET_OUT) == 0)
    return OPER_OUT;
  if (std::strcmp(preset, PRESET_IN_OUT) == 0)
    return OPER_IN_OUT;
  if (std::strcmp(preset, PRESET_SENSOR) == 0)
    return OPER_SENSOR_OR_AUTO;
  if (std::strcmp(preset, PRESET_AUTO) == 0)
    return OPER_SENSOR_OR_AUTO;
  return OPER_IN_OUT;
}

const char *Ecocomfort2Fan::mode_to_preset_name_(uint8_t mode, bool auto_active) const {
  switch (mode) {
    case OPER_OFF:
      return nullptr;
    case OPER_IN:
      return PRESET_IN;
    case OPER_OUT:
      return PRESET_OUT;
    case OPER_IN_OUT:
      return PRESET_IN_OUT;
    case OPER_SENSOR_OR_AUTO:
      return auto_active ? PRESET_AUTO : PRESET_SENSOR;
    default:
      return PRESET_IN_OUT;
  }
}

uint8_t Ecocomfort2Fan::fan_speed_to_device_(int speed) const {
  switch (speed) {
    case 1:
      return SPEED_SLEEP;
    case 2:
      return SPEED_VEL1;
    case 3:
      return SPEED_VEL2;
    case 4:
      return SPEED_VEL3;
    default:
      return SPEED_VEL1;
  }
}

int Ecocomfort2Fan::device_speed_to_fan_(uint8_t speed) const {
  switch (speed) {
    case SPEED_OFF:
      return 0;
    case SPEED_SLEEP:
      return 1;
    case SPEED_VEL1:
      return 2;
    case SPEED_VEL2:
      return 3;
    case SPEED_VEL3:
      return 4;
    default:
      return 0;
  }
}

}  // namespace ecocomfort2
}  // namespace esphome

#endif
