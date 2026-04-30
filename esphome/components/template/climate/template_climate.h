#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/climate/climate.h"

namespace esphome {
namespace template_ {

class TemplateClimate final : public climate::Climate, public Component {
 public:
  TemplateClimate();

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override { return this->traits_; }

  template<typename F> void set_current_temperature_lambda(F &&f) {
    this->current_temperature_f_.set(std::forward<F>(f));
  }
  template<typename F> void set_current_humidity_lambda(F &&f) { this->current_humidity_f_.set(std::forward<F>(f)); }
  template<typename F> void set_target_temperature_lambda(F &&f) {
    this->target_temperature_f_.set(std::forward<F>(f));
  }
  template<typename F> void set_target_temperature_low_lambda(F &&f) {
    this->target_temperature_low_f_.set(std::forward<F>(f));
  }
  template<typename F> void set_target_temperature_high_lambda(F &&f) {
    this->target_temperature_high_f_.set(std::forward<F>(f));
  }
  template<typename F> void set_target_humidity_lambda(F &&f) { this->target_humidity_f_.set(std::forward<F>(f)); }
  template<typename F> void set_mode_lambda(F &&f) { this->mode_f_.set(std::forward<F>(f)); }
  template<typename F> void set_action_lambda(F &&f) { this->action_f_.set(std::forward<F>(f)); }
  template<typename F> void set_fan_mode_lambda(F &&f) { this->fan_mode_f_.set(std::forward<F>(f)); }
  template<typename F> void set_custom_fan_mode_lambda(F &&f) { this->custom_fan_mode_f_.set(std::forward<F>(f)); }
  template<typename F> void set_swing_mode_lambda(F &&f) { this->swing_mode_f_.set(std::forward<F>(f)); }
  template<typename F> void set_preset_lambda(F &&f) { this->preset_f_.set(std::forward<F>(f)); }
  template<typename F> void set_custom_preset_lambda(F &&f) { this->custom_preset_f_.set(std::forward<F>(f)); }

  void set_supported_modes(const std::vector<climate::ClimateMode> &modes) {
    for (auto mode : modes)
      this->traits_.add_supported_mode(mode);
  }
  void set_supported_fan_modes(const std::vector<climate::ClimateFanMode> &modes) {
    for (auto mode : modes)
      this->traits_.add_supported_fan_mode(mode);
  }
  void set_supported_custom_fan_modes(const std::vector<std::string> &modes) {
    this->custom_fan_mode_strings_ = modes;
    std::vector<const char *> mode_ptrs;
    mode_ptrs.reserve(modes.size());
    for (const auto &mode : this->custom_fan_mode_strings_) {
      mode_ptrs.push_back(mode.c_str());
    }
    this->Climate::set_supported_custom_fan_modes(mode_ptrs);
  }
  void set_supported_swing_modes(const std::vector<climate::ClimateSwingMode> &modes) {
    for (auto mode : modes)
      this->traits_.add_supported_swing_mode(mode);
  }
  void set_supported_presets(const std::vector<climate::ClimatePreset> &presets) {
    for (auto preset : presets)
      this->traits_.add_supported_preset(preset);
  }
  void set_supported_custom_presets(const std::vector<std::string> &presets) {
    this->custom_preset_strings_ = presets;
    std::vector<const char *> preset_ptrs;
    preset_ptrs.reserve(presets.size());
    for (const auto &preset : this->custom_preset_strings_) {
      preset_ptrs.push_back(preset.c_str());
    }
    this->Climate::set_supported_custom_presets(preset_ptrs);
  }

  Trigger<climate::ClimateMode> *get_set_mode_trigger() const { return this->set_mode_trigger_; }
  Trigger<float> *get_set_target_temperature_trigger() const { return this->set_target_temperature_trigger_; }
  Trigger<float> *get_set_target_temperature_low_trigger() const { return this->set_target_temperature_low_trigger_; }
  Trigger<float> *get_set_target_temperature_high_trigger() const { return this->set_target_temperature_high_trigger_; }
  Trigger<float> *get_set_target_humidity_trigger() const { return this->set_target_humidity_trigger_; }
  Trigger<climate::ClimateFanMode> *get_set_fan_mode_trigger() const { return this->set_fan_mode_trigger_; }
  Trigger<std::string> *get_set_custom_fan_mode_trigger() const { return this->set_custom_fan_mode_trigger_; }
  Trigger<climate::ClimateSwingMode> *get_set_swing_mode_trigger() const { return this->set_swing_mode_trigger_; }
  Trigger<climate::ClimatePreset> *get_set_preset_trigger() const { return this->set_preset_trigger_; }
  Trigger<std::string> *get_set_custom_preset_trigger() const { return this->set_custom_preset_trigger_; }

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }

 protected:
  void control(const climate::ClimateCall &call) override;

  climate::ClimateTraits traits_;
  bool optimistic_{true};

  TemplateLambda<float> current_temperature_f_;
  TemplateLambda<float> current_humidity_f_;
  TemplateLambda<float> target_temperature_f_;
  TemplateLambda<float> target_temperature_low_f_;
  TemplateLambda<float> target_temperature_high_f_;
  TemplateLambda<float> target_humidity_f_;
  TemplateLambda<climate::ClimateMode> mode_f_;
  TemplateLambda<climate::ClimateAction> action_f_;
  TemplateLambda<climate::ClimateFanMode> fan_mode_f_;
  TemplateLambda<std::string> custom_fan_mode_f_;
  TemplateLambda<climate::ClimateSwingMode> swing_mode_f_;
  TemplateLambda<climate::ClimatePreset> preset_f_;
  TemplateLambda<std::string> custom_preset_f_;

  Trigger<climate::ClimateMode> *set_mode_trigger_;
  Trigger<float> *set_target_temperature_trigger_;
  Trigger<float> *set_target_temperature_low_trigger_;
  Trigger<float> *set_target_temperature_high_trigger_;
  Trigger<float> *set_target_humidity_trigger_;
  Trigger<climate::ClimateFanMode> *set_fan_mode_trigger_;
  Trigger<std::string> *set_custom_fan_mode_trigger_;
  Trigger<climate::ClimateSwingMode> *set_swing_mode_trigger_;
  Trigger<climate::ClimatePreset> *set_preset_trigger_;
  Trigger<std::string> *set_custom_preset_trigger_;

  std::vector<std::string> custom_fan_mode_strings_;
  std::vector<std::string> custom_preset_strings_;
};

}  // namespace template_
}  // namespace esphome
