#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/number/number.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace template_ {

using namespace climate;
using namespace number;
using namespace select;
using namespace sensor;
using namespace text_sensor;

class TemplateClimate : public Climate, public Component {
 public:
  ClimateTraits traits_;

  void setup() override;

  ClimateTraits traits() override { return this->traits_; }

  void set_current_temperature(Sensor *sensor) {
    this->current_temperature_ = sensor;
    this->traits_.set_supports_current_temperature(true);
  }

  void set_target_temperature(Number *num) { this->target_temperature_ = num; }

  void set_mode(Select *s) { this->mode_ = s; }

  void set_fan_mode(Select *s) { this->fan_mode_ = s; }

  void set_swing_mode(Select *s) { this->swing_mode_ = s; }

  void set_preset(Select *s) { this->preset_ = s; }

  void set_action(TextSensor *s) {
    this->action_ = s;
    this->traits_.set_supports_action(true);
  }

 protected:
  void control(const ClimateCall &call) override;

  Sensor *current_temperature_{nullptr};
  Number *target_temperature_{nullptr};
  Select *mode_{nullptr};
  Select *fan_mode_{nullptr};
  Select *swing_mode_{nullptr};
  Select *preset_{nullptr};
  TextSensor *action_{nullptr};
};

}  // namespace template_
}  // namespace esphome
