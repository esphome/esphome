#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

// ref:
// https://github.com/tinovi/i2cArduino/blob/master/i2cArduino.h

namespace esphome {
namespace pmwcs3 {

static const uint8_t PMWCS3_I2C_ADDRESS         = 0x63;

static const uint8_t PMWCS3_REG_READ_START      = 0x01;

static const uint8_t PMWCS3_REG_READ_E25        = 0x02;
static const uint8_t PMWCS3_REG_READ_EC         = 0x03;
static const uint8_t PMWCS3_REG_READ_TEMP       = 0x04;
static const uint8_t PMWCS3_REG_READ_VWC        = 0x05;

static const uint8_t PMWCS3_REG_CALIBRATE_AIR   = 0x06;
static const uint8_t PMWCS3_REG_CALIBRATE_WATER = 0x07;

static const uint8_t PMWCS3_SET_I2C_ADDRESS     = 0x08;

static const uint8_t PMWCS3_REG_GET_DATA        = 0x09;

static const uint8_t PMWCS3_REG_CALIBRATE_EC    = 0x10;


static const uint8_t PMWCS3_REG_CAP             = 0x0A;
static const uint8_t PMWCS3_REG_RES             = 0x0B;
static const uint8_t PMWCS3_REG_RC              = 0x0C;
static const uint8_t PMWCS3_REG_RT              = 0x0D;

class PMWCS3Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void  setup() override;
  void  update() override;
  void  dump_config() override;
  float get_setup_priority() const override;
  
  void set_e25_sensor(sensor::Sensor *e25_sensor) { e25_sensor_ = e25_sensor; }
  void set_ec_sensor(sensor::Sensor *ec_sensor) { ec_sensor_ = ec_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_vwc_sensor(sensor::Sensor *vwc_sensor) { vwc_sensor_ = vwc_sensor; }
 
  void change_i2c_address(uint8_t new_address);
  void set_air_calibration(void);
  void set_water_calibration(void);
   
 protected:
  void read_data_();
  
  sensor::Sensor *e25_sensor_{nullptr};
  sensor::Sensor *ec_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *vwc_sensor_{nullptr};
    
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    ID_REGISTERS,
  } error_code_;
  
};

}  // namespace pmwcs3
}  // namespace esphome
