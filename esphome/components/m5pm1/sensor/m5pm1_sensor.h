#pragma once

#include <cstdint>

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "../m5pm1.h"

namespace esphome::m5pm1 {

enum class M5PM1VoltageType : uint8_t {
  M5PM1_VOLTAGE_TYPE_BATTERY,
  M5PM1_VOLTAGE_TYPE_VIN,
  M5PM1_VOLTAGE_TYPE_5VOUT,
  M5PM1_VOLTAGE_TYPE_VREF,
};

class M5PM1ADCSensor : public sensor::Sensor, public PollingComponent, public Parented<M5PM1Component> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_pin(M5PM1GPIOPin *pin) { this->pin_ = pin; }
  void set_channel(M5PM1ADCChannel channel) { this->channel_ = channel; }

 protected:
  M5PM1GPIOPin *pin_{nullptr};
  M5PM1ADCChannel channel_{M5PM1ADCChannel::M5PM1_ADC_CHANNEL_1};
};

class M5PM1TemperatureSensor : public sensor::Sensor, public PollingComponent, public Parented<M5PM1Component> {
 public:
  void update() override;
};

class M5PM1VoltageSensor : public sensor::Sensor, public PollingComponent, public Parented<M5PM1Component> {
 public:
  explicit M5PM1VoltageSensor(M5PM1VoltageType type) : type_(type) {}

  void update() override;

 protected:
  M5PM1VoltageType type_;
};

}  // namespace esphome::m5pm1
