#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ufire_ise {

static const float PROBE_MV_TO_PH = 59.2;
static const float PROBE_TMP_CORRECTION = 0.03;

static const uint8_t CONFIG_TEMP_COMPENSATION = 0x02;

static const uint8_t REGISTER_VERSION = 0;
static const uint8_t REGISTER_MV = 1;
static const uint8_t REGISTER_TEMP = 5;
static const uint8_t REGISTER_REFHIGH = 13;
static const uint8_t REGISTER_REFLOW = 17;
static const uint8_t REGISTER_READHIGH = 21;
static const uint8_t REGISTER_READLOW = 25;
static const uint8_t REGISTER_SOLUTION = 29;
static const uint8_t REGISTER_CONFIG = 38;
static const uint8_t REGISTER_TASK = 39;

static const uint8_t COMMAND_CALIBRATE_HIGH = 8;
static const uint8_t COMMAND_CALIBRATE_LOW = 10;
static const uint8_t COMMAND_MEASURE_TEMP = 40;
static const uint8_t COMMAND_MEASURE_MV = 80;

class UFireISEComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_temperature_sensor_external(sensor::Sensor *temperature_sensor) {
    this->temperature_sensor_external_ = temperature_sensor;
  }
  void set_ph_sensor(sensor::Sensor *ph_sensor) { this->ph_sensor_ = ph_sensor; }
  void calibrate_probe_low(float solution);
  void calibrate_probe_high(float solution);
  void reset_board();

 protected:
  float measure_temperature_();
  float measure_mv_();
  float measure_ph_(float temperature);
  void set_solution_(float solution);
  float read_data_(uint8_t reg);
  void write_data_(uint8_t reg, float data);
  void update_internal_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_external_{nullptr};
  sensor::Sensor *ph_sensor_{nullptr};
};

template<typename... Ts> class UFireISECalibrateProbeLowAction : public Action<Ts...> {
 public:
  UFireISECalibrateProbeLowAction(UFireISEComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, solution)

  void play(Ts... x) override { this->parent_->calibrate_probe_low(this->solution_.value(x...)); }

 protected:
  UFireISEComponent *parent_;
};

template<typename... Ts> class UFireISECalibrateProbeHighAction : public Action<Ts...> {
 public:
  UFireISECalibrateProbeHighAction(UFireISEComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, solution)

  void play(Ts... x) override { this->parent_->calibrate_probe_high(this->solution_.value(x...)); }

 protected:
  UFireISEComponent *parent_;
};

template<typename... Ts> class UFireISEResetAction : public Action<Ts...> {
 public:
  UFireISEResetAction(UFireISEComponent *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->reset_board(); }

 protected:
  UFireISEComponent *parent_;
};

}  // namespace ufire_ise
}  // namespace esphome
