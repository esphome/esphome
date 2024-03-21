#pragma once
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/tinovi/i2cArduino/blob/master/i2cArduino.h

namespace esphome {
namespace pmwcs3 {

class PMWCS3Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void set_e25_sensor(sensor::Sensor *e25_sensor) { e25_sensor_ = e25_sensor; }
  void set_ec_sensor(sensor::Sensor *ec_sensor) { ec_sensor_ = ec_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_vwc_sensor(sensor::Sensor *vwc_sensor) { vwc_sensor_ = vwc_sensor; }

  void new_i2c_address(uint8_t newaddress);
  void air_calibration();
  void water_calibration();

 protected:
  void read_data_();

  sensor::Sensor *e25_sensor_{nullptr};
  sensor::Sensor *ec_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vwc_sensor_{nullptr};
};

template<typename... Ts> class PMWCS3AirCalibrationAction : public Action<Ts...> {
 public:
  PMWCS3AirCalibrationAction(PMWCS3Component *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->air_calibration(); }

 protected:
  PMWCS3Component *parent_;
};

template<typename... Ts> class PMWCS3WaterCalibrationAction : public Action<Ts...> {
 public:
  PMWCS3WaterCalibrationAction(PMWCS3Component *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->water_calibration(); }

 protected:
  PMWCS3Component *parent_;
};

template<typename... Ts> class PMWCS3NewI2cAddressAction : public Action<Ts...> {
 public:
  PMWCS3NewI2cAddressAction(PMWCS3Component *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(int, new_address)

  void play(Ts... x) override { this->parent_->new_i2c_address(this->new_address_.value(x...)); }

 protected:
  PMWCS3Component *parent_;
};

}  // namespace pmwcs3
}  // namespace esphome
