#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../m5ioe1.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome::m5ioe1 {

class M5IOE1ADCSensor : public sensor::Sensor, public PollingComponent, public Parented<M5IOE1Component> {
 public:
  void update() override;
  void dump_config() override;

  void set_channel(M5IOE1ADCChannel channel) { this->channel_ = channel; }

 protected:
  M5IOE1ADCChannel channel_{ADC_NONE};
};

class M5IOE1TemperatureSensor : public sensor::Sensor, public PollingComponent, public Parented<M5IOE1Component> {
 public:
  void update() override;
};

}  // namespace esphome::m5ioe1
