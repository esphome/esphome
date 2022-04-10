#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mpl3115a2 {

// enums from https://github.com/adafruit/Adafruit_MPL3115A2_Library/
/** MPL3115A2 registers **/
enum {
  MPL3115A2_REGISTER_STATUS = (0x00),

  MPL3115A2_REGISTER_PRESSURE_MSB = (0x01),
  MPL3115A2_REGISTER_PRESSURE_CSB = (0x02),
  MPL3115A2_REGISTER_PRESSURE_LSB = (0x03),

  MPL3115A2_REGISTER_TEMP_MSB = (0x04),
  MPL3115A2_REGISTER_TEMP_LSB = (0x05),

  MPL3115A2_REGISTER_DR_STATUS = (0x06),

  MPL3115A2_OUT_P_DELTA_MSB = (0x07),
  MPL3115A2_OUT_P_DELTA_CSB = (0x08),
  MPL3115A2_OUT_P_DELTA_LSB = (0x09),

  MPL3115A2_OUT_T_DELTA_MSB = (0x0A),
  MPL3115A2_OUT_T_DELTA_LSB = (0x0B),

  MPL3115A2_WHOAMI = (0x0C),

  MPL3115A2_BAR_IN_MSB = (0x14),
  MPL3115A2_BAR_IN_LSB = (0x15),
};

/** MPL3115A2 status register bits **/
enum {
  MPL3115A2_REGISTER_STATUS_TDR = 0x02,
  MPL3115A2_REGISTER_STATUS_PDR = 0x04,
  MPL3115A2_REGISTER_STATUS_PTDR = 0x08,
};

/** MPL3115A2 PT DATA register bits **/
enum {
  MPL3115A2_PT_DATA_CFG = 0x13,
  MPL3115A2_PT_DATA_CFG_TDEFE = 0x01,
  MPL3115A2_PT_DATA_CFG_PDEFE = 0x02,
  MPL3115A2_PT_DATA_CFG_DREM = 0x04,
};

/** MPL3115A2 control registers **/
enum {

  MPL3115A2_CTRL_REG1 = (0x26),
  MPL3115A2_CTRL_REG2 = (0x27),
  MPL3115A2_CTRL_REG3 = (0x28),
  MPL3115A2_CTRL_REG4 = (0x29),
  MPL3115A2_CTRL_REG5 = (0x2A),
};

/** MPL3115A2 control register bits **/
enum {
  MPL3115A2_CTRL_REG1_SBYB = 0x01,
  MPL3115A2_CTRL_REG1_OST = 0x02,
  MPL3115A2_CTRL_REG1_RST = 0x04,
  MPL3115A2_CTRL_REG1_RAW = 0x40,
  MPL3115A2_CTRL_REG1_ALT = 0x80,
  MPL3115A2_CTRL_REG1_BAR = 0x00,
};

/** MPL3115A2 oversample values **/
enum {
  MPL3115A2_CTRL_REG1_OS1 = 0x00,
  MPL3115A2_CTRL_REG1_OS2 = 0x08,
  MPL3115A2_CTRL_REG1_OS4 = 0x10,
  MPL3115A2_CTRL_REG1_OS8 = 0x18,
  MPL3115A2_CTRL_REG1_OS16 = 0x20,
  MPL3115A2_CTRL_REG1_OS32 = 0x28,
  MPL3115A2_CTRL_REG1_OS64 = 0x30,
  MPL3115A2_CTRL_REG1_OS128 = 0x38,
};

class MPL3115A2Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void set_temperature(sensor::Sensor *temperature) { temperature_ = temperature; }
  void set_altitude(sensor::Sensor *altitude) { altitude_ = altitude; }
  void set_pressure(sensor::Sensor *pressure) { pressure_ = pressure; }

  void setup() override;
  void dump_config() override;
  void update() override;

  float get_setup_priority() const override { return setup_priority::DATA; }

 protected:
  sensor::Sensor *temperature_{nullptr};
  sensor::Sensor *altitude_{nullptr};
  sensor::Sensor *pressure_{nullptr};
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};
};

}  // namespace mpl3115a2
}  // namespace esphome
