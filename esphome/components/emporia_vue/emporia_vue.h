#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace emporia_vue {

struct __attribute__((__packed__)) PowerDataEntry {
  int32_t phase_one;
  int32_t phase_two;
  int32_t phase_three;
};

struct __attribute__((__packed__)) EmporiaSensorData {
  uint8_t version;
  uint8_t checksum;
  uint8_t unknown;  // TODO
  uint8_t counter;

  PowerDataEntry power[19];

  uint16_t voltage[3];
  uint16_t frequency;
  uint16_t degrees[2];

  uint16_t current[19];

  uint16_t end;
};

class PhaseConfig;
class PowerSensor;

class EmporiaVueComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void dump_config() override;

  void set_phases(std::vector<PhaseConfig *> phases);
  void set_power_sensors(std::vector<PowerSensor *> sensors);

  void update() override;

 private:
  std::vector<PhaseConfig *> _phases;
  std::vector<PowerSensor *> _power_sensors;
};

enum PhaseInputColor { BLACK, RED, BLUE };

class PhaseConfig {
 public:
  void set_input_color(PhaseInputColor input_color);
  int32_t extract_power_for_phase(const PowerDataEntry &entry);

 private:
  PhaseInputColor _input_color;
};

enum CTInputPort : uint8_t {
  A = 0,
  B = 1,
  C = 2,
  ONE = 3,
  TWO = 4,
  THREE = 5,
  FOUR = 6,
  FIVE = 7,
  SIX = 8,
  SEVEN = 9,
  EIGHT = 10,
  NINE = 11,
  TEN = 12,
  ELEVEN = 13,
  TWELVE = 14,
  THIRTEEN = 15,
  FOURTEEN = 16,
  FIFTEEN = 17,
  SIXTEEN = 18,
};

class PowerSensor : public sensor::Sensor {
 public:
  void set_phase(PhaseConfig *phase);
  void set_ct_input(CTInputPort ct_input);

  void update_from_data(const EmporiaSensorData &data);

 private:
  PhaseConfig *_phase;
  CTInputPort _ct_input;
};

}  // namespace emporia_vue
}  // namespace esphome
