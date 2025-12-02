#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/gpio.h"
#include "xensiv_dps3xx.h"
namespace esphome {
namespace xensiv_dps3xx_base {

class XensivDPS3xx : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_dps_sensor(sensor::Sensor *dps_sensor) { dps_sensor_ = dps_sensor; }
  void set_interrupt_pin(InternalGPIOPin *pin) { interrupt_pin_ = pin; }
  // void set_sensor_rate_value(int16_t rate) { sensor_rate_ = rate; }
  // void set_operation_mode(bool mode) { continuous_operation_mode_ = mode; }
  bool measure_now();

 protected:
  sensor::Sensor *dps_sensor_{nullptr};
  uint16_t version_{2};
  // int16_t sensor_rate_{10};               // Default rate in seconds

  xensiv_dps3xx_t dps_obj_{};  // DPS3xx sensor object (zero-initialized)
  xensiv_dps3xx_i2c_addr_t i2c_addr_{XENSIV_DPS3XX_I2C_ADDR_DEFAULT};

  static void gpio_intr(XensivDPS3xx *arg);
  bool test_scratch_register_();

  // Static I2C wrapper functions for the library
  static cy_rslt_t i2c_read_wrapper(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr, uint8_t *data,
                                    uint8_t length);
  static cy_rslt_t i2c_write_wrapper(void *context, uint16_t timeout, uint8_t i2c_addr, uint8_t reg_adr, uint8_t *data,
                                     uint8_t length);
  static cy_rslt_t delay_wrapper(uint32_t ms);

  virtual bool read_byte(uint8_t reg, uint8_t *data) = 0;
  virtual bool read_bytes(uint8_t reg, uint8_t *data, size_t len) = 0;
  virtual bool write_byte(uint8_t reg, uint8_t value) = 0;

  InternalGPIOPin *interrupt_pin_{nullptr};
  volatile bool data_ready_{false};

  std::string failure_reason_;
};

}  // namespace xensiv_dps3xx_base
}  // namespace esphome
