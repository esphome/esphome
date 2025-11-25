#pragma once

#include "esphome/components/xensiv_pas_co2_base/xensiv_pas_co2_base.h"
#include "esphome/components/i2c/i2c.h"
namespace esphome {
namespace xensiv_pas_co2_i2c {

class XensivPasCO2I2C : public esphome::xensiv_pas_co2_base::XensivPasCO2, public i2c::I2CDevice {
 public:
 protected:
  void dump_config() override;
  virtual bool read_byte(uint8_t reg, uint8_t *data) override;
  virtual bool read_bytes(uint8_t reg, uint8_t *data, size_t len) override;
  virtual bool write_byte(uint8_t reg, uint8_t value) override;
};

}  // namespace xensiv_pas_co2_i2c
}  // namespace esphome
