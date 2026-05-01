#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/preferences.h"

#ifdef USE_BSEC2

#include "esphome/components/bme68x_bsec2/bme68x_bsec2.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace bme68x_bsec2_i2c {

class BME68xBSEC2I2CComponent : public bme68x_bsec2::BME68xBSEC2Component, public i2c::I2CDevice {
  void setup() override;
  void dump_config() override;

  uint32_t get_hash() override;

  static int8_t read_bytes_wrapper(uint8_t a_register, uint8_t *data, uint32_t len, void *intfPtr);
  static int8_t write_bytes_wrapper(uint8_t a_register, const uint8_t *data, uint32_t len, void *intfPtr);
  static void delay_us(uint32_t period, void *intfPtr);
};

}  // namespace bme68x_bsec2_i2c
}  // namespace esphome
#endif
