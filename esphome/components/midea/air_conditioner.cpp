#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "air_conditioner.h"
#include "ac_adapter.h"
#include <cmath>
#include <cstdint>
#include <utility>

namespace esphome::midea::ac {

static void set_sensor(Sensor *sensor, float value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->get_raw_state() != value))
    sensor->publish_state(value);
}

template<typename T> void update_property(T &property, const T &value, bool &flag) {
  if (property != value) {
    property = value;
    flag = true;
  }
}

void AirConditioner::on_status_change() {
  // Add frost protection custom preset once when autoconf completes
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK &&
      this->base_.getCapabilities().supportFrostProtectionPreset() && !this->frost_protection_set_) {
    // Read existing presets (set by codegen), append frost protection, write back
    auto traits = this->get_traits();
    const auto &existing = traits.get_supported_custom_presets();
    bool found = false;
    for (const char *p : existing) {
      if (strcmp(p, Constants::FREEZE_PROTECTION) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      std::vector<const char *> merged(existing.begin(), existing.end());
      merged.push_back(Constants::FREEZE_PROTECTION);
      this->set_supported_custom_presets(merged);
    }
    this->frost_protection_set_ = true;
  }
  bool need_publish = false;
  update_property(this->target_temperature, this->base_.getTargetTemp(), need_publish);
  update_property(this->current_temperature, this->base_.getIndoorTemp(), need_publish);
  auto mode = Converters::to_climate_mode(this->base_.getMode());
  update_property(this->mode, mode, need_publish);
  auto swing_mode = Converters::to_climate_swing_mode(this->base_.getSwingMode());
  update_property(this->swing_mode, swing_mode, need_publish);
  // Preset
  auto preset = this->base_.getPreset();
  if (Converters::is_custom_midea_preset(preset)) {
    if (this->set_custom_preset_(Converters::to_custom_climate_preset(preset)))
      need_publish = true;
  } else if (this->set_preset_(Converters::to_climate_preset(preset))) {
    need_publish = true;
  }
  // Fan mode
  auto fan_mode = this->base_.getFanMode();
  if (Converters::is_custom_midea_fan_mode(fan_mode)) {
    if (this->set_custom_fan_mode_(Converters::to_custom_climate_fan_mode(fan_mode)))
      need_publish = true;
  } else if (this->set_fan_mode_(Converters::to_climate_fan_mode(fan_mode))) {
    need_publish = true;
  }
  if (need_publish)
    this->publish_state();
  set_sensor(this->outdoor_temperature_sensor_, this->base_.getOutdoorTemp());
  set_sensor(this->power_sensor_, this->base_.getPowerUsage());
  set_sensor(this->humidity_setpoint_sensor_, this->base_.getIndoorHum());
  this->publish_appliance_state_();
}

/* ################ */
/* ### ENTITIES ### */
/* ################ */

// Values of the select options, in option order
static constexpr uint8_t SWING_ANGLE_VALUES[] = {0, 1, 25, 50, 75, 100};
static constexpr uint8_t CASCADE_VALUES[] = {0, 1, 2};
static constexpr uint8_t FRESH_AIR_VALUES[] = {0, 40, 60, 80, 100};
static constexpr uint8_t RATE_SELECT_2_VALUES[] = {100, 75, 50};
static constexpr uint8_t RATE_SELECT_5_VALUES[] = {100, 80, 60, 40, 20, 1};
static constexpr uint8_t BREEZE_MODE_VALUES[] = {1, 2, 3, 4};
static constexpr uint8_t BREEZE_AWAY_DIRECTION_VALUES[] = {0, 2, 3};

struct ValueTable {
  const uint8_t *values;
  uint8_t size;
};
template<size_t N> static constexpr ValueTable table(const uint8_t (&values)[N]) {
  return {values, static_cast<uint8_t>(N)};
}

static MideaProperty select_property(MideaSelect setting) {
  switch (setting) {
    case MideaSelect::MIDEA_SELECT_VERTICAL_SWING_ANGLE:
      return MideaProperty::MIDEA_PROPERTY_SWING_UD_ANGLE;
    case MideaSelect::MIDEA_SELECT_HORIZONTAL_SWING_ANGLE:
      return MideaProperty::MIDEA_PROPERTY_SWING_LR_ANGLE;
    case MideaSelect::MIDEA_SELECT_CASCADE:
      return MideaProperty::MIDEA_PROPERTY_CASCADE;
    case MideaSelect::MIDEA_SELECT_FRESH_AIR:
      return MideaProperty::MIDEA_PROPERTY_FRESH_AIR;
    case MideaSelect::MIDEA_SELECT_RATE_SELECT:
      return MideaProperty::MIDEA_PROPERTY_RATE_SELECT;
    case MideaSelect::MIDEA_SELECT_BREEZE_MODE:
      return MideaProperty::MIDEA_PROPERTY_BREEZE_CONTROL;
    case MideaSelect::MIDEA_SELECT_BREEZE_AWAY_DIRECTION:
      return MideaProperty::MIDEA_PROPERTY_BREEZE_AWAY_DIRECTION;
    case MideaSelect::MIDEA_SELECT_AUX_HEAT_MODE:
      break;  // a status field, not a property
  }
  return MideaProperty::MIDEA_PROPERTY_COUNT;
}

static ValueTable select_values(MideaSelect setting, uint8_t rate_select_levels) {
  switch (setting) {
    case MideaSelect::MIDEA_SELECT_VERTICAL_SWING_ANGLE:
    case MideaSelect::MIDEA_SELECT_HORIZONTAL_SWING_ANGLE:
      return table(SWING_ANGLE_VALUES);
    case MideaSelect::MIDEA_SELECT_CASCADE:
      return table(CASCADE_VALUES);
    case MideaSelect::MIDEA_SELECT_FRESH_AIR:
      return table(FRESH_AIR_VALUES);
    case MideaSelect::MIDEA_SELECT_RATE_SELECT:
      return rate_select_levels == 2 ? table(RATE_SELECT_2_VALUES) : table(RATE_SELECT_5_VALUES);
    case MideaSelect::MIDEA_SELECT_BREEZE_MODE:
      return table(BREEZE_MODE_VALUES);
    case MideaSelect::MIDEA_SELECT_BREEZE_AWAY_DIRECTION:
      return table(BREEZE_AWAY_DIRECTION_VALUES);
    case MideaSelect::MIDEA_SELECT_AUX_HEAT_MODE:
      break;  // options are the MideaAuxHeatMode values
  }
  return {nullptr, 0};
}

static MideaProperty switch_property(MideaSwitch setting) {
  switch (setting) {
    case MideaSwitch::MIDEA_SWITCH_JET_COOL:
      return MideaProperty::MIDEA_PROPERTY_JET_COOL;
    case MideaSwitch::MIDEA_SWITCH_BREEZE_AWAY:
      return MideaProperty::MIDEA_PROPERTY_BREEZE_AWAY;
    case MideaSwitch::MIDEA_SWITCH_BREEZELESS:
      return MideaProperty::MIDEA_PROPERTY_BREEZELESS;
    case MideaSwitch::MIDEA_SWITCH_OUT_SILENT:
      return MideaProperty::MIDEA_PROPERTY_OUT_SILENT;
    case MideaSwitch::MIDEA_SWITCH_IECO:
      return MideaProperty::MIDEA_PROPERTY_IECO;
    case MideaSwitch::MIDEA_SWITCH_SOUND:
      return MideaProperty::MIDEA_PROPERTY_SOUND;
    case MideaSwitch::MIDEA_SWITCH_PURIFIER:
    case MideaSwitch::MIDEA_SWITCH_FAHRENHEIT:
      break;  // status fields, not properties
  }
  return MideaProperty::MIDEA_PROPERTY_COUNT;
}

void AirConditioner::setup() {
  this->request_appliance_data_();
  ApplianceBase::setup();
}

// Poll only what the configured entities need
void AirConditioner::request_appliance_data_() {
  auto &b = this->base_;
  if (this->compressor_frequency_sensor_ != nullptr || this->compressor_target_frequency_sensor_ != nullptr ||
      this->compressor_current_sensor_ != nullptr || this->compressor_voltage_sensor_ != nullptr ||
      this->indoor_coil_temperature_sensor_ != nullptr || this->outdoor_coil_temperature_sensor_ != nullptr ||
      this->discharge_temperature_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_1);
  bool group2 = this->indoor_fan_speed_sensor_ != nullptr || this->indoor_fan_target_speed_sensor_ != nullptr;
  bool group5 = this->indoor_humidity_sensor_ != nullptr || this->outdoor_fan_speed_sensor_ != nullptr;
#ifdef USE_BINARY_SENSOR
  group2 |= this->water_pump_binary_sensor_ != nullptr;
  group5 |= this->defrost_binary_sensor_ != nullptr;
  if (this->self_cleaning_binary_sensor_ != nullptr)
    b.request_property(MideaProperty::MIDEA_PROPERTY_SELF_CLEAN);
#endif
  if (group2)
    b.request_group(MideaGroup::MIDEA_GROUP_2);
  if (group5)
    b.request_group(MideaGroup::MIDEA_GROUP_5);
  if (this->total_energy_sensor_ != nullptr || this->current_energy_sensor_ != nullptr ||
      this->real_time_power_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_4);
  if (this->outdoor_unit_power_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_7);
  if (this->horizontal_louver_angle_sensor_ != nullptr || this->vertical_louver_angle_sensor_ != nullptr)
    b.request_group(MideaGroup::MIDEA_GROUP_11);
#ifdef USE_SWITCH
  const std::pair<switch_::Switch *, MideaSwitch> switches[] = {
      {this->jet_cool_switch_, MideaSwitch::MIDEA_SWITCH_JET_COOL},
      {this->breeze_away_switch_, MideaSwitch::MIDEA_SWITCH_BREEZE_AWAY},
      {this->breezeless_switch_, MideaSwitch::MIDEA_SWITCH_BREEZELESS},
      {this->out_silent_switch_, MideaSwitch::MIDEA_SWITCH_OUT_SILENT},
      {this->ieco_switch_, MideaSwitch::MIDEA_SWITCH_IECO},
      {this->sound_switch_, MideaSwitch::MIDEA_SWITCH_SOUND},
  };
  for (const auto &[sw, setting] : switches) {
    if (sw != nullptr)
      b.request_property(switch_property(setting));
  }
#endif
#ifdef USE_SELECT
  const std::pair<select::Select *, MideaSelect> selects[] = {
      {this->vertical_swing_angle_select_, MideaSelect::MIDEA_SELECT_VERTICAL_SWING_ANGLE},
      {this->horizontal_swing_angle_select_, MideaSelect::MIDEA_SELECT_HORIZONTAL_SWING_ANGLE},
      {this->cascade_select_, MideaSelect::MIDEA_SELECT_CASCADE},
      {this->fresh_air_select_, MideaSelect::MIDEA_SELECT_FRESH_AIR},
      {this->rate_select_select_, MideaSelect::MIDEA_SELECT_RATE_SELECT},
      {this->breeze_mode_select_, MideaSelect::MIDEA_SELECT_BREEZE_MODE},
      {this->breeze_away_direction_select_, MideaSelect::MIDEA_SELECT_BREEZE_AWAY_DIRECTION},
  };
  for (const auto &[sel, setting] : selects) {
    if (sel != nullptr)
      b.request_property(select_property(setting));
  }
#endif
}

static void publish_known(Sensor *sensor, float value) {
  if (std::isfinite(value))
    set_sensor(sensor, value);
}

#ifdef USE_BINARY_SENSOR
static void publish_binary(binary_sensor::BinarySensor *sensor, bool value) {
  if (sensor != nullptr && (!sensor->has_state() || sensor->state != value))
    sensor->publish_state(value);
}
#endif

#ifdef USE_SWITCH
static void publish_switch(switch_::Switch *sw, bool value) {
  if (sw != nullptr && (!sw->has_state() || sw->state != value))
    sw->publish_state(value);
}
#endif

#ifdef USE_SELECT
static void publish_select(select::Select *sel, size_t index) {
  if (sel == nullptr)
    return;
  auto active = sel->active_index();
  if (!active.has_value() || *active != index)
    sel->publish_state(index);
}

// Index of the option for a reported value: the exact one, or with `nearest` the closest; -1 if none
static int find_option(ValueTable values, uint8_t value, bool nearest) {
  int best = -1;
  int best_diff = nearest ? 256 : 1;
  for (uint8_t idx = 0; idx < values.size; ++idx) {
    const int diff = values.values[idx] > value ? values.values[idx] - value : value - values.values[idx];
    if (diff < best_diff) {
      best = idx;
      best_diff = diff;
    }
  }
  return best;
}
#endif

#ifdef USE_NUMBER
static void publish_number(number::Number *num, float value) {
  if (num == nullptr)
    return;
  // Publish on change; NAN (unknown) equals NAN.
  if (!num->has_state() || (num->state != value && (!std::isnan(num->state) || !std::isnan(value))))
    num->publish_state(value);
}
#endif

void AirConditioner::publish_appliance_state_() {
  const auto &b = this->base_;
  const auto &e = b.get_appliance_state();
  if (e.status_valid)
    set_sensor(this->error_code_sensor_, e.error_code);
  publish_known(this->compressor_frequency_sensor_, e.compressor_frequency);
  publish_known(this->compressor_target_frequency_sensor_, e.compressor_target_frequency);
  publish_known(this->compressor_current_sensor_, e.compressor_current);
  publish_known(this->compressor_voltage_sensor_, e.compressor_voltage);
  publish_known(this->indoor_coil_temperature_sensor_, e.indoor_coil_temperature);
  publish_known(this->outdoor_coil_temperature_sensor_, e.outdoor_coil_temperature);
  publish_known(this->discharge_temperature_sensor_, e.discharge_temperature);
  publish_known(this->indoor_fan_speed_sensor_, e.indoor_fan_speed);
  publish_known(this->indoor_fan_target_speed_sensor_, e.indoor_fan_target_speed);
  publish_known(this->total_energy_sensor_, e.total_energy);
  publish_known(this->current_energy_sensor_, e.current_energy);
  publish_known(this->real_time_power_sensor_, e.real_time_power);
  publish_known(this->indoor_humidity_sensor_, e.indoor_humidity);
  publish_known(this->outdoor_fan_speed_sensor_, e.outdoor_fan_speed);
  publish_known(this->outdoor_unit_power_sensor_, e.outdoor_unit_power);
  publish_known(this->horizontal_louver_angle_sensor_, e.horizontal_louver_angle);
  publish_known(this->vertical_louver_angle_sensor_, e.vertical_louver_angle);
#ifdef USE_BINARY_SENSOR
  if (e.defrost >= 0)
    publish_binary(this->defrost_binary_sensor_, e.defrost);
  if (e.water_pump >= 0)
    publish_binary(this->water_pump_binary_sensor_, e.water_pump);
  if (e.status_valid) {
    publish_binary(this->filter_alert_binary_sensor_, e.filter_alert);
    publish_binary(this->display_binary_sensor_, e.display_on);
    publish_binary(this->follow_me_binary_sensor_, e.follow_me);
  }
  if (b.has_property(MideaProperty::MIDEA_PROPERTY_SELF_CLEAN))
    publish_binary(this->self_cleaning_binary_sensor_, b.get_property(MideaProperty::MIDEA_PROPERTY_SELF_CLEAN));
#endif
#ifdef USE_SWITCH
  if (e.status_valid) {
    publish_switch(this->purifier_switch_, e.purifier);
    publish_switch(this->fahrenheit_switch_, e.fahrenheit);
  }
  const std::pair<switch_::Switch *, MideaSwitch> switches[] = {
      {this->jet_cool_switch_, MideaSwitch::MIDEA_SWITCH_JET_COOL},
      {this->breeze_away_switch_, MideaSwitch::MIDEA_SWITCH_BREEZE_AWAY},
      {this->breezeless_switch_, MideaSwitch::MIDEA_SWITCH_BREEZELESS},
      {this->out_silent_switch_, MideaSwitch::MIDEA_SWITCH_OUT_SILENT},
      {this->ieco_switch_, MideaSwitch::MIDEA_SWITCH_IECO},
      {this->sound_switch_, MideaSwitch::MIDEA_SWITCH_SOUND},
  };
  for (const auto &[sw, setting] : switches) {
    const MideaProperty prop = switch_property(setting);
    if (b.has_property(prop))
      publish_switch(sw, b.get_property(prop) != 0);
  }
#endif
#ifdef USE_SELECT
  if (e.status_valid)
    publish_select(this->aux_heat_mode_select_, static_cast<size_t>(e.aux_heat_mode));
  const std::pair<select::Select *, MideaSelect> selects[] = {
      {this->vertical_swing_angle_select_, MideaSelect::MIDEA_SELECT_VERTICAL_SWING_ANGLE},
      {this->horizontal_swing_angle_select_, MideaSelect::MIDEA_SELECT_HORIZONTAL_SWING_ANGLE},
      {this->cascade_select_, MideaSelect::MIDEA_SELECT_CASCADE},
      {this->fresh_air_select_, MideaSelect::MIDEA_SELECT_FRESH_AIR},
      {this->rate_select_select_, MideaSelect::MIDEA_SELECT_RATE_SELECT},
      {this->breeze_mode_select_, MideaSelect::MIDEA_SELECT_BREEZE_MODE},
      {this->breeze_away_direction_select_, MideaSelect::MIDEA_SELECT_BREEZE_AWAY_DIRECTION},
  };
  for (const auto &[sel, setting] : selects) {
    const MideaProperty prop = select_property(setting);
    if (sel == nullptr || !b.has_property(prop))
      continue;
    const uint8_t value = b.get_property(prop);
    // Angles and fresh air speeds can be set to values between the options, e.g. from the remote
    const bool nearest = setting == MideaSelect::MIDEA_SELECT_VERTICAL_SWING_ANGLE ||
                         setting == MideaSelect::MIDEA_SELECT_HORIZONTAL_SWING_ANGLE ||
                         setting == MideaSelect::MIDEA_SELECT_FRESH_AIR;
    const int option = find_option(select_values(setting, this->rate_select_levels_), value, nearest);
    if (option >= 0) {
      publish_select(sel, option);
      this->unmapped_values_[static_cast<uint8_t>(setting)] = 0xFF;
    } else if (this->unmapped_values_[static_cast<uint8_t>(setting)] != value) {
      // The select keeps its option; log each new value once (a wrong rate_select levels shows up here)
      this->unmapped_values_[static_cast<uint8_t>(setting)] = value;
      ESP_LOGW(Constants::TAG, "%s: reported value %u has no option", LOG_STR_ARG(sel->get_log_name()), value);
    }
  }
#endif
#ifdef USE_NUMBER
  if (e.status_valid) {
    // 102 is auto; the number only shows a manual speed and is unknown otherwise
    publish_number(this->fan_speed_number_, e.fan_speed >= 1 && e.fan_speed <= 100 ? e.fan_speed : NAN);
    if (e.target_humidity != 0)
      publish_number(this->target_humidity_number_, e.target_humidity);
  }
#endif
}

void AirConditioner::control_switch(MideaSwitch setting, bool state) {
  switch (setting) {
    case MideaSwitch::MIDEA_SWITCH_PURIFIER:
      this->base_.set_purifier(state);
      break;
    case MideaSwitch::MIDEA_SWITCH_FAHRENHEIT:
      this->base_.set_fahrenheit(state);
      break;
    default:
      this->base_.set_property(switch_property(setting), state ? 1 : 0);
      break;
  }
}

void AirConditioner::control_select(MideaSelect setting, size_t index) {
  if (setting == MideaSelect::MIDEA_SELECT_AUX_HEAT_MODE) {
    this->base_.set_aux_heat_mode(static_cast<MideaAuxHeatMode>(index));
    return;
  }
  const ValueTable values = select_values(setting, this->rate_select_levels_);
  if (index < values.size)
    this->base_.set_property(select_property(setting), values.values[index]);
}

void AirConditioner::control(const ClimateCall &call) {
  dudanov::midea::ac::Control ctrl{};
  auto target_temp_val = call.get_target_temperature();
  if (target_temp_val.has_value())
    ctrl.targetTemp = *target_temp_val;
  auto swing_mode_val = call.get_swing_mode();
  if (swing_mode_val.has_value())
    ctrl.swingMode = Converters::to_midea_swing_mode(*swing_mode_val);
  auto mode_val = call.get_mode();
  if (mode_val.has_value())
    ctrl.mode = Converters::to_midea_mode(*mode_val);
  auto preset_val = call.get_preset();
  if (preset_val.has_value()) {
    ctrl.preset = Converters::to_midea_preset(*preset_val);
  } else if (call.has_custom_preset()) {
    // get_custom_preset() returns StringRef pointing to null-terminated string literals from codegen
    ctrl.preset = Converters::to_midea_preset(call.get_custom_preset().c_str());
  }
  auto fan_mode_val = call.get_fan_mode();
  if (fan_mode_val.has_value()) {
    ctrl.fanMode = Converters::to_midea_fan_mode(*fan_mode_val);
  } else if (call.has_custom_fan_mode()) {
    // get_custom_fan_mode() returns StringRef pointing to null-terminated string literals from codegen
    ctrl.fanMode = Converters::to_midea_fan_mode(call.get_custom_fan_mode().c_str());
  }
  this->base_.control(ctrl);
}

ClimateTraits AirConditioner::traits() {
  auto traits = ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE);
  traits.set_visual_min_temperature(17);
  traits.set_visual_max_temperature(30);
  traits.set_visual_temperature_step(0.5);
  traits.set_supported_modes(this->supported_modes_);
  traits.set_supported_swing_modes(this->supported_swing_modes_);
  traits.set_supported_presets(this->supported_presets_);
  // Custom fan modes and presets are stored on Climate base class and wired via get_traits()
  /* + MINIMAL SET OF CAPABILITIES */
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(ClimateFanMode::CLIMATE_FAN_HIGH);
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK) {
    Converters::to_climate_traits(traits, this->base_.getCapabilities());
  }
  if (!traits.get_supported_modes().empty())
    traits.add_supported_mode(ClimateMode::CLIMATE_MODE_OFF);
  if (!traits.get_supported_swing_modes().empty())
    traits.add_supported_swing_mode(ClimateSwingMode::CLIMATE_SWING_OFF);
  if (!traits.get_supported_presets().empty())
    traits.add_supported_preset(ClimatePreset::CLIMATE_PRESET_NONE);
  return traits;
}

#ifdef USE_REMOTE_TRANSMITTER
static const LogString *display_control_str(DisplayControl control) {
  switch (control) {
    case DisplayControl::DISPLAY_CONTROL_UART:
      return LOG_STR("UART");
    case DisplayControl::DISPLAY_CONTROL_IR:
      return LOG_STR("IR");
    default:
      return LOG_STR("AUTO");
  }
}
#endif

void AirConditioner::dump_config() {
  ESP_LOGCONFIG(Constants::TAG,
                "MideaDongle:\n"
                "  [x] Period: %" PRIu32 "ms\n"
                "  [x] Response timeout: %" PRIu32 "ms\n"
                "  [x] Request attempts: %d",
                this->base_.getPeriod(), this->base_.getTimeout(), this->base_.getNumAttempts());
#ifdef USE_REMOTE_TRANSMITTER
  ESP_LOGCONFIG(Constants::TAG,
                "  [x] Using RemoteTransmitter\n"
                "  [x] Display control: %s",
                LOG_STR_ARG(display_control_str(this->display_control_)));
#endif
  if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_OK) {
    this->base_.getCapabilities().dump();
  } else if (this->base_.getAutoconfStatus() == dudanov::midea::AUTOCONF_ERROR) {
    ESP_LOGW(Constants::TAG,
             "Failed to get 0xB5 capabilities report. Suggest to disable it in config and manually set your "
             "appliance options.");
  }
  this->dump_traits_(Constants::TAG);
}

/* ACTIONS */

void AirConditioner::do_follow_me(float temperature, bool use_fahrenheit, bool beeper) {
#ifdef USE_REMOTE_TRANSMITTER
  // Check if temperature is finite (not NaN or infinite)
  if (!std::isfinite(temperature)) {
    ESP_LOGW(Constants::TAG, "Follow me action requires a finite temperature, got: %f", temperature);
    return;
  }

  // Clamp to the uint8_t range before rounding so lroundf never sees an out-of-range value
  uint8_t temp_uint8 = static_cast<uint8_t>(std::lroundf(esphome::clamp(temperature, 0.0f, 255.0f)));

  char temp_symbol = use_fahrenheit ? 'F' : 'C';
  ESP_LOGD(Constants::TAG, "Follow me action called with temperature: %.5f °%c, rounded to: %u °%c", temperature,
           temp_symbol, temp_uint8, temp_symbol);

  // Create and transmit the data
  IrFollowMeData data(temp_uint8, use_fahrenheit, beeper);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_swing_step() {
#ifdef USE_REMOTE_TRANSMITTER
  IrSpecialData data(0x01);
  this->transmitter_.transmit(data);
#else
  ESP_LOGW(Constants::TAG, "Action needs remote_transmitter component");
#endif
}

void AirConditioner::do_display_toggle() {
#ifdef USE_REMOTE_TRANSMITTER
  // AUTO uses IR unless the appliance reports light control over UART. Without a transmitter
  // built in, every setting sends the UART request: Mundoclima, Rotenso and others accept it
  // without advertising it in the 0xB5 capabilities report.
  const bool use_ir = this->display_control_ == DisplayControl::DISPLAY_CONTROL_IR ||
                      (this->display_control_ == DisplayControl::DISPLAY_CONTROL_AUTO &&
                       !this->base_.getCapabilities().supportLightControl());
  if (use_ir) {
    IrSpecialData data(0x08);
    this->transmitter_.transmit(data);
    return;
  }
#endif
  this->base_.displayToggle();
}

using dudanov::midea::FrameData;
using dudanov::midea::FrameType;
using dudanov::midea::ResponseStatus;
using dudanov::midea::ac::StatusData;

static constexpr uint8_t GROUP_IDS[] = {1, 2, 4, 5, 7, 11};
static constexpr uint16_t PROPERTY_IDS[] = {
    0x0009,  // SWING_UD_ANGLE
    0x000A,  // SWING_LR_ANGLE
    0x0018,  // BREEZELESS
    0x0039,  // SELF_CLEAN
    0x0042,  // BREEZE_AWAY
    0x0043,  // BREEZE_CONTROL
    0x0048,  // RATE_SELECT
    0x004B,  // FRESH_AIR
    0x0058,  // BREEZE_AWAY_DIRECTION
    0x0059,  // CASCADE
    0x0067,  // FLASH (jet cool)
    0x00CD,  // OUT_SILENT
    0x00E3,  // IECO
    0x022C,  // SOUND
};
static_assert(sizeof(GROUP_IDS) == static_cast<uint8_t>(MideaGroup::MIDEA_GROUP_COUNT));
static_assert(sizeof(PROPERTY_IDS) / sizeof(PROPERTY_IDS[0]) ==
              static_cast<uint8_t>(MideaProperty::MIDEA_PROPERTY_COUNT));
static constexpr uint16_t PROPERTY_ID_BUZZER = 0x001A;

/* ################## */
/* ### FRAME DATA ### */
/* ################## */

// 0x41 group data query: {0x41, 0x21, 0x01, 0x40 | group, 0...} as sent by midea-msmart
class GroupQueryData : public FrameData {
 public:
  explicit GroupQueryData(uint8_t group) : FrameData(20) {
    this->m_data[0] = 0x41;
    this->m_data[1] = 0x21;
    this->m_data[2] = 0x01;
    this->m_data[3] = 0x40 | group;
    this->m_data.push_back(FrameData::m_getID());
    this->appendCRC();
  }
};

// 0xB1 get properties and 0xB0 set properties requests
class PropertiesData : public FrameData {
 public:
  explicit PropertiesData(uint8_t id) : FrameData({id, 0x00}) {}
  void add_id(uint16_t id) {
    this->m_data.push_back(id & 0xFF);
    this->m_data.push_back(id >> 8);
    ++this->m_data[1];
  }
  void add_value(uint16_t id, const uint8_t *value, uint8_t size) {
    this->add_id(id);
    this->m_data.push_back(size);
    this->m_data.insert(this->m_data.end(), value, value + size);
  }
  void finish() {
    this->m_data.push_back(FrameData::m_getID());
    this->appendCRC();
  }
};

// 0x40 set status request with the fields MideaUART does not expose
class StatusRequest : public StatusData {
 public:
  explicit StatusRequest(const StatusData &status) : StatusData(status) {}
  void set_fan_speed(uint8_t speed) { this->m_setValue(3, speed); }
  void set_purifier(bool state) { this->m_setMask(9, state, 0x20); }
  void set_aux_heat(bool state) { this->m_setMask(9, state, 0x08); }
  void set_independent_aux_heat(bool state) {
    // The status report has this flag in byte 8, the request in byte 22; copyStatus() copies byte 8 over
    this->m_setMask(8, false, 0x40);
    this->m_setMask(22, state, 0x08);
  }
  void set_target_humidity(uint8_t humidity) { this->m_setValue(19, humidity & 0x7F, 0x7F); }
};

/* ############### */
/* ### POLLING ### */
/* ############### */

// Replaces MideaUART's m_onIdle(), which only queues the status query; its power usage query runs on its own timer
void Appliance::m_onIdle() {
  // Group and properties queries take turns with the status query, so the climate state keeps its refresh rate
  if (!this->status_turn_ && this->queue_next_data_query_()) {
    this->status_turn_ = true;
    return;
  }
  this->status_turn_ = false;
  this->queue_status_query_();
}

bool Appliance::queue_next_data_query_() {
  constexpr uint8_t group_count = static_cast<uint8_t>(MideaGroup::MIDEA_GROUP_COUNT);
  // One slot per group, then one for all the properties
  for (uint8_t tries = 0; tries <= group_count; ++tries) {
    const uint8_t idx = this->next_query_;
    this->next_query_ = (idx + 1) % (group_count + 1);
    if (idx < group_count) {
      if (this->polled_groups_ & (1U << idx)) {
        this->queue_group_query_(static_cast<MideaGroup>(idx));
        return true;
      }
    } else if (this->polled_properties_) {
      this->queue_properties_query_();
      return true;
    }
  }
  return false;
}

void Appliance::queue_status_query_() {
  dudanov::midea::ac::QueryStateData data{};
  ESP_LOGV(Constants::TAG, "Enqueuing a GET_STATUS(0x41) request");
  this->m_queueRequest(FrameType::DEVICE_QUERY, std::move(data),
                       [this](FrameData data) -> ResponseStatus { return this->read_status_(std::move(data)); });
}

// Status reports answer both the status query and the 0x40 set status request
ResponseStatus Appliance::read_status_(FrameData data) {
  if (!data.hasStatus())
    return ResponseStatus::RESPONSE_WRONG;
  const bool changed = this->read_status_fields_(data);
  const ResponseStatus status = this->m_readStatus(std::move(data));
  if (this->state_.status_valid) {
    // m_readStatus() refreshes only bytes 1-10 of the request template; set the ones past it that the
    // controls change from the report too, so a value the appliance refused is not sent again
    StatusRequest request(this->m_status);
    if (this->state_.target_humidity != 0)  // 0: the report is too short to carry it
      request.set_target_humidity(this->state_.target_humidity);
    request.set_independent_aux_heat(this->state_.aux_heat_mode == MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_AUX_ONLY);
    this->m_status = std::move(request);
  }
  if (changed)
    this->sendUpdate();
  return status;
}

void Appliance::queue_group_query_(MideaGroup group) {
  const uint8_t id = GROUP_IDS[static_cast<uint8_t>(group)];
  ESP_LOGV(Constants::TAG, "Enqueuing a GET_GROUP_DATA(0x41) request for group %u", id);
  GroupQueryData data(id);
  this->m_queueRequest(
      FrameType::DEVICE_QUERY, std::move(data),
      [this, group](const FrameData &data) -> ResponseStatus { return this->read_group_(group, data); });
}

void Appliance::queue_properties_query_() {
  PropertiesData data(0xB1);
  for (uint8_t idx = 0; idx < static_cast<uint8_t>(MideaProperty::MIDEA_PROPERTY_COUNT); ++idx) {
    if (this->polled_properties_ & (1U << idx))
      data.add_id(PROPERTY_IDS[idx]);
  }
  data.finish();
  ESP_LOGV(Constants::TAG, "Enqueuing a GET_PROPERTIES(0xB1) request");
  this->m_queueRequest(FrameType::DEVICE_QUERY, std::move(data),
                       [this](const FrameData &data) -> ResponseStatus { return this->read_properties_(data); });
}

/* ############### */
/* ### PARSING ### */
/* ############### */

template<typename T> static bool same(T a, T b) { return a == b; }
// NAN marks a value the appliance does not report; it never equals itself, so compare it explicitly
static bool same(float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); }

template<typename T> static void update(T &property, T value, bool &changed) {
  if (!same(property, value)) {
    property = value;
    changed = true;
  }
}

bool Appliance::read_status_fields_(const FrameData &data) {
  const uint8_t *d = data.data();
  const uint8_t size = data.size();
  if (size < 18)
    return false;
  bool changed = false;
  update(this->state_.status_valid, true, changed);
  update(this->state_.fan_speed, static_cast<uint8_t>(d[3] & 0x7F), changed);
  update(this->state_.follow_me, static_cast<bool>(d[8] & 0x80), changed);
  update(this->state_.purifier, static_cast<bool>(d[9] & 0x20), changed);
  update(this->state_.fahrenheit, static_cast<bool>(d[10] & 0x04), changed);
  update(this->state_.filter_alert, static_cast<bool>(d[13] & 0x20), changed);
  update(this->state_.display_on, d[14] != 0x70, changed);
  update(this->state_.error_code, d[16], changed);
  MideaAuxHeatMode aux = MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_OFF;
  if (d[8] & 0x40) {
    aux = MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_AUX_ONLY;
  } else if (d[9] & 0x08) {
    aux = MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_AUX_HEAT;
  }
  update(this->state_.aux_heat_mode, aux, changed);
  if (size >= 22)
    update(this->state_.target_humidity, static_cast<uint8_t>(d[19] & 0x7F), changed);
  return changed;
}

static uint8_t bcd(uint8_t value) { return 10 * (value >> 4) + (value & 0x0F); }
// Louver angles are 0-180 degrees; a larger value (0xF0 seen on a unit without an angle setting) is not an angle
static float angle(uint8_t value) { return value <= 180 ? static_cast<float>(value) : NAN; }
// Shortest reply that holds every field read from it, per group
static constexpr uint8_t GROUP_MIN_SIZES[] = {17, 11, 21, 13, 14, 15};
static_assert(sizeof(GROUP_MIN_SIZES) == static_cast<uint8_t>(MideaGroup::MIDEA_GROUP_COUNT));

ResponseStatus Appliance::read_group_(MideaGroup group, const FrameData &data) {
  const uint8_t *d = data.data();
  const uint8_t size = data.size();
  const uint8_t idx = static_cast<uint8_t>(group);
  if (!data.hasID(0xC1) || size < 4 || (d[3] & 0x0F) != (GROUP_IDS[idx] & 0x0F))
    return ResponseStatus::RESPONSE_WRONG;
  if (size < GROUP_MIN_SIZES[idx]) {
    // A reply in a layout this code does not know; its entities stay unknown, so say why once
    if (!(this->short_groups_ & (1U << idx))) {
      this->short_groups_ |= 1U << idx;
      ESP_LOGW(Constants::TAG, "Group %u reply has %u bytes, expected %u; its values stay unknown", GROUP_IDS[idx],
               size, GROUP_MIN_SIZES[idx]);
    }
    return ResponseStatus::RESPONSE_OK;
  }
  bool changed = false;
  auto &e = this->state_;
  switch (group) {
    case MideaGroup::MIDEA_GROUP_1:
      update(e.compressor_frequency, static_cast<float>(d[4]), changed);
      update(e.compressor_target_frequency, static_cast<float>(d[5]), changed);
      update(e.compressor_current, static_cast<float>(d[7]), changed);
      update(e.compressor_voltage, static_cast<float>(d[8]), changed);
      update(e.indoor_coil_temperature, (static_cast<float>(d[11]) - 30.0f) * 0.5f, changed);
      update(e.outdoor_coil_temperature, (static_cast<float>(d[12]) - 50.0f) * 0.5f, changed);
      update(e.discharge_temperature, static_cast<float>(d[14]), changed);
      break;
    case MideaGroup::MIDEA_GROUP_2:
      update(e.indoor_fan_target_speed, static_cast<float>(d[4] * 8), changed);
      update(e.indoor_fan_speed, static_cast<float>(d[5] * 8), changed);
      update(e.water_pump, static_cast<int8_t>((d[8] & 0x10) != 0), changed);
      break;
    case MideaGroup::MIDEA_GROUP_4: {
      float total, current, power;
      if (this->energy_format_ == MideaEnergyFormat::MIDEA_ENERGY_FORMAT_BINARY) {
        auto u32 = [d](uint8_t i) {
          return (static_cast<uint32_t>(d[i]) << 24) | (static_cast<uint32_t>(d[i + 1]) << 16) |
                 (static_cast<uint32_t>(d[i + 2]) << 8) | d[i + 3];
        };
        total = static_cast<float>(u32(4)) * 0.1f;
        current = static_cast<float>(u32(12)) * 0.1f;
        power = static_cast<float>((static_cast<uint32_t>(d[16]) << 16) | (static_cast<uint32_t>(d[17]) << 8) | d[18]) *
                0.1f;
      } else {
        auto energy = [d](uint8_t i) {
          return 10000.0f * bcd(d[i]) + 100.0f * bcd(d[i + 1]) + bcd(d[i + 2]) + 0.01f * bcd(d[i + 3]);
        };
        total = energy(4);
        current = energy(12);
        power = 1000.0f * bcd(d[16]) + 10.0f * bcd(d[17]) + 0.1f * bcd(d[18]);
      }
      // All zero means the unit has no energy metering
      if (total == 0.0f && current == 0.0f && power == 0.0f)
        break;
      update(e.total_energy, total, changed);
      update(e.current_energy, current, changed);
      update(e.real_time_power, power, changed);
      break;
    }
    case MideaGroup::MIDEA_GROUP_5:
      // 0 means the unit has no humidity sensor
      update(e.indoor_humidity, d[4] != 0 ? static_cast<float>(d[4]) : NAN, changed);
      update(e.outdoor_fan_speed, static_cast<float>(d[8] * 8), changed);
      update(e.defrost, static_cast<int8_t>(d[10] != 0), changed);
      break;
    case MideaGroup::MIDEA_GROUP_7:
      update(e.outdoor_unit_power, static_cast<float>(d[10] | (d[11] << 8)), changed);
      break;
    case MideaGroup::MIDEA_GROUP_11:
      update(e.horizontal_louver_angle, angle(d[9]), changed);
      update(e.vertical_louver_angle, angle(d[12]), changed);
      break;
    default:
      break;
  }
  if (changed)
    this->sendUpdate();
  return ResponseStatus::RESPONSE_OK;
}

static int8_t property_index(uint16_t id) {
  for (size_t idx = 0; idx < sizeof(PROPERTY_IDS) / sizeof(PROPERTY_IDS[0]); ++idx) {
    if (PROPERTY_IDS[idx] == id)
      return static_cast<int8_t>(idx);
  }
  return -1;
}

ResponseStatus Appliance::read_properties_(const FrameData &data) {
  if (!data.hasID(0xB0) && !data.hasID(0xB1))
    return ResponseStatus::RESPONSE_WRONG;
  if (data.size() < 4) {
    ESP_LOGW(Constants::TAG, "Properties reply too short (%u bytes)", data.size());
    return ResponseStatus::RESPONSE_WRONG;
  }
  const uint8_t *d = data.data();
  // Leave out the message counter and the CRC; some units send a wrong CRC here, so it is not checked
  const uint8_t *const end = d + data.size() - 2;
  const bool is_ack = data.hasID(0xB0);
  uint8_t count = d[1];
  const uint8_t *it = d + 2;
  bool changed = false;
  // Each entry: ID (little endian), result, value size, value
  for (; count && end - it >= 4; --count) {
    const uint16_t id = it[0] | (it[1] << 8);
    const uint8_t result = it[2];
    const uint8_t len = it[3];
    const uint8_t *value = it + 4;
    it += 4 + len;
    if (it > end)
      break;
    const int8_t idx = property_index(id);
    if ((result & 0x10) && (is_ack || idx < 0 || !(this->failed_properties_ & (1U << idx)))) {
      if (idx >= 0)
        this->failed_properties_ |= 1U << idx;
      ESP_LOGW(Constants::TAG, "Property 0x%04X failed, result 0x%02X", id, result);
    }
    if (idx < 0 || len == 0)
      continue;
    uint8_t decoded;
    switch (static_cast<MideaProperty>(idx)) {
      case MideaProperty::MIDEA_PROPERTY_BREEZE_AWAY:
        decoded = value[0] == 2;
        break;
      case MideaProperty::MIDEA_PROPERTY_CASCADE:
      case MideaProperty::MIDEA_PROPERTY_FRESH_AIR:
        // value[0] is the on flag, value[1] the setting
        decoded = (len >= 2 && value[0]) ? value[1] : 0;
        break;
      case MideaProperty::MIDEA_PROPERTY_IECO:
        // value[0] is the iECO number, value[1] the switch
        decoded = len >= 2 && value[1] != 0;
        break;
      case MideaProperty::MIDEA_PROPERTY_OUT_SILENT:
        decoded = value[0] == 3;
        break;
      case MideaProperty::MIDEA_PROPERTY_BREEZELESS:
      case MideaProperty::MIDEA_PROPERTY_SELF_CLEAN:
      case MideaProperty::MIDEA_PROPERTY_JET_COOL:
      case MideaProperty::MIDEA_PROPERTY_SOUND:
        decoded = value[0] != 0;
        break;
      default:
        decoded = value[0];
        break;
    }
    const uint16_t bit = 1U << idx;
    if (!(this->known_properties_ & bit) || this->property_values_[idx] != decoded) {
      this->known_properties_ |= bit;
      this->property_values_[idx] = decoded;
      changed = true;
    }
  }
  if (changed)
    this->sendUpdate();
  return ResponseStatus::RESPONSE_OK;
}

/* ################ */
/* ### CONTROLS ### */
/* ################ */

template<typename F> void Appliance::modify_status_(F &&modify) {
  if (!this->state_.status_valid) {
    // m_status is only a template until the first status report; sending it would change other settings
    ESP_LOGW(Constants::TAG, "No status received from the appliance yet, control ignored");
    return;
  }
  StatusRequest status(this->m_status);
  modify(status);
  // Later climate controls start from m_status; read_status_() sets it back to what the appliance reports
  this->m_status = status;
  status.setBeeper(this->m_beeper);
  status.appendCRC();
  ESP_LOGD(Constants::TAG, "Enqueuing a priority SET_STATUS(0x40) request");
  // The entities change only when the status report in the reply shows that the appliance applied the request
  this->m_queueRequestPriority(FrameType::DEVICE_CONTROL, std::move(status), [this](FrameData data) -> ResponseStatus {
    return this->read_status_(std::move(data));
  });
}

void Appliance::set_fan_speed(uint8_t speed) {
  if (speed < 1 || speed > 100)
    return;
  this->modify_status_([speed](StatusRequest &s) { s.set_fan_speed(speed); });
}

void Appliance::set_purifier(bool state) {
  this->modify_status_([state](StatusRequest &s) { s.set_purifier(state); });
}

void Appliance::set_fahrenheit(bool state) {
  this->modify_status_([state](StatusRequest &s) { s.setFahrenheits(state); });
}

void Appliance::set_target_humidity(uint8_t humidity) {
  this->modify_status_([humidity](StatusRequest &s) { s.set_target_humidity(humidity); });
}

void Appliance::set_aux_heat_mode(MideaAuxHeatMode mode) {
  this->modify_status_([mode](StatusRequest &s) {
    s.set_aux_heat(mode == MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_AUX_HEAT);
    s.set_independent_aux_heat(mode == MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_AUX_ONLY);
  });
}

void Appliance::set_property(MideaProperty prop, uint8_t value) {
  const uint8_t idx = static_cast<uint8_t>(prop);
  if (prop >= MideaProperty::MIDEA_PROPERTY_COUNT)
    return;
  uint8_t raw[13]{};
  uint8_t len = 1;
  switch (prop) {
    case MideaProperty::MIDEA_PROPERTY_BREEZE_AWAY:
      raw[0] = value ? 2 : 1;
      break;
    case MideaProperty::MIDEA_PROPERTY_CASCADE:
      raw[0] = value ? 1 : 0;
      raw[1] = value;
      len = 2;
      break;
    case MideaProperty::MIDEA_PROPERTY_FRESH_AIR:
      raw[0] = value ? 1 : 0;
      raw[1] = value;
      raw[2] = 0xFF;
      len = 3;
      break;
    case MideaProperty::MIDEA_PROPERTY_IECO:
      // iECO frame, number, switch, then 10 reserved bytes
      raw[1] = this->ieco_number_;
      raw[2] = value ? 1 : 0;
      len = sizeof(raw);
      break;
    case MideaProperty::MIDEA_PROPERTY_OUT_SILENT:
      raw[0] = value ? 3 : 0;
      break;
    default:
      raw[0] = value;
      break;
  }
  PropertiesData data(0xB0);
  data.add_value(PROPERTY_IDS[idx], raw, len);
  const uint8_t buzzer = this->m_beeper ? 1 : 0;
  data.add_value(PROPERTY_ID_BUZZER, &buzzer, 1);
  data.finish();
  ESP_LOGD(Constants::TAG, "Enqueuing a priority SET_PROPERTIES(0xB0) request for 0x%04X = %u", PROPERTY_IDS[idx],
           value);
  // The acknowledgement carries each property's value after the request, so a refused one keeps its old value
  this->m_queueRequestPriority(
      FrameType::DEVICE_CONTROL, std::move(data),
      [this](const FrameData &data) -> ResponseStatus { return this->read_properties_(data); });
}

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
