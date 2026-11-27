#include "haier_tundra.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::haier_tundra {

// Number of bytes in decoded packet sans checksum
static constexpr uint8_t HAIER_TUNDRA_FRAME_SIZE = 13;

using climate_ir::from_ir_code;
using climate_ir::IRCode;
using climate_ir::to_ir_code;

static constexpr IRCode MODES[] PROGMEM = {
    {climate::CLIMATE_MODE_HEAT_COOL, HAIER_MODE_AUTO}, {climate::CLIMATE_MODE_COOL, HAIER_MODE_COOL},
    {climate::CLIMATE_MODE_DRY, HAIER_MODE_DRY},        {climate::CLIMATE_MODE_HEAT, HAIER_MODE_HEAT},
    {climate::CLIMATE_MODE_FAN_ONLY, HAIER_MODE_FAN},
};
static constexpr IRCode FANS[] PROGMEM = {
    {climate::CLIMATE_FAN_AUTO, HAIER_FAN_AUTO},
    {climate::CLIMATE_FAN_LOW, HAIER_FAN_LOW},
    {climate::CLIMATE_FAN_MEDIUM, HAIER_FAN_MED},
    {climate::CLIMATE_FAN_HIGH, HAIER_FAN_HIGH},
};

void HaierTundra::transmit_state() {
  // Button value does not seem to matter too much except
  // when toggling light, as it has no bit flag of its own
  // So in the default case we can simply set it to POWER
  this->transmit_(HAIER_BUTTON_POWER);
}

void HaierTundra::transmit_(uint8_t button) {
  // Force Quiet and Turbo off if we're not in Cool or Heat mode
  if (this->mode != climate::CLIMATE_MODE_COOL && this->mode != climate::CLIMATE_MODE_HEAT) {
#ifdef USE_SWITCH
    PUBLISH_STATE(this->quiet_mode_switch_, false);
    PUBLISH_STATE(this->turbo_mode_switch_, false);
#endif

    this->quiet_mode_ = false;
    this->turbo_mode_ = false;
  }

  const uint8_t frame[HAIER_TUNDRA_FRAME_SIZE] = {
      this->unit_,
      static_cast<uint8_t>(this->temperature_() << 4 | this->vertical_swing_()),
      static_cast<uint8_t>(this->horizontal_swing_() << 5),
      static_cast<uint8_t>(this->health_() << 1),
      static_cast<uint8_t>(this->power_() << 6),
      static_cast<uint8_t>(to_ir_code(FANS, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO), HAIER_FAN_AUTO) << 5),
      static_cast<uint8_t>(this->quiet_() << 7 | this->turbo_() << 6),
      static_cast<uint8_t>(to_ir_code(MODES, this->mode, HAIER_MODE_AUTO) << 5),
      0x00,
      0x00,
      static_cast<uint8_t>((button == HAIER_BUTTON_SELF_CLEAN ? 1 : 0) << 4),
      0x00,
      button,
  };

  auto transmit = this->transmitter_->transmit();
  remote_base::HaierProtocol().encode(transmit.get_data(), frame, sizeof(frame));
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
      return HAIER_SWING_V_1TOP;  // Cool/Dry/Auto
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

uint8_t HaierTundra::health_() { return this->health_mode_ ? 1 : 0; }

uint8_t HaierTundra::power_() { return this->mode == climate::CLIMATE_MODE_OFF ? 0 : 1; }

uint8_t HaierTundra::quiet_() { return this->quiet_mode_ ? 1 : 0; }

uint8_t HaierTundra::turbo_() { return this->turbo_mode_ ? 1 : 0; }

bool HaierTundra::on_receive(remote_base::RemoteReceiveData data) {
  auto haier = remote_base::HaierProtocol().decode(data);
  if (haier.has_value()) {
    return this->on_haier_(*haier);
  }
  return false;
}

bool HaierTundra::on_haier_(const remote_base::HaierData &haier) {
  // Verify frame length and header
  if (haier.data.size() != HAIER_TUNDRA_FRAME_SIZE || haier.data[0] != this->unit_) {
    return false;
  }

  this->target_temperature = this->get_temperature_(haier.data[1] >> 4);
  this->swing_mode = this->get_swing_mode_(haier.data[1] & 0xF, haier.data[2] >> 5);
  this->health_mode_ = haier.data[3] & 0x2;
  this->mode = (haier.data[4] & 0x40) ? from_ir_code(MODES, haier.data[7] >> 5, climate::CLIMATE_MODE_HEAT_COOL)
                                      : climate::CLIMATE_MODE_OFF;
  this->fan_mode = from_ir_code(FANS, haier.data[5] >> 5, climate::CLIMATE_FAN_AUTO);
  this->quiet_mode_ = haier.data[6] & 0x80;
  this->turbo_mode_ = haier.data[6] & 0x40;

#ifdef USE_SWITCH
  PUBLISH_STATE(this->health_mode_switch_, this->health_mode_);
  PUBLISH_STATE(this->quiet_mode_switch_, this->quiet_mode_);
  PUBLISH_STATE(this->turbo_mode_switch_, this->turbo_mode_);
#endif

  this->publish_state();

  return true;
}

float HaierTundra::get_temperature_(uint8_t temp) { return temp + HAIER_TEMP_MIN; }

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

void HaierTundra::set_health_mode(bool health, bool transmit) {
  this->health_mode_ = health;

  if (transmit && this->mode != climate::CLIMATE_MODE_OFF) {
    this->transmit_(HAIER_BUTTON_HEALTH);
  }
}

void HaierTundra::set_quiet_mode(bool quiet, bool transmit) {
  // Quiet & Turbo cannot be enabled at the same time
  if (quiet) {
#ifdef USE_SWITCH
    PUBLISH_STATE(this->turbo_mode_switch_, false);
#endif
    this->turbo_mode_ = false;
  }

  this->quiet_mode_ = quiet;
  if (transmit)
    this->transmit_(HAIER_BUTTON_TURBO_QUIET);
}

void HaierTundra::set_turbo_mode(bool turbo, bool transmit) {
  // Quiet & Turbo cannot be enabled at the same time
  if (turbo) {
#ifdef USE_SWITCH
    PUBLISH_STATE(this->quiet_mode_switch_, false);
#endif
    this->quiet_mode_ = false;
  }

  this->turbo_mode_ = turbo;
  if (transmit)
    this->transmit_(HAIER_BUTTON_TURBO_QUIET);
}

void HaierTundra::toggle_light() {
  if (this->mode != climate::CLIMATE_MODE_OFF) {
    this->transmit_(HAIER_BUTTON_LIGHT);
  }
}

void HaierTundra::start_self_clean() {
  // Can be started when off
  this->transmit_(HAIER_BUTTON_SELF_CLEAN);
}

}  // namespace esphome::haier_tundra
