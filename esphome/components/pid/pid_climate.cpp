#include "pid_climate.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::pid {

ESPHOME_LOG_TAG(TAG, "pid.climate");

static constexpr uint32_t AUTOTUNE_PROGRESS_INTERVAL_ID = 0;

bool PIDClimate::set_deadband_thresholds(float threshold_low, float threshold_high) {
  if (threshold_low > threshold_high) {
    ESP_LOGW(TAG, "Deadband threshold low %.2f must not be greater than high %.2f", threshold_low, threshold_high);
    return false;
  }

  this->set_threshold_low(threshold_low);
  this->set_threshold_high(threshold_high);
  return true;
}

void PIDClimate::setup() {
  this->sensor_->add_on_state_callback([this](float state) {
    // only publish if state/current temperature has changed in two digits of precision
    this->do_publish_ = roundf(state * 100) != roundf(this->current_temperature * 100);
    this->current_temperature = state;
    this->update_pid_();
  });
  this->current_temperature = this->sensor_->state;

  // register for humidity values and get initial state
  if (this->humidity_sensor_ != nullptr) {
    this->humidity_sensor_->add_on_state_callback([this](float state) {
      this->current_humidity = state;
      this->publish_state();
    });
    this->current_humidity = this->humidity_sensor_->state;
  }

  // restore set points
  optional<climate::ClimateDeviceRestoreState> restore;
  if (this->on_boot_restore_from_ == OnBootRestoreFrom::ON_BOOT_RESTORE_FROM_MEMORY)
    restore = this->restore_state_();
  if (restore.has_value()) {
    restore->to_call(this).perform();
  } else {
    // restore from defaults, change_away handles those for us
    if (supports_heat_() && supports_cool_()) {
      this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    } else if (supports_cool_()) {
      this->mode = climate::CLIMATE_MODE_COOL;
    } else if (supports_heat_()) {
      this->mode = climate::CLIMATE_MODE_HEAT;
    }
    this->target_temperature = this->default_target_temperature_;
  }
  this->setup_complete_ = true;

  if (restore.has_value())
    return;
  // Selected directly rather than through a call, so on_control automations do not run at boot.
  bool selected = false;
  if (this->default_custom_preset_ != nullptr) {
    selected = this->select_custom_preset_(StringRef(this->default_custom_preset_));
  } else if (this->default_preset_ != climate::CLIMATE_PRESET_NONE) {
    selected = this->select_preset_(this->default_preset_);
  }
  if (selected)
    this->preset_change_callback_.call();
}
void PIDClimate::control(const climate::ClimateCall &call) {
  auto previous_preset = this->preset;
  auto previous_custom_preset = this->get_custom_preset();
  float previous_target = this->target_temperature;

  // A target in the same call wins over the preset's, so a restore keeps the saved target.
  bool preset_selected = false;
  if (call.has_custom_preset()) {
    preset_selected = this->select_custom_preset_(call.get_custom_preset());
  } else if (call.get_preset().has_value()) {
    preset_selected = this->select_preset_(*call.get_preset());
  }
  auto call_mode = call.get_mode();
  if (call_mode.has_value())
    this->mode = *call_mode;
  auto call_target = call.get_target_temperature();
  if (call_target.has_value())
    this->target_temperature = *call_target;

  // If switching to off mode, set output immediately
  if (this->mode == climate::CLIMATE_MODE_OFF)
    this->write_output_(0.0f);

  this->publish_state();

  // After publish_state(), so automations see the state the whole call produced.
  bool changed = this->preset != previous_preset || this->get_custom_preset() != previous_custom_preset ||
                 this->target_temperature != previous_target;
  if (preset_selected && changed && this->setup_complete_)
    this->preset_change_callback_.call();
}
climate::ClimateTraits PIDClimate::traits() {
  auto traits = climate::ClimateTraits();
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);

  if (this->humidity_sensor_ != nullptr)
    traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_HUMIDITY);

  traits.set_supported_modes({climate::CLIMATE_MODE_OFF});
  if (supports_cool_())
    traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  if (supports_heat_())
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  if (supports_heat_() && supports_cool_())
    traits.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);

  // Custom presets are stored on the Climate base class and added by get_traits()
  for (uint8_t i = 0; i < this->preset_count_; i++)
    traits.add_supported_preset(this->preset_entry_(i).preset);

  return traits;
}
PIDPresetEntry PIDClimate::preset_entry_(uint8_t index) const {
  PIDPresetEntry entry;
  progmem_memcpy(&entry, &this->preset_config_[index], sizeof(entry));
  return entry;
}
PIDCustomPresetEntry PIDClimate::custom_preset_entry_(uint8_t index) const {
  PIDCustomPresetEntry entry;
  progmem_memcpy(&entry, &this->custom_preset_config_[index], sizeof(entry));
  return entry;
}
bool PIDClimate::select_preset_(climate::ClimatePreset preset) {
  for (uint8_t i = 0; i < this->preset_count_; i++) {
    auto entry = this->preset_entry_(i);
    if (entry.preset == preset) {
      this->set_preset_(entry.preset);
      this->target_temperature = entry.target_temperature;
      return true;
    }
  }
  return false;
}
bool PIDClimate::select_custom_preset_(StringRef name) {
  for (uint8_t i = 0; i < this->custom_preset_count_; i++) {
    auto entry = this->custom_preset_entry_(i);
    if (name == entry.name) {
      this->set_custom_preset_(entry.name);
      this->target_temperature = entry.target_temperature;
      return true;
    }
  }
  return false;
}
void PIDClimate::dump_config() {
  LOG_CLIMATE("", "PID Climate", this);
  ESP_LOGCONFIG(TAG,
                "  Control Parameters:\n"
                "    kp: %.5f, ki: %.5f, kd: %.5f, output samples: %d",
                controller_.kp_, controller_.ki_, controller_.kd_, controller_.output_samples_);

  if (controller_.threshold_low_ == 0 && controller_.threshold_high_ == 0) {
    ESP_LOGCONFIG(TAG, "  Deadband disabled.");
  } else {
    ESP_LOGCONFIG(TAG,
                  "  Deadband Parameters:\n"
                  "    threshold: %0.5f to %0.5f, multipliers(kp: %.5f, ki: %.5f, kd: %.5f), "
                  "output samples: %d",
                  controller_.threshold_low_, controller_.threshold_high_, controller_.kp_multiplier_,
                  controller_.ki_multiplier_, controller_.kd_multiplier_, controller_.deadband_output_samples_);
  }

  for (uint8_t i = 0; i < this->preset_count_; i++) {
    auto entry = this->preset_entry_(i);
    ESP_LOGCONFIG(TAG, "  Preset %s: %.1f°C", LOG_STR_ARG(climate::climate_preset_to_string(entry.preset)),
                  entry.target_temperature);
  }
  for (uint8_t i = 0; i < this->custom_preset_count_; i++) {
    auto entry = this->custom_preset_entry_(i);
    ESP_LOGCONFIG(TAG, "  Preset %s: %.1f°C", entry.name, entry.target_temperature);
  }

  if (this->autotuner_ != nullptr) {
    this->autotuner_->dump_config();
  }
}
void PIDClimate::write_output_(float value) {
  this->output_value_ = value;

  // first ensure outputs are off (both outputs not active at the same time)
  if (this->supports_cool_() && value >= 0)
    this->cool_output_->set_level(0.0f);
  if (this->supports_heat_() && value <= 0)
    this->heat_output_->set_level(0.0f);

  // value < 0 means cool, > 0 means heat
  if (this->supports_cool_() && value < 0)
    this->cool_output_->set_level(std::min(1.0f, -value));
  if (this->supports_heat_() && value > 0)
    this->heat_output_->set_level(std::min(1.0f, value));

  // Update action variable for user feedback what's happening
  climate::ClimateAction new_action;
  if (this->supports_cool_() && value < 0) {
    new_action = climate::CLIMATE_ACTION_COOLING;
  } else if (this->supports_heat_() && value > 0) {
    new_action = climate::CLIMATE_ACTION_HEATING;
  } else if (this->mode == climate::CLIMATE_MODE_OFF) {
    new_action = climate::CLIMATE_ACTION_OFF;
  } else {
    new_action = climate::CLIMATE_ACTION_IDLE;
  }

  if (new_action != this->action) {
    this->action = new_action;
    this->do_publish_ = true;
  }
  this->pid_computed_callback_.call();
}
void PIDClimate::update_pid_() {
  float value;
  if (std::isnan(this->current_temperature) || std::isnan(this->target_temperature)) {
    // if any control parameters are nan, turn off all outputs
    value = 0.0;
  } else {
    // Update PID controller irrespective of current mode, to not mess up D/I terms
    // In non-auto mode, we just discard the output value
    value = this->controller_.update(this->target_temperature, this->current_temperature);

    // Check autotuner
    if (this->autotuner_ != nullptr && !this->autotuner_->is_finished()) {
      auto res = this->autotuner_->update(this->target_temperature, this->current_temperature);
      if (res.result_params.has_value()) {
        this->controller_.kp_ = res.result_params->kp;
        this->controller_.ki_ = res.result_params->ki;
        this->controller_.kd_ = res.result_params->kd;
        // keep autotuner instance so that subsequent dump_configs will print the long result message.
      } else {
        value = res.output;
      }
    }
  }

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    this->write_output_(0.0);
  } else {
    this->write_output_(value);
  }

  if (this->do_publish_)
    this->publish_state();
}
void PIDClimate::start_autotune(float noiseband, float positive_output, float negative_output) {
  auto tuner = make_unique<PIDAutotuner>();
  tuner->set_noiseband(noiseband);
  tuner->set_output_positive(positive_output);
  tuner->set_output_negative(negative_output);
  this->start_autotune(std::move(tuner));
}

void PIDClimate::start_autotune(std::unique_ptr<PIDAutotuner> &&autotune) {
  this->autotuner_ = std::move(autotune);
  float min_value = this->supports_cool_() ? -1.0f : 0.0f;
  float max_value = this->supports_heat_() ? 1.0f : 0.0f;
  this->autotuner_->config(min_value, max_value);
  this->autotuner_->set_autotuner_id(this->get_log_name());

  ESP_LOGI(TAG,
           "%s: Autotune has started. This can take a long time depending on the "
           "responsiveness of your system. Your system "
           "output will be altered to deliberately oscillate above and below the setpoint multiple times. "
           "Until your sensor provides a reading, the autotuner may display \'nan\'",
           LOG_STR_ARG(this->get_log_name()));

  this->set_interval(AUTOTUNE_PROGRESS_INTERVAL_ID, 10000, [this]() {
    if (this->autotuner_ != nullptr && !this->autotuner_->is_finished())
      this->autotuner_->dump_config();
  });

  if (mode != climate::CLIMATE_MODE_HEAT_COOL) {
    ESP_LOGW(TAG, "%s: !!! For PID autotuner you need to set AUTO (also called heat/cool) mode!",
             LOG_STR_ARG(this->get_log_name()));
  }
}

void PIDClimate::reset_integral_term() { this->controller_.reset_accumulated_integral(); }

}  // namespace esphome::pid
