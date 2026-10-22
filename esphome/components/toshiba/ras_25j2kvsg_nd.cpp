#include "toshiba.h"

#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND

#include <cmath>
#include <iterator>

#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::toshiba {

ESPHOME_LOG_TAG(TAG, "toshiba.climate");

static constexpr const char *const CUSTOM_FANS[] PROGMEM = {"Low+", "Medium+"};
static constexpr const char *const CUSTOM_PRESETS[] PROGMEM = {
    "Fireplace 1", "Fireplace 2", "Frost protection", "HADA Care", "Sleep 1h", "Sleep 3h", "Sleep 5h", "Sleep 9h",
};
static constexpr uint8_t SLEEP_HOURS[] PROGMEM = {1, 3, 5, 9};
// The two copies of a SWING press can arrive in separate receiver buffers.
static constexpr uint32_t SWING_REPEAT_MS = 200;

void ToshibaClimate::setup_ras_25j2kvsg_nd_() {
  this->minimum_temperature_ = this->temperature_min_();
  this->maximum_temperature_ = this->temperature_max_();
  this->swing_modes_ = this->toshiba_swing_modes_();
  this->set_supported_custom_fan_modes_static(CUSTOM_FANS, std::size(CUSTOM_FANS));
  this->set_supported_custom_presets_static(CUSTOM_PRESETS, std::size(CUSTOM_PRESETS));
  this->presets_ = {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO};
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
}

void ToshibaClimate::restore_ras_25j2kvsg_nd_(climate::ClimateDeviceRestoreState &restore) {
  if (!this->modes_.count(restore.mode))
    restore.mode = climate::CLIMATE_MODE_OFF;
  auto preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  if (restore.uses_custom_preset && restore.custom_preset < std::size(CUSTOM_PRESETS)) {
    preset = static_cast<ToshibaPreset>(restore.custom_preset +
                                        static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1));
  }
  // A restored duration is not the remaining time; never re-arm an OFF timer after reboot.
  const bool heating_preset =
      preset >= ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1 && preset <= ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  if ((restore.uses_custom_preset &&
       (preset == ToshibaPreset::TOSHIBA_PRESET_NONE || preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H ||
        (heating_preset && restore.mode != climate::CLIMATE_MODE_HEAT))) ||
      (!restore.uses_custom_preset && !this->presets_.count(restore.preset))) {
    restore.uses_custom_preset = false;
    restore.preset = climate::CLIMATE_PRESET_NONE;
    preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  }
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    restore.swing_mode = climate::CLIMATE_SWING_OFF;
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  restore.target_temperature = std::isfinite(restore.target_temperature)
                                   ? clamp(restore.target_temperature, frost ? 5.0f : 17.0f, frost ? 13.0f : 30.0f)
                                   : (frost ? 8.0f : 24.0f);
  if (!frost)
    this->normal_target_temperature_ = restore.target_temperature;
}

ToshibaClimate::ToshibaPreset ToshibaClimate::ras_25j2kvsg_nd_preset_() const {
  if (this->preset == climate::CLIMATE_PRESET_BOOST)
    return ToshibaPreset::TOSHIBA_PRESET_BOOST;
  if (this->preset == climate::CLIMATE_PRESET_ECO)
    return ToshibaPreset::TOSHIBA_PRESET_ECO;
  for (size_t i = 0; i < std::size(CUSTOM_PRESETS); i++) {
    if (this->get_custom_preset() == progmem_read_ptr(&CUSTOM_PRESETS[i]))
      return static_cast<ToshibaPreset>(i + static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1));
  }
  return ToshibaPreset::TOSHIBA_PRESET_NONE;
}

void ToshibaClimate::set_ras_25j2kvsg_nd_preset_(ToshibaPreset preset) {
  switch (preset) {
    case ToshibaPreset::TOSHIBA_PRESET_NONE:
      this->set_preset_(climate::CLIMATE_PRESET_NONE);
      break;
    case ToshibaPreset::TOSHIBA_PRESET_BOOST:
      this->set_preset_(climate::CLIMATE_PRESET_BOOST);
      break;
    case ToshibaPreset::TOSHIBA_PRESET_ECO:
      this->set_preset_(climate::CLIMATE_PRESET_ECO);
      break;
    default:
      this->set_custom_preset_(
          progmem_read_ptr(&CUSTOM_PRESETS[static_cast<uint8_t>(preset) -
                                           static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1)]));
  }
}

uint8_t ToshibaClimate::ras_25j2kvsg_nd_fan_() const {
  if (this->get_custom_fan_mode() == progmem_read_ptr(&CUSTOM_FANS[0]))
    return 0x60;
  if (this->get_custom_fan_mode() == progmem_read_ptr(&CUSTOM_FANS[1]))
    return 0xA0;
  return this->encode_mode_fan_() & 0xF0;
}

void ToshibaClimate::set_ras_25j2kvsg_nd_fan_(uint8_t code) {
  if (code == 0x60 || code == 0xA0) {
    this->set_custom_fan_mode_(progmem_read_ptr(&CUSTOM_FANS[code == 0x60 ? 0 : 1]));
    return;
  }
  switch (code) {
    case 0x20:
      this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    case 0x40:
      this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case 0x80:
      this->set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case 0xC0:
      this->set_fan_mode_(climate::CLIMATE_FAN_HIGH);
      break;
    default:
      this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
  }
}

void ToshibaClimate::control(const climate::ClimateCall &call) {
  if (this->model_ != MODEL_RAS_25J2KVSG_ND) {
    climate_ir::ClimateIR::control(call);
    return;
  }

  const auto previous_preset = this->ras_25j2kvsg_nd_preset_();
  auto preset = previous_preset;
  if (call.get_preset().has_value()) {
    switch (*call.get_preset()) {
      case climate::CLIMATE_PRESET_BOOST:
        preset = ToshibaPreset::TOSHIBA_PRESET_BOOST;
        break;
      case climate::CLIMATE_PRESET_ECO:
        preset = ToshibaPreset::TOSHIBA_PRESET_ECO;
        break;
      default:
        preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
    }
  } else if (call.has_custom_preset()) {
    for (size_t i = 0; i < std::size(CUSTOM_PRESETS); i++) {
      if (call.get_custom_preset() == progmem_read_ptr(&CUSTOM_PRESETS[i]))
        preset = static_cast<ToshibaPreset>(i + static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1));
    }
  }

  auto mode = call.get_mode().value_or(this->mode);
  const bool heating_preset =
      preset >= ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1 && preset <= ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  if (heating_preset && !this->modes_.count(climate::CLIMATE_MODE_HEAT)) {
    ESP_LOGW(TAG, "This preset requires HEAT support");
    return;
  }
  if (heating_preset && mode != climate::CLIMATE_MODE_HEAT) {
    if (call.has_custom_preset() && call.get_mode().has_value()) {
      ESP_LOGW(TAG, "This preset requires HEAT mode");
      return;
    }
    if (call.has_custom_preset()) {
      mode = climate::CLIMATE_MODE_HEAT;
    } else {
      preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
    }
  }
  if (call.get_mode() == climate::CLIMATE_MODE_OFF && !call.get_preset().has_value() && !call.has_custom_preset())
    preset = ToshibaPreset::TOSHIBA_PRESET_NONE;

  if (this->power_selection_ >= 3 && preset != ToshibaPreset::TOSHIBA_PRESET_NONE) {
    ESP_LOGW(TAG, "Combined outdoor Silent and preset encoding is not verified; select 100%%, 75%% or 50%% first");
    return;
  }
  if (call.get_swing_mode().has_value() && *call.get_swing_mode() != climate::CLIMATE_SWING_OFF &&
      preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE) {
    ESP_LOGW(TAG, "SWING is unavailable during HADA Care");
    return;
  }

  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  float temperature = this->target_temperature;
  if (frost && previous_preset != preset) {
    temperature = 8;
  } else if (!frost && previous_preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION) {
    temperature = this->normal_target_temperature_;
  }
  temperature = call.get_target_temperature().value_or(temperature);
  if (!std::isfinite(temperature) || temperature < (frost ? 5 : 17) || temperature > (frost ? 13 : 30)) {
    ESP_LOGW(TAG, "Target must be %s",
             frost ? LOG_STR_LITERAL("5-13 C for frost protection") : LOG_STR_LITERAL("17-30 C for normal operation"));
    return;
  }

  const bool sleep = preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H;
  const bool was_sleep = previous_preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H;
  if (sleep && !was_sleep) {
    this->sleep_fan_ = this->ras_25j2kvsg_nd_fan_();
  } else if (!sleep && was_sleep) {
    this->set_ras_25j2kvsg_nd_fan_(this->sleep_fan_);
  }
  if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  } else if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  }
  if (sleep || mode == climate::CLIMATE_MODE_DRY)
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);

  if (frost && previous_preset != preset)
    this->normal_target_temperature_ = this->target_temperature;
  this->mode = mode;
  this->target_temperature = roundf(temperature);
  if (!frost)
    this->normal_target_temperature_ = this->target_temperature;
  this->set_ras_25j2kvsg_nd_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  const bool state_command = call.get_mode().has_value() || call.get_target_temperature().has_value() ||
                             call.get_fan_mode().has_value() || call.has_custom_fan_mode() ||
                             call.get_preset().has_value() || call.has_custom_preset();
  if (state_command) {
    this->transmit_ras_25j2kvsg_nd_();
  } else if (this->swing_mode != this->last_swing_mode_) {
    this->transmit_ras_25j2kvsg_nd_motion_(0x04, 1);
  }
  this->publish_state();
}

void ToshibaClimate::transmit_ras_25j2kvsg_nd_() {
  uint8_t message[12] = {0xF2, 0x0D, 0, 0, 0x01};
  uint8_t length = 9;
  const auto preset = this->ras_25j2kvsg_nd_preset_();
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  message[5] = (static_cast<uint8_t>(this->target_temperature) - (frost ? 1 : 17)) << 4;
  message[6] = (this->encode_mode_fan_() & 0x0F) | this->ras_25j2kvsg_nd_fan_();
  if (this->power_selection_ == 1 || this->power_selection_ == 2)
    message[7] = this->power_selection_ << 6;

  if (preset == ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1) {
    message[7] |= 0x02;
  } else if (preset == ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_2) {
    message[7] |= 0x03;
  } else if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE) {
    length = 11;
    message[8] = 0x21;
    message[9] = 0x17;
  } else if (preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H) {
    length = 12;
    message[4] = 0x0B;
    message[8] =
        progmem_read_byte(
            &SLEEP_HOURS[static_cast<uint8_t>(preset) - static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H)])
        << 2;
    message[9] = 0x7F;
    message[10] = 0x03;
  } else if (preset != ToshibaPreset::TOSHIBA_PRESET_NONE || this->power_selection_ >= 3) {
    length = 10;
    message[4] = 0x09;
    if (preset == ToshibaPreset::TOSHIBA_PRESET_BOOST) {
      message[8] = 0x01;
    } else if (preset == ToshibaPreset::TOSHIBA_PRESET_ECO) {
      message[8] = 0x03;
    } else if (frost) {
      message[8] = 0x04;
    } else {
      message[8] = this->power_selection_ == 3 ? 0x02 : 0x0A;
    }
  }
  message[2] = length - 6;
  message[3] = ~message[2];
  message[length - 1] = xor_bytes(message + 4, length - 5);
  auto transmit = this->transmitter_->transmit();
  this->encode_(transmit.get_data(), message, length, 1);
  if (this->swing_mode != this->last_swing_mode_) {
    const uint8_t motion[] = {0xF2, 0x0D, 0x01, 0xFE, 0x21, 0x04, 0x25};
    this->encode_(transmit.get_data(), motion, sizeof(motion), 1);
    this->last_swing_mode_ = this->swing_mode;
  }
  transmit.perform();
}

void ToshibaClimate::transmit_ras_25j2kvsg_nd_motion_(uint8_t command, uint8_t repeat) {
  const uint8_t message[] = {0xF2, 0x0D, 0x01, 0xFE, 0x21, command, static_cast<uint8_t>(0x21 ^ command)};
  auto transmit = this->transmitter_->transmit();
  this->encode_(transmit.get_data(), message, sizeof(message), repeat);
  this->last_swing_mode_ = this->swing_mode;
  transmit.perform();
}

bool ToshibaClimate::receive_ras_25j2kvsg_nd_(const uint8_t *message, uint8_t length) {
  if (length == 7) {
    if (message[4] != 0x21 || (message[5] != 0x00 && message[5] != 0x04))
      return false;
    if (message[5] == 0x04) {
      const uint32_t now = App.get_loop_component_start_time();
      if (this->swing_received_ && now - this->last_swing_received_ < SWING_REPEAT_MS)
        return true;
      this->last_swing_received_ = now;
      this->swing_received_ = true;
      this->swing_mode =
          this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
      this->last_swing_mode_ = this->swing_mode;
      this->publish_state();
    }
    return true;
  }
  if (length < 9 || length > 12 || (message[5] & 0x0F) != 0 || ((message[6] & 0x0F) > 4 && (message[6] & 0x0F) != 7) ||
      (message[6] & 0xF0) > 0xC0 || (message[6] & 0x10) != 0 || (message[7] & 0x3C) != 0 ||
      (message[7] & 0xC0) == 0xC0 || (message[7] & 0x03) == 1)
    return false;

  auto preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  uint8_t power = message[7] >> 6;
  if (length == 9 && message[4] == 0x01) {
    if ((message[7] & 0x03) == 2) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1;
    } else if ((message[7] & 0x03) == 3) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_2;
    }
  } else if (length == 10 && message[4] == 0x09 && (message[7] & 0x03) == 0) {
    switch (message[8]) {
      case 0x01:
        preset = ToshibaPreset::TOSHIBA_PRESET_BOOST;
        break;
      case 0x03:
        preset = ToshibaPreset::TOSHIBA_PRESET_ECO;
        break;
      case 0x04:
        preset = ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
        break;
      case 0x02:
      case 0x0A:
        if (power != 0)
          return false;
        power = message[8] == 0x02 ? 3 : 4;
        break;
      default:
        return false;
    }
  } else if (length == 11 && message[4] == 0x01 && (message[7] & 0x03) == 0 && message[8] == 0x21 &&
             message[9] == 0x17) {
    preset = ToshibaPreset::TOSHIBA_PRESET_HADA_CARE;
  } else if (length == 12 && message[4] == 0x0B && (message[7] & 0x03) == 0 && message[9] == 0x7F &&
             message[10] == 0x03) {
    bool found = false;
    for (size_t i = 0; i < std::size(SLEEP_HOURS); i++) {
      if (message[8] == progmem_read_byte(&SLEEP_HOURS[i]) << 2) {
        preset = static_cast<ToshibaPreset>(static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H) + i);
        found = true;
        break;
      }
    }
    if (!found || (message[6] & 0xF0) != 0)
      return false;
  } else {
    return false;
  }
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  const float temperature = (message[5] >> 4) + (frost ? 1 : 17);
  if (temperature < (frost ? 5 : 17) || temperature > (frost ? 13 : 30))
    return false;
  if (preset >= ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1 && preset <= ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION &&
      (message[6] & 0x0F) != 3)
    return false;

  const auto previous_preset = this->ras_25j2kvsg_nd_preset_();
  if (frost && previous_preset != preset)
    this->normal_target_temperature_ = this->target_temperature;
  if (preset >= ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H && previous_preset < ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H)
    this->sleep_fan_ = this->ras_25j2kvsg_nd_fan_();
  this->decode_mode_fan_temperature_(message);
  this->set_ras_25j2kvsg_nd_fan_(message[6] & 0xF0);
  // FAN ONLY has no displayed setpoint; its temperature byte is not a new target.
  this->target_temperature =
      this->mode == climate::CLIMATE_MODE_FAN_ONLY ? this->normal_target_temperature_ : temperature;
  if (!frost && this->mode != climate::CLIMATE_MODE_FAN_ONLY)
    this->normal_target_temperature_ = this->target_temperature;
  this->set_ras_25j2kvsg_nd_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  this->power_selection_ = power;
  this->publish_power_selection_();
  this->publish_state();
  return true;
}

void ToshibaClimate::set_power_selection(size_t index) {
  if (this->model_ != MODEL_RAS_25J2KVSG_ND || index > 4)
    return;
  if (index >= 3 && this->ras_25j2kvsg_nd_preset_() != ToshibaPreset::TOSHIBA_PRESET_NONE) {
    ESP_LOGW(TAG, "Combined outdoor Silent and preset encoding is not verified; cancel the preset first");
    return;
  }
  this->power_selection_ = static_cast<uint8_t>(index);
  this->transmit_ras_25j2kvsg_nd_();
  this->publish_power_selection_();
}

void ToshibaClimate::publish_power_selection_() {
#ifdef USE_SELECT
  if (this->power_selection_select_ != nullptr)
    this->power_selection_select_->publish_state(static_cast<size_t>(this->power_selection_));
#endif
}

}  // namespace esphome::toshiba

#endif  // USE_TOSHIBA_RAS_25J2KVSG_ND
