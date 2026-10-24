#pragma once

#include "../mitsubishi_cn105_component.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome::mitsubishi_cn105 {

enum class MitsubishiCN105SensorKind : uint8_t {
  OUTSIDE_AIR_TEMPERATURE,
  INPUT_POWER,
  ENERGY,
};

class MitsubishiCN105Sensor final : public sensor::Sensor, public Component, public Parented<MitsubishiCN105Component> {
 public:
  void set_kind(MitsubishiCN105SensorKind kind) { this->kind_ = kind; }

  void setup() override;
  void dump_config() override;

 protected:
  void publish_reading_();
  float reading_() const;

  MitsubishiCN105SensorKind kind_{MitsubishiCN105SensorKind::OUTSIDE_AIR_TEMPERATURE};
};

}  // namespace esphome::mitsubishi_cn105
