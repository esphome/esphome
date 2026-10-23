#include "ras_25j2kvsg_nd.h"

#ifdef USE_TOSHIBA_RAS_25J2KVSG_ND

#include <cmath>
#include <cstring>
#include <iterator>

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
static constexpr uint8_t MAX_FRAME_LENGTH = 12;
static constexpr uint8_t MOTION_FRAME_LENGTH = 7;
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
// NaN passes through unchanged, so it never equals a valid target.
static float clamp_target(float target, bool frost) { return clamp(target, target_min(frost), target_max(frost)); }
static uint8_t temperature_code_base(bool frost) {
  return frost ? FROST_TEMP_CODE_BASE : static_cast<uint8_t>(TOSHIBA_GENERIC_TEMP_C_MIN);
}
static float decode_target(const uint8_t *message, bool frost) {
  return (message[5] >> 4) + temperature_code_base(frost);
}
// Pulses encode_() writes for a frame sent repeat + 1 times: header, 8 bits per byte, footer mark and gap.
static constexpr size_t frame_pulses(uint8_t nbytes, uint8_t repeat) { return (repeat + 1u) * (16u * nbytes + 4u); }

void ToshibaRas25j2kvsgNdClimate::setup() {
  this->set_supported_custom_fan_modes_static(CUSTOM_FANS, std::size(CUSTOM_FANS));
  this->set_supported_custom_presets_static(CUSTOM_PRESETS, std::size(CUSTOM_PRESETS));
  this->presets_ = {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_ECO};
  this->set_preset_(climate::CLIMATE_PRESET_NONE);
  this->power_selection_pref_ = this->make_entity_preference<uint8_t>(POWER_SELECTION_RESTORE_VERSION);
  ToshibaPowerSelection power;
  if (this->power_selection_pref_.load(&power) && power < ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_COUNT)
    this->power_selection_ = power;
  ToshibaClimate::setup();
  // A fresh target follows the sensor over the range that includes frost targets.
  this->target_temperature =
      clamp_target(this->target_temperature, this->toshiba_preset_() == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION);
  this->publish_power_selection_();
}

void ToshibaRas25j2kvsgNdClimate::restore_(climate::ClimateDeviceRestoreState &restore) {
  if (!this->modes_.count(restore.mode))
    restore.mode = climate::CLIMATE_MODE_OFF;
  auto preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  if (restore.uses_custom_preset && restore.custom_preset < std::size(CUSTOM_PRESETS))
    preset = custom_preset_at(restore.custom_preset);
  // A restored duration is not the remaining time; never re-arm an OFF timer after reboot.
  if ((restore.uses_custom_preset && (preset == ToshibaPreset::TOSHIBA_PRESET_NONE || is_sleep_preset(preset) ||
                                      (is_heating_preset(preset) && restore.mode != climate::CLIMATE_MODE_HEAT))) ||
      (!restore.uses_custom_preset && !this->presets_.count(restore.preset)) ||
      (is_silent(this->power_selection_) &&
       (restore.uses_custom_preset || restore.preset != climate::CLIMATE_PRESET_NONE))) {
    restore.uses_custom_preset = false;
    restore.preset = climate::CLIMATE_PRESET_NONE;
    preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  }
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    restore.swing_mode = climate::CLIMATE_SWING_OFF;
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  // The restore state is packed, so clamp a copy rather than bind a reference to the field.
  const float target = restore.target_temperature;
  restore.target_temperature =
      std::isfinite(target) ? clamp_target(target, frost) : (frost ? FROST_TEMP_C_DEFAULT : 24.0f);
}

ToshibaRas25j2kvsgNdClimate::ToshibaPreset ToshibaRas25j2kvsgNdClimate::preset_from_climate(
    climate::ClimatePreset preset) {
  switch (preset) {
    case climate::CLIMATE_PRESET_BOOST:
      return ToshibaPreset::TOSHIBA_PRESET_BOOST;
    case climate::CLIMATE_PRESET_ECO:
      return ToshibaPreset::TOSHIBA_PRESET_ECO;
    default:
      return ToshibaPreset::TOSHIBA_PRESET_NONE;
  }
}

ToshibaRas25j2kvsgNdClimate::ToshibaPreset ToshibaRas25j2kvsgNdClimate::preset_from_custom(const char *custom_preset) {
  // Climate keeps a pointer into the supported table, so comparing pointers is enough.
  for (size_t i = 0; i < std::size(CUSTOM_PRESETS); i++) {
    if (custom_preset == progmem_read_ptr(&CUSTOM_PRESETS[i]))
      return custom_preset_at(i);
  }
  return ToshibaPreset::TOSHIBA_PRESET_NONE;
}

ToshibaRas25j2kvsgNdClimate::ToshibaPreset ToshibaRas25j2kvsgNdClimate::toshiba_preset_() const {
  if (this->has_custom_preset())
    return preset_from_custom(this->get_custom_preset().c_str());
  return preset_from_climate(this->preset.value_or(climate::CLIMATE_PRESET_NONE));
}

void ToshibaRas25j2kvsgNdClimate::apply_toshiba_preset_(ToshibaPreset preset) {
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
      this->set_custom_preset_(progmem_read_ptr(&CUSTOM_PRESETS[custom_preset_index(preset)]));
  }
}

void ToshibaRas25j2kvsgNdClimate::enter_preset_(ToshibaPreset previous, ToshibaPreset preset) {
  if (preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION && previous != preset)
    this->normal_target_temperature_ = static_cast<uint8_t>(this->target_temperature);
  if (is_sleep_preset(preset) && !is_sleep_preset(previous))
    this->sleep_fan_ = this->fan_code_();
}

uint8_t ToshibaRas25j2kvsgNdClimate::fan_code_() const {
  if (this->get_custom_fan_mode() == progmem_read_ptr(&CUSTOM_FANS[0]))
    return TOSHIBA_FAN_SPEED_2;
  if (this->get_custom_fan_mode() == progmem_read_ptr(&CUSTOM_FANS[1]))
    return TOSHIBA_FAN_SPEED_4;
  return this->encode_fan_();
}

void ToshibaRas25j2kvsgNdClimate::apply_fan_code_(uint8_t code) {
  switch (code) {
    case TOSHIBA_FAN_SPEED_2:
      this->set_custom_fan_mode_(progmem_read_ptr(&CUSTOM_FANS[0]));
      break;
    case TOSHIBA_FAN_SPEED_4:
      this->set_custom_fan_mode_(progmem_read_ptr(&CUSTOM_FANS[1]));
      break;
    default:
      this->set_fan_mode_(decode_fan(code));
  }
}

void ToshibaRas25j2kvsgNdClimate::control(const climate::ClimateCall &call) {
  const auto previous_preset = this->toshiba_preset_();
  auto preset = previous_preset;
  if (call.get_preset().has_value()) {
    preset = preset_from_climate(*call.get_preset());
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

  if (!power_preset_supported(this->power_selection_, preset))
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
  if (clamp_target(temperature, frost) != temperature) {
    ESP_LOGW(TAG, "Target must be %s",
             frost ? LOG_STR_LITERAL("5-13 C for frost protection") : LOG_STR_LITERAL("17-30 C for normal operation"));
    return;
  }

  this->enter_preset_(previous_preset, preset);
  const bool sleep = is_sleep_preset(preset);
  if (!sleep && is_sleep_preset(previous_preset))
    this->apply_fan_code_(this->sleep_fan_);
  if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
  } else if (call.has_custom_fan_mode()) {
    this->set_custom_fan_mode_(call.get_custom_fan_mode());
  }
  if (sleep || mode == climate::CLIMATE_MODE_DRY)
    this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);

  this->mode = mode;
  this->target_temperature = roundf(temperature);
  this->apply_toshiba_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  if (call.get_swing_mode().has_value())
    this->swing_mode = *call.get_swing_mode();

  const bool state_command = call.get_mode().has_value() || call.get_target_temperature().has_value() ||
                             call.get_fan_mode().has_value() || call.has_custom_fan_mode() ||
                             call.get_preset().has_value() || call.has_custom_preset();
  if (state_command) {
    this->transmit_frame_(preset);
  } else if (this->swing_mode != this->last_swing_mode_) {
    this->transmit_motion_(TOSHIBA_MOTION_SWING, 1);
  }
  this->publish_state();
}

void ToshibaRas25j2kvsgNdClimate::transmit_state() { this->transmit_frame_(this->toshiba_preset_()); }

void ToshibaRas25j2kvsgNdClimate::transmit_frame_(ToshibaPreset preset) {
  uint8_t message[MAX_FRAME_LENGTH] = {0, 0, 0, 0, TOSHIBA_COMMAND_DEFAULT};
  uint8_t length = 9;
  const bool frost = preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION;
  const auto power = this->power_selection_;
  message[5] = (static_cast<uint8_t>(this->target_temperature) - temperature_code_base(frost)) << 4;
  message[6] = this->encode_mode_() | this->fan_code_();
  if (power == ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_75 ||
      power == ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_50)
    message[7] = static_cast<uint8_t>(power) << 6;

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
  } else if (preset != ToshibaPreset::TOSHIBA_PRESET_NONE || is_silent(power)) {
    length = 10;
    message[4] = TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_POWER;
    if (preset == ToshibaPreset::TOSHIBA_PRESET_BOOST) {
      message[8] = TOSHIBA_POWER_HIGH;
    } else if (preset == ToshibaPreset::TOSHIBA_PRESET_ECO) {
      message[8] = TOSHIBA_POWER_ECO;
    } else if (frost) {
      message[8] = FEATURE_FROST;
    } else {
      message[8] = power == ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_SILENT_1 ? SILENT_1 : SILENT_2;
    }
  }
  finalize_frame(message, length);
  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  const bool swing = this->swing_mode != this->last_swing_mode_;
  data->reserve(frame_pulses(length, 1) + (swing ? frame_pulses(MOTION_FRAME_LENGTH, 1) : 0));
  this->encode_(data, message, length, 1);
  if (swing) {
    this->encode_motion_(data, TOSHIBA_MOTION_SWING, 1);
    this->last_swing_mode_ = this->swing_mode;
  }
  this->perform_(transmit, swing);
}

void ToshibaRas25j2kvsgNdClimate::transmit_motion_(uint8_t command, uint8_t repeat) {
  auto transmit = this->transmitter_->transmit();
  transmit.get_data()->reserve(frame_pulses(MOTION_FRAME_LENGTH, repeat));
  this->encode_motion_(transmit.get_data(), command, repeat);
  this->last_swing_mode_ = this->swing_mode;
  this->perform_(transmit, command == TOSHIBA_MOTION_SWING);
}

void ToshibaRas25j2kvsgNdClimate::encode_motion_(remote_base::RemoteTransmitData *data, uint8_t command,
                                                 uint8_t repeat) {
  uint8_t message[MOTION_FRAME_LENGTH] = {0, 0, 0, 0, SHORT_MOTION_COMMAND, command};
  finalize_frame(message, sizeof(message));
  this->encode_(data, message, sizeof(message), repeat);
}

void ToshibaRas25j2kvsgNdClimate::perform_(remote_base::RemoteTransmitterBase::TransmitCall &transmit, bool swing) {
  uint32_t duration_us = 0;
  if (swing) {
    for (const int32_t pulse : transmit.get_data()->get_data())
      duration_us += static_cast<uint32_t>(std::abs(pulse));
  }
  transmit.perform();
  // perform() may wait for an earlier send before queuing this one, or block until this send finishes.
  if (swing)
    this->swing_ignore_until_ = millis() + (duration_us + 999) / 1000 + SWING_REPEAT_MS;
}

bool ToshibaRas25j2kvsgNdClimate::receive_motion_(uint8_t command) {
  if (command != TOSHIBA_MOTION_SWING)
    return command == TOSHIBA_MOTION_FIX;
  const uint32_t now = millis();
  // Windows last well under 65 s, so a larger remainder is a deadline that has already passed.
  const uint32_t remaining = this->swing_ignore_until_ - now;
  if (remaining != 0 && remaining <= UINT16_MAX)
    return true;
  this->swing_ignore_until_ = now + SWING_REPEAT_MS;
  this->swing_mode =
      this->swing_mode == climate::CLIMATE_SWING_OFF ? climate::CLIMATE_SWING_VERTICAL : climate::CLIMATE_SWING_OFF;
  this->last_swing_mode_ = this->swing_mode;
  this->publish_state();
  return true;
}

bool ToshibaRas25j2kvsgNdClimate::parse_state(const uint8_t *message, uint8_t length, ToshibaPreset &preset,
                                              ToshibaPowerSelection &power) {
  const uint8_t mode = message[6] & 0x0F;
  const uint8_t feature = message[7] & 0x03;
  if ((message[5] & 0x0F) != 0 || (mode > 4 && mode != 7) || (message[6] & 0xF0) > 0xC0 || (message[6] & 0x10) != 0 ||
      (message[7] & 0x3C) != 0 || (message[7] & 0xC0) == 0xC0 || feature == 1)
    return false;

  preset = ToshibaPreset::TOSHIBA_PRESET_NONE;
  power = static_cast<ToshibaPowerSelection>(message[7] >> 6);
  if (length == 9) {
    if (message[4] != TOSHIBA_COMMAND_DEFAULT)
      return false;
    if (feature == FIREPLACE_1) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_1;
    } else if (feature == FIREPLACE_2) {
      preset = ToshibaPreset::TOSHIBA_PRESET_FIREPLACE_2;
    }
  } else if (feature != 0) {
    return false;
  } else if (length == 10 && message[4] == (TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_POWER)) {
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
        if (power != ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_100)
          return false;
        power = message[8] == SILENT_1 ? ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_SILENT_1
                                       : ToshibaPowerSelection::TOSHIBA_POWER_SELECTION_SILENT_2;
        break;
      default:
        return false;
    }
  } else if (length == 11 && message[4] == TOSHIBA_COMMAND_DEFAULT && message[8] == HADA_PREFIX &&
             message[9] == HADA_SUFFIX) {
    preset = ToshibaPreset::TOSHIBA_PRESET_HADA_CARE;
  } else if (length == 12 && message[4] == (TOSHIBA_COMMAND_DEFAULT | TOSHIBA_COMMAND_TIMER | TOSHIBA_COMMAND_POWER) &&
             message[9] == SLEEP_TIMER_MARKER && message[10] == SLEEP_TIMER_FLAGS && (message[6] & 0xF0) == 0) {
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
  if (clamp_target(temperature, frost) != temperature)
    return false;
  // Heating presets only exist in HEAT mode.
  return !is_heating_preset(preset) || mode == 3;
}

bool ToshibaRas25j2kvsgNdClimate::on_receive(remote_base::RemoteReceiveData data) {
  bool received = false;
  uint8_t previous[MAX_FRAME_LENGTH];
  uint8_t previous_length = 0;
  do {
    uint8_t message[MAX_FRAME_LENGTH]{};
    if (!data.expect_item(TOSHIBA_HEADER_MARK, TOSHIBA_HEADER_SPACE) ||
        !this->decode_(&data, message, TOSHIBA_HEADER_LENGTH) || !frame_header_valid(message) || message[2] < 1 ||
        message[2] > 6 || message[2] == 2)
      return received;
    const uint8_t length = message[2] + 6;
    if (!this->decode_(&data, message + TOSHIBA_HEADER_LENGTH, length - TOSHIBA_HEADER_LENGTH) ||
        !data.expect_mark(TOSHIBA_BIT_MARK) || !frame_checksum_valid(message, length))
      return received;
    // The remote sends every frame twice; apply and publish each press once.
    if (length != previous_length || memcmp(message, previous, length) != 0) {
      if (!this->receive_frame_(message, length))
        return received;
      memcpy(previous, message, length);
      previous_length = length;
    }
    received = true;
  } while (data.expect_space(TOSHIBA_GAP_SPACE) && data.is_valid());
  return received;
}

bool ToshibaRas25j2kvsgNdClimate::receive_frame_(const uint8_t *message, uint8_t length) {
  if (length == MOTION_FRAME_LENGTH)
    return message[4] == SHORT_MOTION_COMMAND && this->receive_motion_(message[5]);
  ToshibaPreset preset;
  ToshibaPowerSelection power;
  if (!parse_state(message, length, preset, power))
    return false;

  const auto previous_preset = this->toshiba_preset_();
  this->enter_preset_(previous_preset, preset);
  this->decode_mode_(message);
  this->apply_fan_code_(message[6] & 0xF0);
  // FAN ONLY has no displayed setpoint; its temperature byte is not a new target.
  if (this->mode != climate::CLIMATE_MODE_FAN_ONLY) {
    this->target_temperature = decode_target(message, preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION);
  } else if (previous_preset == ToshibaPreset::TOSHIBA_PRESET_FROST_PROTECTION) {
    this->target_temperature = this->normal_target_temperature_;
  }
  this->apply_toshiba_preset_(preset);
  if (preset == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE)
    this->swing_mode = this->last_swing_mode_ = climate::CLIMATE_SWING_OFF;
  if (this->store_power_selection_(power))
    this->publish_power_selection_();
  this->publish_state();
  return true;
}

void ToshibaRas25j2kvsgNdClimate::fix() {
  if (this->toshiba_preset_() == ToshibaPreset::TOSHIBA_PRESET_HADA_CARE) {
    ESP_LOGW(TAG, "FIX is unavailable during HADA Care");
    return;
  }
  this->transmit_motion_(TOSHIBA_MOTION_FIX, 0);
}

void ToshibaRas25j2kvsgNdClimate::set_power_selection(size_t index) {
  const auto power = static_cast<ToshibaPowerSelection>(index);
  const auto preset = this->toshiba_preset_();
  if (!power_preset_supported(power, preset))
    return;
  this->store_power_selection_(power);
  this->transmit_frame_(preset);
  // Published after sending, so a preset change from the select's on_value goes out as its own frame.
  this->publish_power_selection_();
}

bool ToshibaRas25j2kvsgNdClimate::power_preset_supported(ToshibaPowerSelection power, ToshibaPreset preset) {
  if (is_silent(power) && preset != ToshibaPreset::TOSHIBA_PRESET_NONE) {
    ESP_LOGW(TAG, "Outdoor Silent with a preset is not supported");
    return false;
  }
  return true;
}

bool ToshibaRas25j2kvsgNdClimate::store_power_selection_(ToshibaPowerSelection power) {
  if (this->power_selection_ == power)
    return false;
  this->power_selection_ = power;
  this->power_selection_pref_.save(&this->power_selection_);
  return true;
}

void ToshibaRas25j2kvsgNdClimate::publish_power_selection_() {
#ifdef USE_SELECT
  if (this->power_selection_select_ != nullptr)
    this->power_selection_select_->publish_state(static_cast<size_t>(this->power_selection_));
#endif
}

}  // namespace esphome::toshiba

#endif  // USE_TOSHIBA_RAS_25J2KVSG_ND
