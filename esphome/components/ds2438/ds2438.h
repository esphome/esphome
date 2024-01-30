#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ds248x/ds248x.h"

using namespace esphome::ds248x;

namespace esphome {
namespace ds2438 {

static const uint8_t DALLAS_COMMAND_START_TCONVERSION = 0xB4;
static const uint8_t DALLAS_COMMAND_RECALL_MEMORY = 0xB8;

class DS2438BatterySensor : public DS248xSensor {
public:
  bool setup_sensor();
  void set_temperature_sensor(Sensor* sensor) { temperature_sensor_ = sensor; }
  void set_voltage_sensor(Sensor* sensor) { voltage_sensor_ = sensor; }
  void set_current_sensor(Sensor* sensor) { current_sensor_ = sensor; }

  bool update();
  void add_conversion_commands(std::set<uint8_t> &commands);

protected:
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};

private:
  void updateTemp();
  void updateVoltage();
  void updateCurrent();

};

}
}