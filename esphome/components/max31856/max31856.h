#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/component.h"

#include <cinttypes>

namespace esphome {
namespace max31856 {

enum MAX31856RegisterMasks { SPI_WRITE_M = 0x80 };

enum MAX31856Registers {
  MAX31856_CR0_REG = 0x00,          ///< Config 0 register
  MAX31856_CR0_AUTOCONVERT = 0x80,  ///< Config 0 Auto convert flag
  MAX31856_CR0_1SHOT = 0x40,        ///< Config 0 one shot convert flag
  MAX31856_CR0_OCFAULT00 = 0x00,    ///< Config 0 open circuit fault 00 flag
  MAX31856_CR0_OCFAULT01 = 0x10,    ///< Config 0 open circuit fault 01 flag
  MAX31856_CR0_OCFAULT10 = 0x20,    ///< Config 0 open circuit fault 10 flag
  MAX31856_CR0_CJ = 0x08,           ///< Config 0 cold junction disable flag
  MAX31856_CR0_FAULT = 0x04,        ///< Config 0 fault mode flag
  MAX31856_CR0_FAULTCLR = 0x02,     ///< Config 0 fault clear flag

  MAX31856_CR1_REG = 0x01,     ///< Config 1 register
  MAX31856_MASK_REG = 0x02,    ///< Fault Mask register
  MAX31856_CJHF_REG = 0x03,    ///< Cold junction High temp fault register
  MAX31856_CJLF_REG = 0x04,    ///< Cold junction Low temp fault register
  MAX31856_LTHFTH_REG = 0x05,  ///< Linearized Temperature High Fault Threshold Register, MSB
  MAX31856_LTHFTL_REG = 0x06,  ///< Linearized Temperature High Fault Threshold Register, LSB
  MAX31856_LTLFTH_REG = 0x07,  ///< Linearized Temperature Low Fault Threshold Register, MSB
  MAX31856_LTLFTL_REG = 0x08,  ///< Linearized Temperature Low Fault Threshold Register, LSB
  MAX31856_CJTO_REG = 0x09,    ///< Cold-Junction Temperature Offset Register
  MAX31856_CJTH_REG = 0x0A,    ///< Cold-Junction Temperature Register, MSB
  MAX31856_CJTL_REG = 0x0B,    ///< Cold-Junction Temperature Register, LSB
  MAX31856_LTCBH_REG = 0x0C,   ///< Linearized TC Temperature, Byte 2
  MAX31856_LTCBM_REG = 0x0D,   ///< Linearized TC Temperature, Byte 1
  MAX31856_LTCBL_REG = 0x0E,   ///< Linearized TC Temperature, Byte 0
  MAX31856_SR_REG = 0x0F,      ///< Fault Status Register

  MAX31856_FAULT_CJRANGE = 0x80,  ///< Fault status Cold Junction Out-of-Range flag
  MAX31856_FAULT_TCRANGE = 0x40,  ///< Fault status Thermocouple Out-of-Range flag
  MAX31856_FAULT_CJHIGH = 0x20,   ///< Fault status Cold-Junction High Fault flag
  MAX31856_FAULT_CJLOW = 0x10,    ///< Fault status Cold-Junction Low Fault flag
  MAX31856_FAULT_TCHIGH = 0x08,   ///< Fault status Thermocouple Temperature High Fault flag
  MAX31856_FAULT_TCLOW = 0x04,    ///< Fault status Thermocouple Temperature Low Fault flag
  MAX31856_FAULT_OVUV = 0x02,     ///< Fault status Overvoltage or Undervoltage Input Fault flag
  MAX31856_FAULT_OPEN = 0x01,     ///< Fault status Thermocouple Open-Circuit Fault flag
};

/**
 * Multiple types of thermocouples supported by the chip.
 * Currently only K type implemented here.
 */
enum MAX31856ThermocoupleType {
  MAX31856_TCTYPE_B = 0b0000,   // 0x00
  MAX31856_TCTYPE_E = 0b0001,   // 0x01
  MAX31856_TCTYPE_J = 0b0010,   // 0x02
  MAX31856_TCTYPE_K = 0b0011,   // 0x03
  MAX31856_TCTYPE_N = 0b0100,   // 0x04
  MAX31856_TCTYPE_R = 0b0101,   // 0x05
  MAX31856_TCTYPE_S = 0b0110,   // 0x06
  MAX31856_TCTYPE_T = 0b0111,   // 0x07
  MAX31856_VMODE_G8 = 0b1000,   // 0x08
  MAX31856_VMODE_G32 = 0b1100,  // 0x12
};

enum MAX31856ConfigFilter {
  FILTER_60HZ = 0,
  FILTER_50HZ = 1,
};

class MAX31856Sensor : public sensor::Sensor,
                       public PollingComponent,
                       public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                             spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void set_filter(MAX31856ConfigFilter filter) { filter_ = filter; }
  void update() override;

 protected:
  MAX31856ConfigFilter filter_;

  uint8_t read_register_(uint8_t reg);
  uint32_t read_register24_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value);

  void one_shot_temperature_();
  bool has_fault_();
  void clear_fault_();
  void read_thermocouple_temperature_();
  void set_thermocouple_type_();
  void set_noise_filter_();
};

}  // namespace max31856
}  // namespace esphome
