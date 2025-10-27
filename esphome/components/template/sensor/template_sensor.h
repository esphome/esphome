#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace template_ {

class TemplateSensor : public sensor::Sensor, public PollingComponent {
 public:
  void set_template(std::function<optional<float>()> &&f);

  void update() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  optional<std::function<optional<float>()>> f_;
};

}  // namespace template_
}  // namespace esphome
