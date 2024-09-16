#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

/***********************************************************************************
 * SOURCE
 ***********************************************************************************
 * The IR codes and the functional description below were taken from
 * 'arduino-heatpumpir/ZHLT01HeatpumpIR.h' as can be found on GitHub
 * https://github.com/ToniA/arduino-heatpumpir/blob/master/ZHLT01HeatpumpIR.h
 *
 ************************************************************************************
 *  Airconditional remote control encoder for:
 *
 *  ZH/LT-01 Remote control https://www.google.com/search?q=zh/lt-01
 *
 *  The ZH/LT-01 remote control is used for many locally branded Split
 *  airconditioners, so it is better to name this protocol by the name of the
 *  REMOTE rather then the name of the Airconditioner. For this project I used
 *  a 2014 model Eurom-airconditioner, which is Dutch-branded and sold in
 *  the Netherlands at Hornbach.
 *
 * For airco-brands:
 *  Eurom
 *  Chigo
 *  Tristar
 *  Tecnomaster
 *  Elgin
 *  Geant
 *  Tekno
 *  Topair
 *  Proma
 *  Sumikura
 *  JBS
 *  Turbo Air
 *  Nakatomy
 *  Celestial Air
 *  Ager
 *  Blueway
 *  Airlux
 *  Etc.
 *
 ***********************************************************************************
 *  SUMMARY FUNCTIONAL DESCRIPTION
 ***********************************************************************************
 *  The remote sends a 12 Byte message which contains all possible settings every
 *  time.
 *
 *  Byte 11 (and 10) contain the remote control identifier and are always 0xD5 and
 *  0x2A respectively for the ZH/LT-01 remote control.
 *  Every UNeven Byte (01,03,05,07 and 09) holds command data
 *  Every EVEN Byte (00,02,04,06,08 and 10) holds a checksum of the corresponding
 *  command-, or identifier-byte by _inverting_ the bits, for example:
 *
 *  The identifier byte[11] = 0xD5 = B1101 0101
 *  The checksum byte[10]   = 0x2A = B0010 1010
 *
 *  So, you can check the message by:
 *  - inverting the bits of the checksum byte with the corresponding command-, or
 *    identifier byte, they should be the same, or
 *  - Summing up the checksum byte and the corresponding command-, or identifier byte,
 *    they should always add up to 0xFF = B11111111 = 255
 *
 *  Control bytes:
 *  [01] - Timer (1-24 hours, Off)
 *         Time is hardcoded to OFF
 *
 *  [03] - LAMP ON/OFF, TURBO ON/OFF, HOLD ON/OFF
 *         Lamp and Hold are hardcoded to OFF
 *         Turbo is used for the BOOST preset
 *
 *  [05] - Indicates which button the user _pressed_ on the remote control
 *         Hardcoded to POWER-button
 *
 *  [07] - POWER ON/OFF, FAN AUTO/3/2/1, SLEEP ON/OFF, AIRFLOW ON/OFF,
 *         VERTICAL SWING/WIND/FIXED
 *         SLEEP is used for preset SLEEP
 *         Vertical Swing supports Fixed, Swing and "Wind". The Wind option
 *         is ignored in this implementation
 *
 *  [09] - MODE AUTO/COOL/VENT/DRY/HEAT, TEMPERATURE (16 - 32°C)
 *
 ***********************************************************************************/

namespace esphome {
namespace zhlt01 {

/********************************************************************************
 *  TIMINGS
 *  Space:        Not used
 *  Header Mark:  6100 us
 *  Header Space: 7400 us
 *  Bit Mark:      500 us
 *  Zero Space:    600 us
 *  One Space:    1800 us
 *
 * Note : These timings are slightly different than those of ZHLT01HeatpumpIR
 *        The values below were measured by taking the average of 2 different
 *        remote controls each sending 10 commands
 *******************************************************************************/
static const uint32_t AC1_HDR_MARK = 6100;
static const uint32_t AC1_HDR_SPACE = 7400;
static const uint32_t AC1_BIT_MARK = 500;
static const uint32_t AC1_ZERO_SPACE = 600;
static const uint32_t AC1_ONE_SPACE = 1800;

/********************************************************************************
 *
 * ZHLT01 codes
 *
 *******************************************************************************/

// Power
static const uint8_t AC1_POWER_OFF = 0x00;
static const uint8_t AC1_POWER_ON = 0x02;

// Operating Modes
static const uint8_t AC1_MODE_AUTO = 0x00;
static const uint8_t AC1_MODE_COOL = 0x20;
static const uint8_t AC1_MODE_DRY = 0x40;
static const uint8_t AC1_MODE_FAN = 0x60;
static const uint8_t AC1_MODE_HEAT = 0x80;

// Fan control
static const uint8_t AC1_FAN_AUTO = 0x00;
static const uint8_t AC1_FAN_SILENT = 0x01;
static const uint8_t AC1_FAN1 = 0x60;
static const uint8_t AC1_FAN2 = 0x40;
static const uint8_t AC1_FAN3 = 0x20;
static const uint8_t AC1_FAN_TURBO = 0x08;

// Vertical Swing
static const uint8_t AC1_VDIR_WIND = 0x00;   // "Natural Wind", ignore
static const uint8_t AC1_VDIR_SWING = 0x04;  // Swing
static const uint8_t AC1_VDIR_FIXED = 0x08;  // Fixed

// Horizontal Swing
static const uint8_t AC1_HDIR_SWING = 0x00;  // Swing
static const uint8_t AC1_HDIR_FIXED = 0x10;  // Fixed

// Temperature range
static const float AC1_TEMP_MIN = 16.0f;
static const float AC1_TEMP_MAX = 32.0f;
static const float AC1_TEMP_INC = 1.0f;

class ZHLT01Climate : public climate_ir::ClimateIR {
 public:
  ZHLT01Climate()
      : climate_ir::ClimateIR(
            AC1_TEMP_MIN, AC1_TEMP_MAX, AC1_TEMP_INC, true, true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
             climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
             climate::CLIMATE_SWING_BOTH},
            {climate::CLIMATE_PRESET_NONE, climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_BOOST}) {}

  void setup() override { climate_ir::ClimateIR::setup(); }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
};

}  // namespace zhlt01
}  // namespace esphome
