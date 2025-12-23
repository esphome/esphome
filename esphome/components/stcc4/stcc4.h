#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

#include <cinttypes>

namespace esphome {
namespace stcc4 {

class STCC4Component : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_temp_sensor(sensor::Sensor *temp_sensor) { this->temp_sensor_ = temp_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }

  struct SensorState {
    bool is_idle = false;
    bool is_sleep = false;
    bool is_conditioning = false;
    bool is_testing_mode = false;
    bool is_sht45_present = false;
    bool is_rht_compensated = false;
    bool is_pressure_compensated = false;

    void reset() {
      is_idle = true;
      is_sleep = false;
      is_conditioning = false;
      is_testing_mode = false;
      is_sht45_present = false;
      is_rht_compensated = false;
      is_pressure_compensated = false;
    }
  };

  void set_continuous(bool continuous) { continuous_ = continuous; }

 protected:
  bool continuous_{false};
  SensorState state_;

  enum class SensorCommand : uint16_t {
    START_CONTINUOUS_MEASUREMENT = 0x218B,
    STOP_CONTINUOUS_MEASUREMENT = 0x3F86,
    READ_MEASUREMENT = 0xEC05,
    SET_RHT_COMPENSATION = 0xE000,
    SET_PRESSURE_COMPENSATION = 0xE016,
    MEASURE_SINGLE_SHOT = 0x219D,
    ENTER_SLEEP_MODE = 0x3650,
    EXIT_SLEEP_MODE = 0x0000,
    PERFORM_CONDITIONING = 0x29BC,
    PERFORM_SOFT_RESET = 0x0006,
    PERFORM_FACTORY_RESET = 0x3632,
    PERFORM_SELF_TEST = 0x278C,
    ENABLE_TESTING_MODE = 0x3FBC,
    DISABLE_TESTING_MODE = 0x3F3D,
    PERFORM_FORCED_RECALIBRATION = 0x362F,
    GET_PRODUCT_ID = 0x365B
  };

  void read_serial_number_();

  uint64_t serial_number_;

  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *temp_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};

 private:
  void start_continuous_measurement_();  // Called automatically
  void stop_continuous_measurement_();
  void read_measurement_(uint16_t *data);
  void set_rht_compensation_(uint16_t temp, uint16_t rh);
  void set_pressure_compensation_(uint16_t pressure);
  void measure_single_shot_(uint16_t *data);
  void enter_sleep_mode_();
  void exit_sleep_mode_();
  void perform_conditioning_();
  void perform_soft_reset_();
  void perform_factory_reset_();
  void perform_self_test_();
  void enable_testing_mode_();
  void disable_testing_mode_();
  void perform_forced_recalibration_();
};

}  // namespace stcc4
}  // namespace esphome
