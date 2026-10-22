#include "toshiba.h"

#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND

#include <cmath>
#include <iterator>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::toshiba {

ESPHOME_LOG_TAG(TAG, "toshiba.climate");

static constexpr const char *const CUSTOM_FANS[] PROGMEM = {"Low+", "Medium+"};
static constexpr const char *const CUSTOM_PRESETS[] PROGMEM = {
    "Fireplace 1", "Fireplace 2", "Frost protection", "HADA Care", "Sleep 1h", "Sleep 3h", "Sleep 5h", "Sleep 9h",
};
static constexpr uint8_t SLEEP_HOURS[] PROGMEM = {1, 3, 5, 9};
// The two copies of a SWING press can arrive in separate receiver buffers.
static constexpr uint32_t SWING_REPEAT_MS = 200;
static constexpr uint32_t POWER_SELECTION_RESTORE_VERSION = 0x5371E24D;
static constexpr uint8_t SHORT_MOTION_COMMAND = 0x21;
static constexpr uint8_t FIREPLACE_1 = 0x02;
static constexpr uint8_t FIREPLACE_2 = 0x03;
static constexpr uint8_t FEATURE_FROST = 0x04;
static constexpr uint8_t SILENT_1 = 0x02;
static constexpr uint8_t SILENT_2 = 0x0A;
static constexpr uint8_t HADA_PREFIX = 0x21;
static constexpr uint8_t HADA_SUFFIX = 0x17;
static constexpr uint8_t SLEEP_TIMER_MARKER = 0x7F;
static constexpr uint8_t SLEEP_TIMER_FLAGS = 0x03;
static constexpr float FROST_TEMP_C_DEFAULT = 8.0f;
// Frost targets are encoded from 1 C, normal targets from the normal minimum.
static constexpr uint8_t FROST_TEMP_CODE_BASE = 1;

static float target_min(bool frost) {
  return frost ? TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MIN : TOSHIBA_GENERIC_TEMP_C_MIN;
}
static float target_max(bool frost) {
  return frost ? TOSHIBA_RAS_25J2KVSG_ND_FROST_TEMP_C_MAX : TOSHIBA_GENERIC_TEMP_C_MAX;
}
static uint8_t temperature_code_base(bool frost) {
  return frost ? FROST_TEMP_CODE_BASE : static_cast<uint8_t>(TOSHIBA_GENERIC_TEMP_C_MIN);
}
static float decode_target(const uint8_t *message, bool frost) {
  return (message[5] >> 4) + temperature_code_base(frost);
}

void ToshibaClimate::setup_ras_25j2kvsg_nd_() {
  this->set_supported_custom_fan_modes_static(CUSTOM_FANS, std::size(CUSTOM_FANS));
  this->set_supported_custom_presets_static(CUSTOM_PRESETS, std::size(CUSTOM_PRESETS));
  this->presets_ = {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO};
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->power_selection_pref_ = this->make_entity_preference<uint8_t>(POWER_SELECTION_RESTORE_VERSION);
  // Lets the first SWING copy through without a separate flag.
  this->last_swing_received_ = UINT32_MAX - SWING_REPEAT_MS;
  uint8_t power;
  if (this->power_selection_pref_.load(&power) && power <= 4)
    this->power_selection_ = power;
}

void ToshibaClimate::restore_ras_25j2kvsg_nd_(climate::ClimateDeviceRestoreState &restore) {
  if (!this->modes_.count(restore.mode))
    restore.mode = climate::CLIMATE_MODE_OFF;
  auto preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  if (restore.uses_custom_preset && restore.custom_preset < std::size(CUSTOM_PRESETS))
    preset = custom_preset_at(restore.custom_preset);
  // A restored duration is not the remaining time; never re-arm an OFF timer after reboot.
  if ((restore.uses_custom_preset && (preset == ToshibaPreset::TOSHIBA_PRESET_NONE || is_sleep_preset(preset) ||
                                      (is_heating_preset(preset) && restore.mode != climate::CLIMATE_MODE_HEAT))) ||
      (!restore.uses_custom_preset && !this->presets_.count(restore.preset)) ||
      (this->power_selection_ >= 3 && (restore.uses_custom_preset || restore.preset != climate::CLIMATE_PRESET_NONE))) {
    restore.uses_custom_preset = false;
    restore.preset = climate::CLIMATE_PRESET_NONE;
    preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  }
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    restore.swing_mode = climate::CLIMATE_SWING_OFF;
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  // The restore state is packed, so clamp a copy rather than bind a reference to the field.
  const float target = restore.target_temperature;
  restore.target_temperature = std::isfinite(target) ? clamp(target, target_min(frost), target_max(frost))
                                                     : (frost ? FROST_TEMP_C_DEFAULT : 24.0f);
  if (!frost)
    this->normal_target_temperature_ = static_cast<uint8_t>(restore.target_temperature);
}

ToshibaClimate::ToshibaPreset ToshibaClimate::ras_25j2kvsg_nd_preset_() const {
  if (this->preset == climate::CLIMATE_PRESET_BOOST)
    return ToshibaPreset::TOSHIBA_PRESET_BOOST;
  if (this->preset == climate::CLIMATE_PRESET_ECO)
    return ToshibaPreset::TOSHIBA_PRESET_ECO;
  return preset_from_custom(this->get_custom_preset().c_str());
}

ToshibaClimate::ToshibaPreset ToshibaClimate::preset_from_custom(const char *custom_preset) {
  // Climate keeps a pointer into the supported table, so comparing pointers is enough.
  for (size_t i = 0; i < std::size(CUSTOM_PRESETS); i++) {
    if (custom_preset == progmem_read_ptr(&CUSTOM_PRESETS[i]))
      return custom_preset_at(i);
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
    return TOSHIBA_FAN_SPEED_2;
  if (this->get_custom_fan_mode() == progmem_read_ptr(&CUSTOM_FANS[1]))
    return TOSHIBA_FAN_SPEED_4;
  return this->encode_mode_fan_() & 0xF0;
}

void ToshibaClimate::set_ras_25j2kvsg_nd_fan_(uint8_t code) {
  if (code == TOSHIBA_FAN_SPEED_2 || code == TOSHIBA_FAN_SPEED_4) {
    this->set_custom_fan_mode_(progmem_read_ptr(&CUSTOM_FANS[code == TOSHIBA_FAN_SPEED_2 ? 0 : 1]));
    return;
  }
  switch (code) {
    case TOSHIBA_FAN_SPEED_QUIET:
      this->set_fan_mode_(climate::CLIMATE_FAN_QUIET);
      break;
    case TOSHIBA_FAN_SPEED_1:
      this->set_fan_mode_(climate::CLIMATE_FAN_LOW);
      break;
    case TOSHIBA_FAN_SPEED_3:
      this->set_fan_mode_(climate::CLIMATE_FAN_MEDIUM);
      break;
    case TOSHIBA_FAN_SPEED_5:
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
    preset = preset_from_custom(call.get_custom_preset().c_str());
  }

  auto mode = call.get_mode().value_or(this->mode);
  const bool heating_preset = is_heating_preset(preset);
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

  if (!this->validate_ras_25j2kvsg_nd_power_preset_(this->power_selection_, preset))
    return;
  if (call.get_swing_mode().has_value() && *call.get_swing_mode() != climate::CLIMATE_SWING_OFF &&
      preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE) {
    ESP_LOGW(TAG, "SWING is unavailable during HADA Care");
    return;
  }

  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  float temperature = this->target_temperature;
  if (frost && previous_preset != preset) {
    temperature = FROST_TEMP_C_DEFAULT;
  } else if (!frost && previous_preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION) {
    temperature = this->normal_target_temperature_;
  }
  temperature = call.get_target_temperature().value_or(temperature);
  if (!std::isfinite(temperature) || temperature < target_min(frost) || temperature > target_max(frost)) {
    ESP_LOGW(TAG, "Target must be %s",
             frost ? LOG_STR_LITERAL("5-13 C for frost protection") : LOG_STR_LITERAL("17-30 C for normal operation"));
    return;
  }

  const bool sleep = is_sleep_preset(preset);
  const bool was_sleep = is_sleep_preset(previous_preset);
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
    this->normal_target_temperature_ = static_cast<uint8_t>(this->target_temperature);
  this->mode = mode;
  this->target_temperature = roundf(temperature);
  if (!frost)
    this->normal_target_temperature_ = static_cast<uint8_t>(this->target_temperature);
  this->set_ras_25j2kvsg_nd_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  const bool state_command = call.get_mode().has_value() || call.get_target_temperature().has_value() ||
                             call.get_fan_mode().has_value() || call.has_custom_fan_mode() ||
                             call.get_preset().has_value() || call.has_custom_preset();
  if (state_command) {
    this->transmit_ras_25j2kvsg_nd_(preset);
  } else if (this->swing_mode != this->last_swing_mode_) {
    this->transmit_ras_25j2kvsg_nd_motion_(TOSHIBA_MOTION_SWING, 1);
  }
  this->publish_state();
}

void ToshibaClimate::transmit_ras_25j2kvsg_nd_(ToshibaPreset preset) {
  uint8_t message[12] = {0, 0, 0, 0, TOSHIBA_COMMAND_DEFAULT};
  uint8_t length = 9;
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  message[5] = (static_cast<uint8_t>(this->target_temperature) - temperature_code_base(frost)) << 4;
  message[6] = (this->encode_mode_fan_() & 0x0F) | this->ras_25j2kvsg_nd_fan_();
  if (this->power_selection_ == 1 || this->power_selection_ == 2)
    message[7] = this->power_selection_ << 6;

  if (preset == ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1) {
    message[7] |= FIREPLACE_1;
  } else if (preset == ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_2) {
    message[7] |= FIREPLACE_2;
  } else if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE) {
    length = 11;
    message[8] = HADA_PREFIX;
    message[9] = HADA_SUFFIX;
  } else if (is_sleep_preset(preset)) {
    length = 12;
    message[4] = TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_TIMER | TOSHIBA_COMMAND_POWER;
    message[8] =
        progmem_read_byte(
            &SLEEP_HOURS[static_cast<uint8_t>(preset) - static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H)])
        << 2;
    message[9] = SLEEP_TIMER_MARKER;
    message[10] = SLEEP_TIMER_FLAGS;
  } else if (preset != ToshibaPreset::TOSHIBA_PRESET_NONE || this->power_selection_ >= 3) {
    length = 10;
    message[4] = TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_POWER;
    if (preset == ToshibaPreset::TOSHIBA_PRESET_BOOST) {
      message[8] = TOSHIBA_POWER_HIGH;
    } else if (preset == ToshibaPreset::TOSHIBA_PRESET_ECO) {
      message[8] = TOSHIBA_POWER_ECO;
    } else if (frost) {
      message[8] = FEATURE_FROST;
    } else {
      message[8] = this->power_selection_ == 3 ? SILENT_1 : SILENT_2;
    }
  }
  finalize_frame(message, length);
  auto transmit = this->transmitter_->transmit();
  this->encode_(transmit.get_data(), message, length, 1);
  const bool swing = this->swing_mode != this->last_swing_mode_;
  if (swing) {
    this->encode_ras_25j2kvsg_nd_motion_(transmit.get_data(), TOSHIBA_MOTION_SWING, 1);
    this->last_swing_mode_ = this->swing_mode;
  }
  this->perform_ras_25j2kvsg_nd_(transmit, swing);
}

void ToshibaClimate::transmit_ras_25j2kvsg_nd_motion_(uint8_t command, uint8_t repeat) {
  auto transmit = this->transmitter_->transmit();
  this->encode_ras_25j2kvsg_nd_motion_(transmit.get_data(), command, repeat);
  this->last_swing_mode_ = this->swing_mode;
  this->perform_ras_25j2kvsg_nd_(transmit, command == TOSHIBA_MOTION_SWING);
}

void ToshibaClimate::encode_ras_25j2kvsg_nd_motion_(remote_base::RemoteTransmitData *data, uint8_t command,
                                                    uint8_t repeat) {
  uint8_t message[7] = {0, 0, 0, 0, SHORT_MOTION_COMMAND, command};
  finalize_frame(message, sizeof(message));
  this->encode_(data, message, sizeof(message), repeat);
}

void ToshibaClimate::perform_ras_25j2kvsg_nd_(remote_base::RemoteTransmitterBase::TransmitCall &transmit, bool swing) {
  uint32_t duration_us = 0;
  if (swing) {
    for (const int32_t pulse : transmit.get_data()->get_data())
      duration_us += static_cast<uint32_t>(std::abs(pulse));
  }
  transmit.perform();
  // perform() may wait for an earlier send before queuing this one, or block until this send finishes.
  if (swing) {
    this->swing_echo_duration_ = static_cast<uint16_t>((duration_us + 999) / 1000 + SWING_REPEAT_MS);
    this->last_swing_transmitted_ = millis();
  }
}

bool ToshibaClimate::receive_ras_25j2kvsg_nd_motion_(uint8_t command) {
  if (command != TOSHIBA_MOTION_SWING)
    return command == TOSHIBA_MOTION_FIX;
  if (this->swing_echo_duration_ != 0) {
    if (millis() - this->last_swing_transmitted_ < this->swing_echo_duration_)
      return true;
    this->swing_echo_duration_ = 0;
  }
  const uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_swing_received_ < SWING_REPEAT_MS)
    return true;
  this->last_swing_received_ = now;
  this->swing_mode =
      this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  this->last_swing_mode_ = this->swing_mode;
  this->publish_state();
  return true;
}

bool ToshibaClimate::parse_ras_25j2kvsg_nd_state(const uint8_t *message, uint8_t length, ToshibaPreset &preset,
                                                 uint8_t &power) {
  if ((message[5] & 0x0F) != 0 || ((message[6] & 0x0F) > 4 && (message[6] & 0x0F) != 7) || (message[6] & 0xF0) > 0xC0 ||
      (message[6] & 0x10) != 0 || (message[7] & 0x3C) != 0 || (message[7] & 0xC0) == 0xC0 || (message[7] & 0x03) == 1)
    return false;

  preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  power = message[7] >> 6;
  if (length == 9 && message[4] == TOSHIBA_COMMAND_DEFAULT) {
    if ((message[7] & 0x03) == FIREPLACE_1) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1;
    } else if ((message[7] & 0x03) == FIREPLACE_2) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_2;
    }
  } else if (length == 10 && message[4] == (TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_POWER) &&
             (message[7] & 0x03) == 0) {
    switch (message[8]) {
      case TOSHIBA_POWER_HIGH:
        preset = ToshibaPreset::TOSHIBA_PRESET_BOOST;
        break;
      case TOSHIBA_POWER_ECO:
        preset = ToshibaPreset::TOSHIBA_PRESET_ECO;
        break;
      case FEATURE_FROST:
        preset = ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
        break;
      case SILENT_1:
      case SILENT_2:
        if (power != 0)
          return false;
        power = message[8] == SILENT_1 ? 3 : 4;
        break;
      default:
        return false;
    }
  } else if (length == 11 && message[4] == TOSHIBA_COMMAND_DEFAULT && (message[7] & 0x03) == 0 &&
             message[8] == HADA_PREFIX && message[9] == HADA_SUFFIX) {
    preset = ToshibaPreset::TOSHIBA_PRESET_HADA_CARE;
  } else if (length == 12 && message[4] == (TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_TIMER | TOSHIBA_COMMAND_POWER) &&
             (message[7] & 0x03) == 0 && message[9] == SLEEP_TIMER_MARKER && message[10] == SLEEP_TIMER_FLAGS &&
             (message[6] & 0xF0) == 0) {
    size_t i = 0;
    while (i < std::size(SLEEP_HOURS) && message[8] != progmem_read_byte(&SLEEP_HOURS[i]) << 2)
      i++;
    if (i == std::size(SLEEP_HOURS))
      return false;
    preset = static_cast<ToshibaPreset>(static_cast<uint8_t>(ToshibaPreset::TOSHIBA_PRESET_SLEEP_1H) + i);
  } else {
    return false;
  }
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  const float temperature = decode_target(message, frost);
  if (temperature < target_min(frost) || temperature > target_max(frost))
    return false;
  // Heating presets only exist in HEAT mode.
  return !is_heating_preset(preset) || (message[6] & 0x0F) == 3;
}

bool ToshibaClimate::receive_ras_25j2kvsg_nd_frame_(const uint8_t *message, uint8_t length) {
  if (length == 7)
    return message[4] == SHORT_MOTION_COMMAND && this->receive_ras_25j2kvsg_nd_motion_(message[5]);
  ToshibaPreset preset;
  uint8_t power;
  if (!parse_ras_25j2kvsg_nd_state(message, length, preset, power))
    return false;

  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  const auto previous_preset = this->ras_25j2kvsg_nd_preset_();
  if (frost && previous_preset != preset)
    this->normal_target_temperature_ = static_cast<uint8_t>(this->target_temperature);
  if (is_sleep_preset(preset) && !is_sleep_preset(previous_preset))
    this->sleep_fan_ = this->ras_25j2kvsg_nd_fan_();
  this->decode_mode_fan_temperature_(message);
  this->set_ras_25j2kvsg_nd_fan_(message[6] & 0xF0);
  // FAN ONLY has no displayed setpoint; its temperature byte is not a new target.
  if (this->mode == climate::CLIMATE_MODE_FAN_ONLY) {
    this->target_temperature = this->normal_target_temperature_;
  } else {
    this->target_temperature = decode_target(message, frost);
    if (!frost)
      this->normal_target_temperature_ = static_cast<uint8_t>(this->target_temperature);
  }
  this->set_ras_25j2kvsg_nd_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  this->set_ras_25j2kvsg_nd_power_(power);
  this->publish_state();
  return true;
}

void ToshibaClimate::set_power_selection(size_t index) {
  const auto preset = this->ras_25j2kvsg_nd_preset_();
  if (index > 4 || !this->validate_ras_25j2kvsg_nd_power_preset_(index, preset))
    return;
  this->set_ras_25j2kvsg_nd_power_(static_cast<uint8_t>(index));
  this->transmit_ras_25j2kvsg_nd_(preset);
}

bool ToshibaClimate::validate_ras_25j2kvsg_nd_power_preset_(size_t power, ToshibaPreset preset) const {
  if (power >= 3 && preset != ToshibaPreset::TOSHIBA_PRESET_NONE) {
    ESP_LOGW(TAG, "Outdoor Silent with a preset is not supported");
    return false;
  }
  return true;
}

void ToshibaClimate::set_ras_25j2kvsg_nd_power_(uint8_t power) {
  if (this->power_selection_ != power) {
    this->power_selection_ = power;
    this->power_selection_pref_.save(&this->power_selection_);
  }
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
