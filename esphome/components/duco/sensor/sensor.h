#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "../duco.h"

namespace esphome {
namespace duco {

class DucoCo2Sensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(const DucoMessage &message) override;

  void set_address(uint8_t address);

 protected:
  uint8_t address_;
};

class DucoFilterRemainingSensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(const DucoMessage &message) override;
};

class DucoFlowLevelSensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(const DucoMessage &message) override;
};

class DucoStateTimeRemainingSensor : public DucoDevice, public PollingComponent, public sensor::Sensor {
 public:
  void setup() override;
  void update() override;

  float get_setup_priority() const override;

  void receive_response(const DucoMessage &message) override;
};

}  // namespace duco
}  // namespace esphome
