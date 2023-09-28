#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/ade7953_base/ade7953_base.h"

#include <vector>

namespace esphome {
namespace ade7953_spi {

class AdE7953Spi : public ade7953_base::ADE7953,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH, spi::CLOCK_PHASE_LEADING,
                                         spi::DATA_RATE_4MHZ> {
 public:
  void setup() override {
    this->spi_setup();
    ade7953_base::ADE7953::setup();
  }

  void dump_config() override;

 protected:
  bool ade_write_8(uint16_t reg, uint8_t value) override {
    this->enable();
    this->write_byte16(reg);
    this->transfer_byte(0);
    this->transfer_byte(value);
    this->disable();
    return false;
  }
  bool ade_write_16(uint16_t reg, uint16_t value) override {
    this->enable();
    this->write_byte16(reg);
    this->transfer_byte(0);
    this->write_byte16(value);
    this->disable();
    return false;
  }
  bool ade_write_32(uint16_t reg, uint32_t value) override {
    this->enable();
    this->write_byte16(reg);
    this->transfer_byte(0);
    this->write_byte16(value >> 16);
    this->write_byte16(value & 0xFFFF);
    this->disable();
    return false;
  }
  bool ade_read_16(uint16_t reg, uint16_t *value) override {
    this->enable();
    this->write_byte16(reg);
    this->transfer_byte(0x80);
    *value  = 0;
    *value |= this->read_byte() << 8;
    *value |= this->read_byte();
    this->disable();
    return false;
  }
  bool ade_read_32(uint16_t reg, uint32_t *value) override {
    this->enable();
    this->write_byte16(reg);
    this->transfer_byte(0x80);
    *value = 0;
    *value |= this->read_byte() << 24;
    *value |= this->read_byte() << 16;
    *value |= this->read_byte() << 8;
    *value |= this->read_byte();
    this->disable();
    return false;
  }
};

}  // namespace ade7953_spi
}  // namespace esphome
