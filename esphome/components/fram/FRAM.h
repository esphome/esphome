#pragma once
//
//    FILE: FRAM.h
//  AUTHOR: Rob Tillaart
// VERSION: 0.5.3
//    DATE: 2018-01-24
// PURPOSE: Arduino library for I2C FRAM
//     URL: https://github.com/RobTillaart/FRAM_I2C
//
// ESPHome port: https://github.com/sharkydog/esphome-fram

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace fram {

class Fram : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  bool is_connected();

  void write8(uint32_t memaddr, uint8_t value);
  void write16(uint32_t memaddr, uint16_t value);
  void write32(uint32_t memaddr, uint32_t value);
  void write_float(uint32_t memaddr, float value);
  void write_double(uint32_t memaddr, double value);
  void write(uint32_t memaddr, uint8_t *obj, uint32_t size);

  uint8_t read8(uint32_t memaddr);
  uint16_t read16(uint32_t memaddr);
  uint32_t read32(uint32_t memaddr);
  float read_float(uint32_t memaddr);
  double read_double(uint32_t memaddr);
  void read(uint32_t memaddr, uint8_t *obj, uint32_t size);

  template<class T> uint32_t write_object(uint32_t memaddr, T &obj);
  template<class T> uint32_t read_object(uint32_t memaddr, T &obj);

  uint16_t get_manufacturer_id() { return this->get_metadata_(0); }
  uint16_t get_product_id() { return this->get_metadata_(1); }
  uint16_t get_size();
  uint32_t get_size_bytes() { return this->size_bytes_; }
  void set_size_bytes(uint32_t value) { this->size_bytes_ = value; }

  uint32_t clear(uint8_t value = 0);
  void sleep();
  bool wakeup(uint32_t trec = 400);

 protected:
  uint32_t size_bytes_{0};

  uint16_t get_metadata_(uint8_t field);

  virtual void write_block(uint32_t memaddr, const uint8_t *obj, uint8_t size);
  virtual void read_block(uint32_t memaddr, uint8_t *obj, uint8_t size);
};

class Fram17 : public Fram {
 protected:
  void write_block(uint32_t memaddr, const uint8_t *obj, uint8_t size) override;
  void read_block(uint32_t memaddr, uint8_t *obj, uint8_t size) override;
};

class Fram11 : public Fram {
 protected:
  void write_block(uint32_t memaddr, const uint8_t *obj, uint8_t size) override;
  void read_block(uint32_t memaddr, uint8_t *obj, uint8_t size) override;
};

class Fram9 : public Fram {
 protected:
  void write_block(uint32_t memaddr, const uint8_t *obj, uint8_t size) override;
  void read_block(uint32_t memaddr, uint8_t *obj, uint8_t size) override;
};

}  // namespace fram
}  // namespace esphome
