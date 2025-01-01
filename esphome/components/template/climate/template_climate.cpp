#include "template_climate.h"
#include <functional>

namespace esphome {
namespace template_ {

static const char *const TAG = "template_climate";

ClimateMode mode_from_string(const std::string &s) {
  if (str_equals_case_insensitive(s, "OFF")) {
    return CLIMATE_MODE_OFF;
  } else if (str_equals_case_insensitive(s, "HEAT_COOL")) {
    return CLIMATE_MODE_HEAT_COOL;
  } else if (str_equals_case_insensitive(s, "COOL")) {
    return CLIMATE_MODE_COOL;
  } else if (str_equals_case_insensitive(s, "HEAT")) {
    return CLIMATE_MODE_HEAT;
  } else if (str_equals_case_insensitive(s, "FAN_ONLY")) {
    return CLIMATE_MODE_FAN_ONLY;
  } else if (str_equals_case_insensitive(s, "DRY")) {
    return CLIMATE_MODE_DRY;
  } else if (str_equals_case_insensitive(s, "AUTO")) {
    return CLIMATE_MODE_AUTO;
  } else {
    ESP_LOGW(TAG, "Unrecognized mode %s", s.c_str());
    return CLIMATE_MODE_OFF;
  }
}

ClimateFanMode fan_mode_from_string(const std::string &s) {
  if (str_equals_case_insensitive(s, "ON")) {
    return CLIMATE_FAN_ON;
  } else if (str_equals_case_insensitive(s, "OFF")) {
    return CLIMATE_FAN_OFF;
  } else if (str_equals_case_insensitive(s, "AUTO")) {
    return CLIMATE_FAN_AUTO;
  } else if (str_equals_case_insensitive(s, "LOW")) {
    return CLIMATE_FAN_LOW;
  } else if (str_equals_case_insensitive(s, "MEDIUM")) {
    return CLIMATE_FAN_MEDIUM;
  } else if (str_equals_case_insensitive(s, "HIGH")) {
    return CLIMATE_FAN_HIGH;
  } else if (str_equals_case_insensitive(s, "MIDDLE")) {
    return CLIMATE_FAN_MIDDLE;
  } else if (str_equals_case_insensitive(s, "FOCUS")) {
    return CLIMATE_FAN_FOCUS;
  } else if (str_equals_case_insensitive(s, "DIFFUSE")) {
    return CLIMATE_FAN_DIFFUSE;
  } else if (str_equals_case_insensitive(s, "QUIET")) {
    return CLIMATE_FAN_QUIET;
  } else {
    ESP_LOGW(TAG, "Unrecognized fan mode %s", s.c_str());
    return CLIMATE_FAN_AUTO;
  }
}

ClimateSwingMode swing_mode_from_string(const std::string &s) {
  if (str_equals_case_insensitive(s, "OFF")) {
    return CLIMATE_SWING_OFF;
  } else if (str_equals_case_insensitive(s, "BOTH")) {
    return CLIMATE_SWING_BOTH;
  } else if (str_equals_case_insensitive(s, "VERTICAL")) {
    return CLIMATE_SWING_VERTICAL;
  } else if (str_equals_case_insensitive(s, "HORIZONTAL")) {
    return CLIMATE_SWING_HORIZONTAL;
  } else {
    ESP_LOGW(TAG, "Unrecognized swing mode %s", s.c_str());
    return CLIMATE_SWING_BOTH;
  }
}

ClimatePreset preset_from_string(const std::string &s) {
  if (str_equals_case_insensitive(s, "NONE")) {
    return CLIMATE_PRESET_NONE;
  } else if (str_equals_case_insensitive(s, "HOME")) {
    return CLIMATE_PRESET_HOME;
  } else if (str_equals_case_insensitive(s, "AWAY")) {
    return CLIMATE_PRESET_AWAY;
  } else if (str_equals_case_insensitive(s, "BOOST")) {
    return CLIMATE_PRESET_BOOST;
  } else if (str_equals_case_insensitive(s, "COMFORT")) {
    return CLIMATE_PRESET_COMFORT;
  } else if (str_equals_case_insensitive(s, "ECO")) {
    return CLIMATE_PRESET_ECO;
  } else if (str_equals_case_insensitive(s, "SLEEP")) {
    return CLIMATE_PRESET_SLEEP;
  } else if (str_equals_case_insensitive(s, "ACTIVITY")) {
    return CLIMATE_PRESET_ACTIVITY;
  } else {
    ESP_LOGW(TAG, "Unrecognized preset %s", s.c_str());
    return CLIMATE_PRESET_NONE;
  }
}

ClimateAction action_from_string(const std::string &s) {
  if (str_equals_case_insensitive(s, "OFF")) {
    return CLIMATE_ACTION_OFF;
  } else if (str_equals_case_insensitive(s, "COOLING")) {
    return CLIMATE_ACTION_COOLING;
  } else if (str_equals_case_insensitive(s, "HEATING")) {
    return CLIMATE_ACTION_HEATING;
  } else if (str_equals_case_insensitive(s, "IDLE")) {
    return CLIMATE_ACTION_IDLE;
  } else if (str_equals_case_insensitive(s, "DRYING")) {
    return CLIMATE_ACTION_DRYING;
  } else if (str_equals_case_insensitive(s, "FAN")) {
    return CLIMATE_ACTION_FAN;
  } else {
    ESP_LOGW(TAG, "Unrecognized action %s", s.c_str());
    return CLIMATE_ACTION_OFF;
  }
}

template<typename T> std::set<T> optset(Select *s, std::function<T(const std::string &)> func) {
  std::set<T> opts;
  for (const auto &opt : s->traits.get_options()) {
    opts.insert(func(opt));
  }
  return opts;
}

void TemplateClimate::setup() {
  this->current_temperature_->add_on_state_callback([this](float x) {
    this->current_temperature = x;
    this->publish_state();
  });
  this->current_temperature = this->current_temperature_->state;

  this->target_temperature_->add_on_state_callback([this](float x) {
    this->target_temperature = x;
    this->publish_state();
  });
  this->current_temperature = this->target_temperature_->state;

  this->traits_.set_supported_modes(optset<ClimateMode>(this->mode_, mode_from_string));
  this->mode_->add_on_state_callback([this](const std::string &x, size_t i) {
    this->mode = mode_from_string(x);
    this->publish_state();
  });
  this->mode = mode_from_string(this->mode_->state);

  if (this->fan_mode_ != nullptr) {
    this->traits_.set_supported_fan_modes(optset<ClimateFanMode>(this->fan_mode_, fan_mode_from_string));
    this->fan_mode_->add_on_state_callback([this](const std::string &x, size_t i) {
      this->fan_mode = fan_mode_from_string(x);
      this->publish_state();
    });
    this->fan_mode = fan_mode_from_string(this->fan_mode_->state);
  }

  if (this->swing_mode_ != nullptr) {
    this->traits_.set_supported_swing_modes(optset<ClimateSwingMode>(this->swing_mode_, swing_mode_from_string));
    this->swing_mode_->add_on_state_callback([this](const std::string &x, size_t i) {
      this->swing_mode = swing_mode_from_string(x);
      this->publish_state();
    });
    this->swing_mode = swing_mode_from_string(this->swing_mode_->state);
  }

  if (this->preset_ != nullptr) {
    this->traits_.set_supported_presets(optset<ClimatePreset>(this->preset_, preset_from_string));
    this->preset_->add_on_state_callback([this](const std::string &x, size_t i) {
      this->preset = preset_from_string(x);
      this->publish_state();
    });
    this->preset = preset_from_string(this->preset_->state);
  }

  if (this->action_ != nullptr) {
    this->action_->add_on_state_callback([this](const std::string &x) {
      this->action = action_from_string(x);
      this->publish_state();
    });
    this->action = action_from_string(this->action_->state);
  }
}

void TemplateClimate::control(const ClimateCall &call) {
  auto mode = call.get_mode();
  if (mode) {
    std::string opt = LOG_STR_ARG(climate_mode_to_string(mode.value()));
    this->mode_->make_call().set_option(opt).perform();
  }

  auto fan_mode = call.get_fan_mode();
  if (fan_mode && this->fan_mode_ != nullptr) {
    std::string opt = LOG_STR_ARG(climate_fan_mode_to_string(fan_mode.value()));
    this->fan_mode_->make_call().set_option(opt).perform();
  }

  auto swing_mode = call.get_swing_mode();
  if (swing_mode && this->swing_mode_ != nullptr) {
    std::string opt = LOG_STR_ARG(climate_swing_mode_to_string(swing_mode.value()));
    this->swing_mode_->make_call().set_option(opt).perform();
  }

  auto preset = call.get_preset();
  if (preset && this->preset_ != nullptr) {
    std::string opt = LOG_STR_ARG(climate_preset_to_string(preset.value()));
    this->preset_->make_call().set_option(opt).perform();
  }

  auto target_temperature = call.get_target_temperature();
  if (target_temperature) {
    this->target_temperature_->make_call().set_value(target_temperature.value()).perform();
  }
}

}  // namespace template_
}  // namespace esphome
