#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace water_heater {

enum WaterHeaterMode {
  WATER_HEATER_OFF = 0,
  WATER_HEATER_ECO,
  WATER_HEATER_ELECTRIC,
  WATER_HEATER_PERFORMANCE,
  WATER_HEATER_HIGH_DEMAND,
  WATER_HEATER_HEAT_PUMP,
  WATER_HEATER_GAS,
};

struct WaterHeaterCall {
  optional<WaterHeaterMode> mode;
  optional<float> target_temperature;
};

class WaterHeater : public EntityBase, public Component {
 public:
  float current_temperature{NAN};
  float target_temperature{NAN};
  WaterHeaterMode mode{WATER_HEATER_OFF};

  float min_temperature{40.0};
  float max_temperature{60.0};

  void publish_state();

  virtual void control(const WaterHeaterCall &call) = 0;

  void setup() override;
};

class TemplateWaterHeater : public WaterHeater {
 public:
  void control(const WaterHeaterCall &call) override;
  void dump_config() override;

  void set_mode_trigger(Trigger<WaterHeaterMode> *t) { mode_trigger_ = t; }
  void set_temperature_trigger(Trigger<float> *t) { temperature_trigger_ = t; }

 protected:
  Trigger<WaterHeaterMode> *mode_trigger_;
  Trigger<float> *temperature_trigger_;
};

}  // namespace water_heater
}  // namespace esphome