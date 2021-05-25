#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace ufire_ec {

static const float PROBE_COEFFICIENT = 0.019;

static const uint8_t CONFIG_TEMP_COMPENSATION = 0x02;

static const uint8_t REGISTER_VERSION = 0;
static const uint8_t REGISTER_MS = 1;
static const uint8_t REGISTER_TEMP = 5;
static const uint8_t REGISTER_SOLUTION = 9;
static const uint8_t REGISTER_COMPENSATION = 45;
static const uint8_t REGISTER_CONFIG = 54;
static const uint8_t REGISTER_TASK = 55;

static const uint8_t COMMAND_CALIBRATE_PROBE = 20;
static const uint8_t COMMAND_MEASURE_TEMP = 40;
static const uint8_t COMMAND_MEASURE_EC = 80;

class UFireECComponent : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_temperature_sensor_external(sensor::Sensor *temperature_sensor) {
    this->temperature_sensor_external_ = temperature_sensor;
  }
  void set_ec_sensor(sensor::Sensor *ec_sensor) { this->ec_sensor_ = ec_sensor; }
  void set_temperature_compensation(float compensation) { this->temperature_compensation_ = compensation; }
  void calibrate_probe(float solution, float temperature);

 protected:
  float measure_temperature_();
  float measure_ms_();
  void set_solution_(float solution, float temperature);
  void set_compensation_(float temperature);
  void set_temperature_(float temperature);
  float read_data_(uint8_t reg);
  void update_internal_();

  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_external_{nullptr};
  sensor::Sensor *ec_sensor_{nullptr};
  float temperature_compensation_{0.0};
};

template<typename... Ts> class UFireISECalibrateProbeAction : public Action<Ts...> {
 public:
  UFireISECalibrateProbeAction(UFireECComponent *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(float, solution)
  TEMPLATABLE_VALUE(float, temperature)

  void play(Ts... x) override {
    this->parent_->calibrate_probe(this->solution_.value(x...), this->temperature_.value(x...));
  }

 protected:
  UFireECComponent *parent_;
};

}  // namespace ufire_ec
}  // namespace esphome
