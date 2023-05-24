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

class FRAM : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  bool is_connected();

  void write8(uint16_t memaddr, uint8_t value);
  void write16(uint16_t memaddr, uint16_t value);
  void write32(uint16_t memaddr, uint32_t value);
  void write_float(uint16_t memaddr, float value);
  void write_double(uint16_t memaddr, double value);
  void write(uint16_t memaddr, uint8_t *obj, uint16_t size);

  uint8_t read8(uint16_t memaddr);
  uint16_t read16(uint16_t memaddr);
  uint32_t read32(uint16_t memaddr);
  float read_float(uint16_t memaddr);
  double read_double(uint16_t memaddr);
  void read(uint16_t memaddr, uint8_t *obj, uint16_t size);

  //  Experimental 0.5.1
  //  read_until returns length 0.. n of the buffer.
  //  read_until does NOT include the separator character.
  //  read_until returns -1 if data does not fit into buffer,
  //  =>  separator not encountered.
  int32_t read_until(uint16_t memaddr, char *buf, uint16_t buflen, char separator);
  //  read_line returns length 0.. n of the buffer.
  //  read_line does include '\n' as end character.
  //  read_line returns -1 if data does not fit into buffer.
  //  buffer needs one place for end char '\0'.
  int32_t read_line(uint16_t memaddr, char *buf, uint16_t buflen);

  template<class T> uint16_t write_object(uint16_t memaddr, T &obj);
  template<class T> uint16_t read_object(uint16_t memaddr, T &obj);

  //  meta info
  //  Fujitsu = 0x000A, Ramtron = 0x004
  uint16_t get_manufacturer_id();
  //  Proprietary
  uint16_t get_product_id();
  //  Returns size in KILO-BYTE (or 0)
  //  Verify for all manufacturers.
  uint16_t get_size();
  //  Returns size in BYTE
  uint32_t get_size_bytes();
  //  override when get_size() fails == 0
  void set_size_bytes(uint32_t value);

  //  fills FRAM with value, default 0.
  uint32_t clear(uint8_t value = 0);

  //  0.3.6
  void sleep();
  //  trec <= 400us  P12
  bool wakeup(uint32_t trec = 400);

 protected:
  uint32_t size_bytes_{0};

  uint16_t get_metadata_(uint8_t field);
  //  virtual so derived classes FRAM9/11 use their implementation.
  virtual void write_block(uint16_t memaddr, const uint8_t *obj, uint8_t size);
  virtual void read_block(uint16_t memaddr, uint8_t *obj, uint8_t size);
};

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM32
//

class FRAM32 : public FRAM {
 public:
  void write8(uint32_t memaddr, uint8_t value);
  void write16(uint32_t memaddr, uint16_t value);
  void write32(uint32_t memaddr, uint32_t value);
  void write_float(uint32_t memaddr, float value);
  void write_double(uint32_t memaddr, double value);
  void write(uint32_t memaddr, uint8_t *obj, uint16_t size);

  uint8_t read8(uint32_t memaddr);
  uint16_t read16(uint32_t memaddr);
  uint32_t read32(uint32_t memaddr);
  float read_float(uint32_t memaddr);
  double read_double(uint32_t memaddr);
  void read(uint32_t memaddr, uint8_t *obj, uint16_t size);

  //  read_until returns length 0.. n of the buffer.
  //  read_until returns -1 if data does not fit into buffer,
  //  =>  separator not encountered.
  int32_t read_until(uint32_t memaddr, char *buf, uint16_t buflen, char separator);
  //  buffer needs one place for end char '\0'.
  int32_t read_line(uint32_t memaddr, char *buf, uint16_t buflen);

  template<class T> uint32_t write_object(uint32_t memaddr, T &obj);
  template<class T> uint32_t read_object(uint32_t memaddr, T &obj);

 protected:
  void write_block32_(uint32_t memaddr, const uint8_t *obj, uint8_t size);
  void read_block32_(uint32_t memaddr, uint8_t *obj, uint8_t size);
};

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM11  for FRAM that use 11 bits addresses - e.g. MB85RC16
//

class FRAM11 : public FRAM {
 protected:
  void write_block(uint16_t memaddr, const uint8_t *obj, uint8_t size) override;
  void read_block(uint16_t memaddr, uint8_t *obj, uint8_t size) override;
};

/////////////////////////////////////////////////////////////////////////////
//
//  FRAM9  for FRAM that use 9 bits addresses - e.g. MB85RC04
//
class FRAM9 : public FRAM {
 protected:
  void write_block(uint16_t memaddr, const uint8_t *obj, uint8_t size) override;
  void read_block(uint16_t memaddr, uint8_t *obj, uint8_t size) override;
};

}  // namespace fram
}  // namespace esphome

//  -- END OF FILE --
