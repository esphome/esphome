#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/template_lambda.h"
#include "esphome/components/water_heater/water_heater.h"

namespace esphome {
namespace template_ {

enum TemplateWaterHeaterRestoreMode {
  WATER_HEATER_NO_RESTORE,
  WATER_HEATER_RESTORE,
  WATER_HEATER_RESTORE_AND_CALL,
};

class TemplateWaterHeater : public water_heater::WaterHeater, public Component {
 public:
  TemplateWaterHeater();

  template<typename F> void set_current_temperature_lambda(F &&f) {
    this->current_temperature_f_.set(std::forward<F>(f));
  }
  template<typename F> void set_mode_lambda(F &&f) { this->mode_f_.set(std::forward<F>(f)); }

  void set_optimistic(bool optimistic) { this->optimistic_ = optimistic; }
  void set_restore_mode(TemplateWaterHeaterRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }

  Trigger<> *get_set_trigger() const { return this->set_trigger_; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

 protected:
  void control(const water_heater::WaterHeaterCall &call) override;
  water_heater::WaterHeaterTraits traits() override;

  bool optimistic_{true};
  TemplateWaterHeaterRestoreMode restore_mode_{WATER_HEATER_NO_RESTORE};

  Trigger<> *set_trigger_ = new Trigger<>();

  TemplateLambda<float> current_temperature_f_;
  TemplateLambda<water_heater::WaterHeaterMode> mode_f_;
};

}  // namespace template_
}  // namespace esphome
