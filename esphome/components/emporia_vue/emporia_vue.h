#pragma once

#ifdef USE_ESP32

#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"

#ifdef USING_OTA_COMPONENT
#include "esphome/components/ota/ota_component.h"
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace esphome {
namespace emporia_vue {

struct __attribute__((__packed__)) ReadingPowerEntry {
  int32_t phase_black;
  int32_t phase_red;
  int32_t phase_blue;
};

struct __attribute__((__packed__)) SensorReading {
  bool is_unread;
  uint8_t checksum;
  uint8_t unknown;
  uint8_t sequence_num;

  ReadingPowerEntry power[19];

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
  void dump_config() override;

  void set_sensor_poll_interval(uint32_t sensor_poll_interval) { this->sensor_poll_interval_ = sensor_poll_interval; }
  uint32_t get_sensor_poll_interval() const { return this->sensor_poll_interval_; }
  void set_phases(std::vector<PhaseConfig *> phases) { this->phases_ = std::move(phases); }
  void set_ct_sensors(std::vector<CTSensor *> sensors) { this->ct_sensors_ = std::move(sensors); }
#ifdef USING_OTA_COMPONENT
  void set_ota(ota::OTAComponent *ota) { this->ota_ = ota; }
#endif

  void setup() override;
  void loop() override;

 protected:
  static void i2c_request_task(void *pv);

  uint32_t sensor_poll_interval_;
  std::vector<PhaseConfig *> phases_;
  std::vector<CTSensor *> ct_sensors_;
  QueueHandle_t i2c_data_queue_;
#ifdef USING_OTA_COMPONENT
  ota::OTAComponent *ota_{nullptr};
#endif
  TaskHandle_t i2c_request_task_;
};

enum PhaseInputWire : uint8_t {
  BLACK = 0,
  RED = 1,
  BLUE = 2,
};

class PhaseConfig {
 public:
  void set_input_wire(PhaseInputWire input_wire) { this->input_wire_ = input_wire; }
  PhaseInputWire get_input_wire() const { return this->input_wire_; }
  void set_calibration(float calibration) { this->calibration_ = calibration; }
  float get_calibration() const { return this->calibration_; }
  void set_voltage_sensor(sensor::Sensor *voltage_sensor) { this->voltage_sensor_ = voltage_sensor; }
  sensor::Sensor *get_voltage_sensor() const { return this->voltage_sensor_; }

  void update_from_reading(const SensorReading &sensor_reading);

  int32_t extract_power_for_phase(const ReadingPowerEntry &power_entry);

 protected:
  PhaseInputWire input_wire_;
  float calibration_;
  sensor::Sensor *voltage_sensor_{nullptr};
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
  const PhaseConfig *get_phase() const { return this->phase_; }
  void set_input_port(CTInputPort input_port) { this->input_port_ = input_port; };
  CTInputPort get_input_port() const { return this->input_port_; }

  void update_from_reading(const SensorReading &sensor_reading);
  float get_calibrated_power(int32_t raw_power) const;

 protected:
  PhaseConfig *phase_;
  CTInputPort input_port_;
};

}  // namespace emporia_vue
}  // namespace esphome

#endif  // ifdef USE_ESP32
