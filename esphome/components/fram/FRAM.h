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

class FRAM : public Component, public i2c::I2CDevice
{
public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  bool     isConnected();

  void     write8(uint16_t memaddr, uint8_t value);
  void     write16(uint16_t memaddr, uint16_t value);
  void     write32(uint16_t memaddr, uint32_t value);
  void     writeFloat(uint16_t memaddr, float value);
  void     writeDouble(uint16_t memaddr, double value);
  void     write(uint16_t memaddr, uint8_t * obj, uint16_t size);

  uint8_t  read8(uint16_t memaddr);
  uint16_t read16(uint16_t memaddr);
  uint32_t read32(uint16_t memaddr);
  float    readFloat(uint16_t memaddr);
  double   readDouble(uint16_t memaddr);
  void     read(uint16_t memaddr, uint8_t * obj, uint16_t size);

  //  Experimental 0.5.1
  //  readUntil returns length 0.. n of the buffer.
  //  readUntil does NOT include the separator character.
  //  readUntil returns -1 if data does not fit into buffer,
  //  =>  separator not encountered.
  int32_t readUntil(uint16_t memaddr, char * buf, uint16_t buflen, char separator);
  //  readLine returns length 0.. n of the buffer.
  //  readLine does include '\n' as end character.
  //  readLine returns -1 if data does not fit into buffer.
  //  buffer needs one place for end char '\0'.
  int32_t readLine(uint16_t memaddr, char * buf, uint16_t buflen);

  template <class T> uint16_t writeObject(uint16_t memaddr, T &obj)
  {
    this->write(memaddr, (uint8_t *) &obj, sizeof(obj));
    return memaddr + sizeof(obj);
  };
  template <class T> uint16_t readObject(uint16_t memaddr, T &obj)
  {
    this->read(memaddr, (uint8_t *) &obj, sizeof(obj));
    return memaddr + sizeof(obj);
  }

  //  meta info
  //  Fujitsu = 0x000A, Ramtron = 0x004
  uint16_t getManufacturerID();
  //  Proprietary
  uint16_t getProductID();
  //  Returns size in KILO-BYTE (or 0)
  //  Verify for all manufacturers.
  uint16_t getSize();
  //  Returns size in BYTE
  uint32_t getSizeBytes();
  //  override when getSize() fails == 0 (see readme.md)
  void     setSizeBytes(uint32_t value);

  //  fills FRAM with value, default 0.
  uint32_t clear(uint8_t value = 0);

  //  0.3.6
  void sleep();
  //  trec <= 400us  P12
  bool wakeup(uint32_t trec = 400);


protected:
  uint32_t _sizeBytes{0};

  uint16_t _getMetaData(uint8_t id);

  //  virtual so derived classes FRAM9/11 use their implementation.
  virtual void _writeBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
  virtual void _readBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
};


/////////////////////////////////////////////////////////////////////////////
//
//  FRAM32
//

class FRAM32 : public FRAM
{
public:
  void     write8(uint32_t memaddr, uint8_t value);
  void     write16(uint32_t memaddr, uint16_t value);
  void     write32(uint32_t memaddr, uint32_t value);
  void     writeFloat(uint32_t memaddr, float value);
  void     writeDouble(uint32_t memaddr, double value);
  void     write(uint32_t memaddr, uint8_t * obj, uint16_t size);

  uint8_t  read8(uint32_t memaddr);
  uint16_t read16(uint32_t memaddr);
  uint32_t read32(uint32_t memaddr);
  float    readFloat(uint32_t memaddr);
  double   readDouble(uint32_t memaddr);
  void     read(uint32_t memaddr, uint8_t * obj, uint16_t size);

  //  readUntil returns length 0.. n of the buffer.
  //  readUntil returns -1 if data does not fit into buffer,
  //  =>  separator not encountered.
  int32_t readUntil(uint32_t memaddr, char * buf, uint16_t buflen, char separator);
  //  buffer needs one place for end char '\0'.
  int32_t readLine(uint32_t memaddr, char * buf, uint16_t buflen);

  template <class T> uint32_t writeObject(uint32_t memaddr, T &obj);
  template <class T> uint32_t readObject(uint32_t memaddr, T &obj);

protected:
  void     _writeBlock(uint32_t memaddr, uint8_t * obj, uint8_t size);
  void     _readBlock(uint32_t memaddr, uint8_t * obj, uint8_t size);
};


/////////////////////////////////////////////////////////////////////////////
//
//  FRAM11  for FRAM that use 11 bits addresses - e.g. MB85RC16
//

class FRAM11 : public FRAM
{
protected:
  void     _writeBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
  void     _readBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
};


/////////////////////////////////////////////////////////////////////////////
//
//  FRAM9  for FRAM that use 9 bits addresses - e.g. MB85RC04
//
class FRAM9 : public FRAM
{
protected:
  void     _writeBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
  void     _readBlock(uint16_t memaddr, uint8_t * obj, uint8_t size);
};

}  // namespace fram
}  // namespace esphome

//  -- END OF FILE --
