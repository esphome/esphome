#include "haier_tundra.h"

namespace esphome::haier_tundra {

void HaierTundra::transmit_state() {
  remote_base::HaierData remote_state{
    {
      HAIER_HEADER_UNIT_A,
      static_cast<uint8_t>(this->temperature_() << 4 | this->vertical_swing_()),
      static_cast<uint8_t>(this->horizontal_swing_() << 5),
      static_cast<uint8_t>(this->health_() << 1),
      static_cast<uint8_t>(this->power_() << 6),
      static_cast<uint8_t>(this->fan_speed_() << 5),
      static_cast<uint8_t>(this->quiet_() << 7 | this->turbo_() << 6),
      static_cast<uint8_t>(this->operation_mode_() << 5),
      0x00,
      0x00,
      static_cast<uint8_t>(this->self_clean_() << 4),
      0x00,
      this->button_(),
    }
  };

  auto transmit = this->transmitter_->transmit();
  remote_base::HaierProtocol().encode(transmit.get_data(), remote_state);
  transmit.perform();
}

uint8_t HaierTundra::temperature_() {
  return (uint8_t) roundf(clamp<float>(this->target_temperature, HAIER_TEMP_MIN, HAIER_TEMP_MAX)) - HAIER_TEMP_MIN;
}

uint8_t HaierTundra::vertical_swing_() {
  if (this->mode == climate::CLIMATE_MODE_OFF) {
    return HAIER_SWING_V_OFF;
  }
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_BOTH:
      return HAIER_SWING_V_AUTO;
    default:
      if (this->mode == climate::CLIMATE_MODE_HEAT) {
        return HAIER_SWING_V_1BOT;
      }
      return HAIER_SWING_V_1TOP; // Cool/Dry/Auto
  }
}

uint8_t HaierTundra::horizontal_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      return HAIER_SWING_H_AUTO;
    default:
      return HAIER_SWING_H_MIDDLE;
  }
}

uint8_t HaierTundra::health_() {
  return this->health_mode_ ? 1 : 0;
}

uint8_t HaierTundra::power_() {
  return this->mode == climate::CLIMATE_MODE_OFF ? 0 : 1;
}

uint8_t HaierTundra::fan_speed_() {
  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_ON)) {
    case climate::CLIMATE_FAN_LOW:
      return HAIER_FAN_LOW;
    case climate::CLIMATE_FAN_MEDIUM:
      return HAIER_FAN_MED;
    case climate::CLIMATE_FAN_HIGH:
      return HAIER_FAN_HIGH;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return HAIER_FAN_AUTO;
  }
}

uint8_t HaierTundra::quiet_() {
  return this->quiet_mode_ ? 1 : 0;
}

uint8_t HaierTundra::turbo_() {
  return this->turbo_mode_ ? 1 : 0;
}

uint8_t HaierTundra::operation_mode_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      return HAIER_MODE_COOL;
    case climate::CLIMATE_MODE_DRY:
      return HAIER_MODE_DRY;
    case climate::CLIMATE_MODE_HEAT:
      return HAIER_MODE_HEAT;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return HAIER_MODE_FAN;
    case climate::CLIMATE_MODE_HEAT_COOL:
    default:
      return HAIER_MODE_AUTO;
  }
}

uint8_t HaierTundra::self_clean_() {
  if (this->self_clean_flag_) {
    this->self_clean_flag_ = false;
    return 1;
  }
  return 0;
}

uint8_t HaierTundra::button_() {
  // Button field apparently does not matter too much except
  // when toggling light, as it has no bit flag of its own
  if (this->light_flag_) {
    this->light_flag_ = false;
    return HAIER_BUTTON_LIGHT;
  }

  // So in all other cases, just pretend we are always powering on/off I guess
  return HAIER_BUTTON_POWER;
}

bool HaierTundra::on_receive(remote_base::RemoteReceiveData data) {
  auto haier = remote_base::HaierProtocol().decode(data);
  if (haier.has_value()) {
    return this->on_haier_(*haier);
  }
  return false;
}

bool HaierTundra::on_haier_(const remote_base::HaierData &haier) {
  this->target_temperature = this->get_temperature_(haier.data[1] >> 4);
  this->swing_mode = this->get_swing_mode_(haier.data[1] & 0xF, haier.data[2] >> 5);
  this->health_mode_ = haier.data[3] & 0x2;
  this->mode = this->get_operation_mode_(haier.data[4] & 0x40, haier.data[7] >> 5);
  this->fan_mode = this->get_fan_speed_(haier.data[5] >> 5);
  this->quiet_mode_ = haier.data[6] & 0x80;
  this->turbo_mode_ = haier.data[6] & 0x40;

#ifdef USE_SWITCH
  if (this->health_switch_ != nullptr) {
    this->health_switch_->publish_state(this->health_mode_);
  }
  if (this->quiet_switch_ != nullptr) {
    this->quiet_switch_->publish_state(this->quiet_mode_);
  }
  if (this->turbo_switch_ != nullptr) {
    this->turbo_switch_->publish_state(this->turbo_mode_);
  }
#endif

  this->publish_state();

  return true;
}

float HaierTundra::get_temperature_(uint8_t temp) {
  return temp + HAIER_TEMP_MIN;
}

climate::ClimateSwingMode HaierTundra::get_swing_mode_(uint8_t swing_v, uint8_t swing_h) {
  if (swing_v == HAIER_SWING_V_AUTO) {
    if (swing_h == HAIER_SWING_H_AUTO) {
      return climate::CLIMATE_SWING_BOTH;
    }

    return climate::CLIMATE_SWING_VERTICAL;
  } else if (swing_h == HAIER_SWING_H_AUTO) {
    return climate::CLIMATE_SWING_HORIZONTAL;
  }

  return climate::CLIMATE_SWING_OFF;
}

climate::ClimateMode HaierTundra::get_operation_mode_(uint8_t power, uint8_t mode) {
  if (!power) {
    return climate::CLIMATE_MODE_OFF;
  }

  switch (mode) {
    case HAIER_MODE_COOL:
      return climate::CLIMATE_MODE_COOL;
    case HAIER_MODE_DRY:
      return climate::CLIMATE_MODE_DRY;
    case HAIER_MODE_HEAT:
      return climate::CLIMATE_MODE_HEAT;
    case HAIER_MODE_FAN:
      return climate::CLIMATE_MODE_FAN_ONLY;
    case HAIER_MODE_AUTO:
    default:
      return climate::CLIMATE_MODE_HEAT_COOL;
  }
}

climate::ClimateFanMode HaierTundra::get_fan_speed_(uint8_t fan) {
  switch (fan) {
    case HAIER_FAN_LOW:
      return climate::CLIMATE_FAN_LOW;
    case HAIER_FAN_MED:
      return climate::CLIMATE_FAN_MEDIUM;
    case HAIER_FAN_HIGH:
      return climate::CLIMATE_FAN_HIGH;
    case HAIER_FAN_AUTO:
    default:
      return climate::CLIMATE_FAN_AUTO;
  }
}

void HaierTundra::set_health_mode(bool health) {
  this->health_mode_ = health;

  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->transmit_state();
  }
}

void HaierTundra::set_quiet_mode(bool quiet) {
  // Quiet & Turbo cannot be enabled at the same time
  if (quiet) {
#ifdef USE_SWITCH
    if (this->turbo_switch_ != nullptr && this->turbo_switch_->state) {
      this->turbo_switch_->publish_state(false);
    }
#endif
    this->turbo_mode_ = false;
  }

  this->quiet_mode_ = quiet;
  this->transmit_state();
}

void HaierTundra::set_turbo_mode(bool turbo) {
  // Quiet & Turbo cannot be enabled at the same time
  if (turbo) {
#ifdef USE_SWITCH
    if (this->quiet_switch_ != nullptr && this->quiet_switch_->state) {
      this->quiet_switch_->publish_state(false);
    }
#endif
    this->quiet_mode_ = false;
  }

  this->turbo_mode_ = turbo;
  this->transmit_state();
}

void HaierTundra::toggle_light() {
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->light_flag_ = true;
    this->transmit_state();
  }
}

void HaierTundra::start_self_clean() {
  this->self_clean_flag_ = true;

  // Can be started when off
  this->transmit_state();
}

}  // namespace esphome::haier_tundra
