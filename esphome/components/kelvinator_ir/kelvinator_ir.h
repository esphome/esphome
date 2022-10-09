#pragma once

/*
  This code could not have been completed without the prior work located here:
  https://github.com/crankyoldgit/IRremoteESP8266/tree/master/src
*/

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/kelvinator_protocol.h"

namespace esphome {
namespace kelvinator_ir {

union KelvinatorCommand {
  // Raw
  uint64_t raw64[2];
  uint8_t raw8[16];

  struct {
    // Byte 0
    uint8_t mode : 3;
    uint8_t power : 1;
    uint8_t basicfan : 2;
    uint8_t swingauto : 1;
    uint8_t : 1;  // Sleep Modes 1 & 3 (1 = On, 0 = Off)
    // Byte 1
    uint8_t temperature : 4;  // Degrees C.
    uint8_t : 4;
    // Byte 2
    uint8_t : 4;
    uint8_t turbo : 1;
    uint8_t light : 1;
    uint8_t ionfilter : 1;
    uint8_t xfan : 1;
    // Byte 3
    uint8_t : 4;
    uint8_t : 2;  // (possibly timer related) (Typically 0b01)
    uint8_t : 2;  // End of command block (B01)
    // (B010 marker and a gap of 20ms)
    // Byte 4
    uint8_t swingv : 4;
    uint8_t swingh : 1;
    uint8_t : 3;
    // Byte 5~6
    uint8_t pad0[2];  // Timer related. Typically 0 except when timer in use.
    // Byte 7
    uint8_t : 4;            // (Used in Timer mode)
    uint8_t checksum1 : 4;  // checksum of the previous bytes (0-6)

    // Byte 8~10
    uint8_t pad1[3];  // Repeat of byte 0~2
    // Byte 11
    uint8_t : 4;
    uint8_t : 2;  // (possibly timer related) (Typically 0b11)
    uint8_t : 2;  // End of command block (B01)
    // (B010 marker and a gap of 20ms)
    // Byte 12
    uint8_t : 1;  // Sleep mode 2 (1 = On, 0=Off)
    uint8_t : 6;  // (Used in Sleep Mode 3, Typically 0b000000)
    uint8_t quiet : 1;
    // Byte 13
    uint8_t : 8;  // (Sleep Mode 3 related, Typically 0x00)
    // Byte 14
    uint8_t : 4;  // (Sleep Mode 3 related, Typically 0b0000)
    uint8_t fan : 3;
    // Byte 15
    uint8_t : 4;
    uint8_t checksum2 : 4;  // checksum of the previous bytes (8-14)
  };
};

// Constants
static const uint8_t KELVINATOR_MODE_AUTO = 0;  // (temp = 25C)
static const uint8_t KELVINATOR_MODE_COOL = 1;
static const uint8_t KELVINATOR_MODE_DRY = 2;  // (temp = 25C, but not shown)
static const uint8_t KELVINATOR_MODE_FAN = 3;
static const uint8_t KELVINATOR_MODE_HEAT = 4;

static const uint8_t KELVINATOR_BASICFAN_AUTO = 0;
static const uint8_t KELVINATOR_BASICFAN_MIN = 1;
static const uint8_t KELVINATOR_BASICFAN_MEDIUM = 2;
static const uint8_t KELVINATOR_BASICFAN_MAX = 3;

static const uint8_t KELVINATOR_TEMPERATURE_MIN = 16;
static const uint8_t KELVINATOR_TEMPERATURE_MAX = 30;
static const uint8_t KELVINATOR_TEMPERATURE_AUTO = 25;

static const uint8_t KELVINATOR_VSWING_OFF = 0b0000;   // 0
static const uint8_t KELVINATOR_VSWING_AUTO = 0b0001;  // 1
// static const uint8_t KELVINATOR_VSWING_HIGHEST     = 0b0010;  // 2
// static const uint8_t KELVINATOR_VSWING_UPPERMIDDLE = 0b0011;  // 3
// static const uint8_t KELVINATOR_VSWING_MIDDLE      = 0b0100;  // 4
// static const uint8_t KELVINATOR_VSWING_LOWERMIDDLE = 0b0101;  // 5
// static const uint8_t KELVINATOR_VSWING_LOWEST      = 0b0110;  // 6
// static const uint8_t KELVINATOR_VSWING_LOWAUTO     = 0b0111;  // 7
// static const uint8_t KELVINATOR_VSWING_MIDDLEAUTO  = 0b1001;  // 9
// static const uint8_t KELVINATOR_VSWING_HIGHAUTO    = 0b1011;  // 11

class KelvinatorIR : public climate_ir::ClimateIR {
 public:
  KelvinatorIR()
      : climate_ir::ClimateIR(KELVINATOR_TEMPERATURE_MIN, KELVINATOR_TEMPERATURE_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void set_light(bool enabled);

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t get_mode_();
  uint8_t get_temperature_();
  uint8_t get_power_();
  uint8_t get_basic_fan_();

  bool light_{true};
};

}  // namespace kelvinator_ir
}  // namespace esphome
