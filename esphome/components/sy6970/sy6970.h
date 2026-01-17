#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

namespace esphome {
namespace sy6970 {

// SY6970 Register addresses
static const uint8_t SY6970_REG_00 = 0x00;
static const uint8_t SY6970_REG_01 = 0x01;
static const uint8_t SY6970_REG_02 = 0x02;
static const uint8_t SY6970_REG_03 = 0x03;
static const uint8_t SY6970_REG_04 = 0x04;
static const uint8_t SY6970_REG_05 = 0x05;
static const uint8_t SY6970_REG_06 = 0x06;
static const uint8_t SY6970_REG_07 = 0x07;
static const uint8_t SY6970_REG_08 = 0x08;
static const uint8_t SY6970_REG_09 = 0x09;
static const uint8_t SY6970_REG_0A = 0x0A;
static const uint8_t SY6970_REG_0B = 0x0B;
static const uint8_t SY6970_REG_0C = 0x0C;
static const uint8_t SY6970_REG_0D = 0x0D;
static const uint8_t SY6970_REG_0E = 0x0E;
static const uint8_t SY6970_REG_11 = 0x11;
static const uint8_t SY6970_REG_12 = 0x12;
static const uint8_t SY6970_REG_13 = 0x13;
static const uint8_t SY6970_REG_14 = 0x14;

// Bus Status values (REG_0B[7:5])
enum BusStatus {
  BUS_STATUS_NO_INPUT = 0,
  BUS_STATUS_USB_SDP = 1,
  BUS_STATUS_USB_CDP = 2,
  BUS_STATUS_USB_DCP = 3,
  BUS_STATUS_HVDCP = 4,
  BUS_STATUS_ADAPTER = 5,
  BUS_STATUS_NO_STD_ADAPTER = 6,
  BUS_STATUS_OTG = 7,
};

// Charge Status values (REG_0B[4:3])
enum ChargeStatus {
  CHARGE_STATUS_NOT_CHARGING = 0,
  CHARGE_STATUS_PRE_CHARGE = 1,
  CHARGE_STATUS_FAST_CHARGE = 2,
  CHARGE_STATUS_CHARGE_DONE = 3,
};

class SY6970Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Get voltage readings (in millivolts)
  uint16_t get_vbus_voltage();
  uint16_t get_battery_voltage();
  uint16_t get_system_voltage();

  // Get current readings (in milliamps)
  uint16_t get_charge_current();
  uint16_t get_precharge_current();

  // Get status
  bool is_vbus_connected();
  bool is_charging();
  bool is_charge_done();
  const char *get_bus_status_string();
  const char *get_charge_status_string();
  const char *get_ntc_status_string();
  uint8_t get_bus_status();
  uint8_t get_charge_status();
  uint8_t get_ntc_status();

  // Configuration methods
  void set_input_current_limit(uint16_t milliamps);
  void set_charge_target_voltage(uint16_t millivolts);
  void set_precharge_current(uint16_t milliamps);
  void set_charge_current(uint16_t milliamps);
  void enable_charge();
  void disable_charge();
  void enable_status_led();
  void disable_status_led();
  void enable_adc_measure();

  // Get configuration values
  uint16_t get_charge_target_voltage();
  uint16_t get_charge_constant_current();

 protected:
  bool read_register_(uint8_t reg, uint8_t *value);
  bool write_register_(uint8_t reg, uint8_t value);
  bool update_register_(uint8_t reg, uint8_t mask, uint8_t value);

  bool initialized_{false};
};

}  // namespace sy6970
}  // namespace esphome
