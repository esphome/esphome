#pragma once

#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace pas_co2 {

enum OperationMode : uint8_t {
  OP_MODE_IDLE = 0U,   /**< The device does not perform any CO2 concentration measurement */
  OP_MODE_SINGLE = 1U, /**< The device triggers a single measurement sequence. At the end of the measurement sequence,
                          the device automatically goes back to idle mode. */
  OP_MODE_CONTINUOUS = 2U /**< The device periodically triggers a CO2 concentration measurement sequence.
                             Once a measurement sequence is completed, the device goes back to an inactive state
                             and wakes up automatically for the next measurement sequence. The measurement period can
                             be programmed from 5 seconds to 4095 seconds. */
};

/** Enum defining the different device baseline offset compensation (BOC) modes */
enum BOCMode : uint8_t {
  BOC_CFG_DISABLE = 0U,   /**< No offset compensation occurs */
  BOC_CFG_AUTOMATIC = 1U, /**< The offset is periodically updated at each BOC computation */
  BOC_CFG_FORCED = 2U     /**< Forced compensation */
};

/** Structure of the sensor's measurement configuration register (MEAS_CFG) */
union measurement_config_t {
  struct {
    uint32_t op_mode : 2;
    uint32_t boc_cfg : 2;
    uint32_t pwm_mode : 1;
    uint32_t pwm_outen : 1;
    uint32_t : 2;
  } b;       /*!< Structure used for bit  access */
  uint8_t u; /*!< Type used for byte access */
};

class PasCo2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void update() override;
  void dump_config() override;

  void set_co2_sensor(sensor::Sensor *co2_sensor) { co2_sensor_ = co2_sensor; }
  void set_ambient_pressure_source(sensor::Sensor *pressure) { ambient_pressure_source_ = pressure; }
  void set_ambient_pressure_compensation(float pressure_in_hpa);
  void set_calibration_offset(uint16_t offset) { this->calib_ref_ = offset; }
  void set_abc_enable(bool enable) { this->enable_abc_ = enable; }
  void start_foced_calibration();
  bool update_abc_enable(bool enable);

 protected:
  i2c::ErrorCode read_register_(uint8_t a_register, uint8_t *data, size_t len) {
    // using stop=true leads to NACK's after a few minutes.
    auto err = this->write(&a_register, 1, false);
    // a 5 ms delay between write and read is recommended.
    delay(5);
    if (err != i2c::ERROR_OK)
      return err;
    return this->read(data, len);
  }
  i2c::ErrorCode read_register_(uint8_t a_register, uint8_t &data) { return read_register_(a_register, &data, 1); }
  i2c::ErrorCode read_register_(uint8_t a_register, uint16_t &data) {
    uint16_t raw = 0;
    auto result = this->read_register_(a_register, reinterpret_cast<uint8_t *>(&raw), 2);
    data = i2c::i2ctohs(raw);
    return result;
  }

  i2c::ErrorCode write_register_(uint8_t a_register, uint8_t data) {
    return this->write_register(a_register, &data, 1, false);
  }
  i2c::ErrorCode write_register_(uint8_t a_register, uint16_t data) {
    auto raw = i2c::htoi2cs(data);
    return this->write_register(a_register, reinterpret_cast<uint8_t *>(&raw), 2, false);
  }

  i2c::ErrorCode set_measurement_rate_(uint16_t rate_in_seconds);

  bool update_ambient_pressure_compensation_(uint16_t pressure_in_hpa);
  sensor::Sensor *co2_sensor_{nullptr};
  // used for compensation
  sensor::Sensor *ambient_pressure_source_{nullptr};
  bool ambient_pressure_compensation_;
  uint16_t ambient_pressure_{0};
  uint8_t product_id_;
  uint16_t calib_ref_{0};
  bool enable_abc_{true};
  bool initialized_{false};
};

template<typename... Ts> class PerformForcedCalibrationAction : public Action<Ts...> {
 public:
  PerformForcedCalibrationAction(PasCo2Component *pas_co2) : pas_co2_(pas_co2) {}

  void play(Ts... x) override { this->pas_co2_->start_foced_calibration(); }

 protected:
  PasCo2Component *pas_co2_;
};

template<typename... Ts> class ABCEnableAction : public Action<Ts...> {
 public:
  ABCEnableAction(PasCo2Component *pas_co2) : pas_co2_(pas_co2) {}

  void play(Ts... x) override { this->pas_co2_->update_abc_enable(true); }

 protected:
  PasCo2Component *pas_co2_;
};

template<typename... Ts> class ABCDisableAction : public Action<Ts...> {
 public:
  ABCDisableAction(PasCo2Component *pas_co2) : pas_co2_(pas_co2) {}

  void play(Ts... x) override { this->pas_co2_->update_abc_enable(false); }

 protected:
  PasCo2Component *pas_co2_;
};

}  // namespace pas_co2
}  // namespace esphome
