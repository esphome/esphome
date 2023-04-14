#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/uart/uart.h"

#include "esphome/components/sensor/sensor.h"
#ifdef USE_TEXT_SENSOR
#include "esphome/components/text_sensor/text_sensor.h"
#endif

#ifndef PYLONTECH_NUM_BATTERIES
#warning PYLONTECH_NUM_BATTERIES not defined!
#define PYLONTECH_NUM_BATTERIES 3  // NOLINT
#endif

namespace esphome {
namespace pylontech {

static const uint8_t NUM_BUFFERS = 20;
static const uint8_t TEXT_SENSOR_MAX_LEN = 8;

#define PYLONTECH_ENTITY_(type, name) \
 protected: \
  type *name##_[PYLONTECH_NUM_BATTERIES]{}; /* NOLINT */ \
\
 public: \
  void set_##name(type *name, int batnum) { /* NOLINT */ \
    this->name##_[batnum - 1] = name; \
  }

#define PYLONTECH_SENSOR(name) PYLONTECH_ENTITY_(sensor::Sensor, name)
#define PYLONTECH_TEXT_SENSOR(name) PYLONTECH_ENTITY_(text_sensor::TextSensor, name)

class PylontechComponent : public PollingComponent, public uart::UARTDevice {
 public:
  PYLONTECH_SENSOR(voltage)
  PYLONTECH_SENSOR(current)
  PYLONTECH_SENSOR(temperature)
  PYLONTECH_SENSOR(temperature_low)
  PYLONTECH_SENSOR(temperature_high)
  PYLONTECH_SENSOR(voltage_low)
  PYLONTECH_SENSOR(voltage_high)

  PYLONTECH_SENSOR(coulomb)
  PYLONTECH_SENSOR(mos_temperature)

#ifdef USE_TEXT_SENSOR
  PYLONTECH_TEXT_SENSOR(base_state)
  PYLONTECH_TEXT_SENSOR(voltage_state)
  PYLONTECH_TEXT_SENSOR(current_state)
  PYLONTECH_TEXT_SENSOR(temperature_state)
#endif
  PylontechComponent();

  void mark_battery_index_in_use(uint8_t max_bat) {
    if (max_bat > this->max_battery_index_)
      this->max_battery_index_ = max_bat;
  }

  /// Schedule data readings.
  void update() override;
  /// Read data once available
  void loop() override;
  /// Setup the sensor and test for a connection.
  void setup() override;
  void dump_config() override;

  float get_setup_priority() const override;

 protected:
  void process_line_(std::string &buffer);

  uint8_t max_battery_index_ = 0;

  // ring buffer
  std::string buffer_[NUM_BUFFERS];
  int buffer_index_write_ = 0;
  int buffer_index_read_ = 0;
};

class PylontechTextComponent : public Component {
 public:
  PylontechTextComponent(PylontechComponent *parent) {}
};

}  // namespace pylontech
}  // namespace esphome
