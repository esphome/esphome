#pragma once

#include <vector>

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome {
namespace emporia_vue {

struct __attribute__((__packed__)) PowerDataEntry {
  int32_t phase_one;
  int32_t phase_two;
  int32_t phase_three;
};

struct __attribute__((__packed__)) EmporiaSensorData {
  uint8_t read_flag; // 0 = stale, 3 = new data
  uint8_t unknown1;  // checksum?
  uint8_t checksum;  // checksum?
  uint8_t timer;     // tick each ~510ms

  PowerDataEntry power[19];

  uint16_t voltage[3];
  uint16_t frequency;
  uint16_t degrees[2];

  uint16_t current[19];

  uint16_t end;
};

class PhaseConfig;
class CTSensor;

class EmporiaVueComponent : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  static void i2c_request_task(void *pv);

  void set_phases(std::vector<PhaseConfig *> phases) { this->phases_ = phases; }
  void set_ct_sensors(std::vector<CTSensor *> sensors) { this->ct_sensors_ = sensors; }

 private:
  std::vector<PhaseConfig *> phases_;
  std::vector<CTSensor *> ct_sensors_;
  QueueHandle_t i2c_data_queue_;
};

enum PhaseInputColor { BLACK, RED, BLUE };

class PhaseConfig {
 public:
  void set_input_color(PhaseInputColor input_color) { this->input_color_ = input_color; }
  void set_calibration(double calibration) {this->calibration_ = calibration; }
  double get_calibration() { return this->calibration_; }
  int32_t extract_power_for_phase(const PowerDataEntry &entry);

 private:
  PhaseInputColor input_color_;
  double calibration_;
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

class CTSensor : public sensor::Sensor {
 public:
  void set_phase(PhaseConfig *phase) { this->phase_ = phase; };
  void set_ct_input(CTInputPort ct_input) { this->ct_input_ = ct_input; };

  void update_from_data(const EmporiaSensorData &data);
  double get_calibrated_power(int32_t raw_power);
  static double get_calibrated_power(int32_t raw_power, double calibration, double correction_factor);

 private:
  PhaseConfig *phase_;
  CTInputPort ct_input_;
};

}  // namespace emporia_vue
}  // namespace esphome
