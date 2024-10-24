#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

/***********************************************************************************
 * SOURCE
 ***********************************************************************************
 * The IR codes and the functional description below were taken from
 * "Reverse engineering the YK-H/531E AC remote control IR protocol"
 * https://blog.spans.fi/2024/04/16/reverse-engineering-the-yk-h531e-ac-remote-control-ir-protocol.html
 * As well as IRremoteESP8266 library
 * https://github.com/crankyoldgit/IRremoteESP8266/blob/master/src/ir_Electra.h
 *
 ************************************************************************************
 *  Air condition remote control encoder for:
 *
 *  YK-H/531E Remote control https://www.google.com/search?q=yk-h/531e+remote+control
 *
 *  The YK-H/531E remote control is used for different airconditioner brands.
 *
 * Tested on:
 *  Electrolux Chillflex EXP26U338CW
 ***********************************************************************************/

namespace esphome {
namespace ykh531e {

/********************************************************************************
 *  Timings in microseconds
 *******************************************************************************/
static const uint32_t HEADER_MARK = 9100;
static const uint32_t HEADER_SPACE = 4500;
static const uint32_t BIT_MARK = 600;
static const uint32_t ZERO_SPACE = 600;
static const uint32_t ONE_SPACE = 1700;

/********************************************************************************
 *
 * YK-H/531E codes
 *
 *******************************************************************************/
static const uint8_t FAN_SPEED_LOW = 0b011;
static const uint8_t FAN_SPEED_MID = 0b010;
static const uint8_t FAN_SPEED_HIGH = 0b001;
static const uint8_t FAN_SPEED_AUTO = 0b101;

static const uint8_t SWING_ON = 0b000;
static const uint8_t SWING_OFF = 0b111;

static const uint8_t MODE_AUTO = 0b000;
static const uint8_t MODE_COOL = 0b001;
static const uint8_t MODE_DRY = 0b010;
static const uint8_t MODE_FAN = 0b110;

// Temperature range
static const float TEMP_MIN = 16.0f;
static const float TEMP_MAX = 32.0f;
static const float TEMP_INC = 1.0f;

class YKH531EClimate : public climate_ir::ClimateIR {
 public:
  YKH531EClimate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX, TEMP_INC, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL},
                              {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP}) {}

  void setup() override { climate_ir::ClimateIR::setup(); }

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace ykh531e
}  // namespace esphome
