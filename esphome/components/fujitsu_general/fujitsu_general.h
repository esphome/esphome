#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace fujitsu_general {

// TODO 16 for heating, 18 for cooling, unsupported in ESPH
static constexpr uint8_t FUJITSU_GENERAL_TEMP_MIN = 16;  // Celsius

static constexpr uint8_t FUJITSU_GENERAL_TEMP_MAX = 30;  // Celsius

// Common header
static constexpr uint8_t FUJITSU_GENERAL_COMMON_LENGTH = 6;
static constexpr uint8_t FUJITSU_GENERAL_COMMON_BYTE0 = 0x14;
static constexpr uint8_t FUJITSU_GENERAL_COMMON_BYTE1 = 0x63;
static constexpr uint8_t FUJITSU_GENERAL_COMMON_BYTE2 = 0x00;
static constexpr uint8_t FUJITSU_GENERAL_COMMON_BYTE3 = 0x10;
static constexpr uint8_t FUJITSU_GENERAL_COMMON_BYTE4 = 0x10;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_BYTE = 5;

// State message - temp & fan etc.
static constexpr uint8_t FUJITSU_GENERAL_STATE_MESSAGE_LENGTH = 16;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_STATE = 0xFE;

// Util messages - off & eco etc.
static constexpr uint8_t FUJITSU_GENERAL_UTIL_MESSAGE_LENGTH = 7;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_OFF = 0x02;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY = 0x09;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_VERTICAL = 0x6C;
static constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_HORIZONTAL = 0x79;

// State header
static constexpr uint8_t FUJITSU_GENERAL_STATE_HEADER_BYTE0 = 0x09;
static constexpr uint8_t FUJITSU_GENERAL_STATE_HEADER_BYTE1 = 0x30;

// State footer
static constexpr uint8_t FUJITSU_GENERAL_STATE_FOOTER_BYTE0 = 0x20;

// Temperature
static constexpr uint8_t FUJITSU_GENERAL_TEMPERATURE_NIBBLE = 16;

// Power on
static constexpr uint8_t FUJITSU_GENERAL_POWER_ON_NIBBLE = 17;
static constexpr uint8_t FUJITSU_GENERAL_POWER_OFF = 0x00;
static constexpr uint8_t FUJITSU_GENERAL_POWER_ON = 0x01;

// Mode
static constexpr uint8_t FUJITSU_GENERAL_MODE_NIBBLE = 19;
static constexpr uint8_t FUJITSU_GENERAL_MODE_AUTO = 0x00;
static constexpr uint8_t FUJITSU_GENERAL_MODE_COOL = 0x01;
static constexpr uint8_t FUJITSU_GENERAL_MODE_DRY = 0x02;
static constexpr uint8_t FUJITSU_GENERAL_MODE_FAN = 0x03;
static constexpr uint8_t FUJITSU_GENERAL_MODE_HEAT = 0x04;
// static constexpr uint8_t FUJITSU_GENERAL_MODE_10C = 0x0B;

// Swing
static constexpr uint8_t FUJITSU_GENERAL_SWING_NIBBLE = 20;
static constexpr uint8_t FUJITSU_GENERAL_SWING_NONE = 0x00;
static constexpr uint8_t FUJITSU_GENERAL_SWING_VERTICAL = 0x01;
static constexpr uint8_t FUJITSU_GENERAL_SWING_HORIZONTAL = 0x02;
static constexpr uint8_t FUJITSU_GENERAL_SWING_BOTH = 0x03;

// Fan
static constexpr uint8_t FUJITSU_GENERAL_FAN_NIBBLE = 21;
static constexpr uint8_t FUJITSU_GENERAL_FAN_AUTO = 0x00;
static constexpr uint8_t FUJITSU_GENERAL_FAN_HIGH = 0x01;
static constexpr uint8_t FUJITSU_GENERAL_FAN_MEDIUM = 0x02;
static constexpr uint8_t FUJITSU_GENERAL_FAN_LOW = 0x03;
static constexpr uint8_t FUJITSU_GENERAL_FAN_SILENT = 0x04;

// TODO Outdoor Unit Low Noise
// static constexpr uint8_t FUJITSU_GENERAL_OUTDOOR_UNIT_LOW_NOISE_BYTE14 = 0xA0;
// static constexpr uint8_t FUJITSU_GENERAL_STATE_BYTE14 = 0x20;

static constexpr uint16_t FUJITSU_GENERAL_HEADER_MARK = 3300;
static constexpr uint16_t FUJITSU_GENERAL_HEADER_SPACE = 1600;

static constexpr uint16_t FUJITSU_GENERAL_BIT_MARK = 420;
static constexpr uint16_t FUJITSU_GENERAL_ONE_SPACE = 1200;
static constexpr uint16_t FUJITSU_GENERAL_ZERO_SPACE = 420;

static constexpr uint16_t FUJITSU_GENERAL_TRL_MARK = 420;
static constexpr uint16_t FUJITSU_GENERAL_TRL_SPACE = 8000;

static constexpr uint32_t FUJITSU_GENERAL_CARRIER_FREQUENCY = 38000;

// clang-format off
/**
 * ```
 *                                                                                               turn
 *                                                                                               on  temp mode     fan swing
 *                                                                                               *   |  | | |      | | *
 *
 * temperatures                                                                                  1   1248 124      124 1
 * auto auto 18        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000100 00000000 00000000 00000000  00000000 00000000 00000100 11110001
 * auto auto 19        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10001100 00000000 00000000 00000000  00000000 00000000 00000100 11111110
 * auto auto 30        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 00000000 00000000 00000000  00000000 00000000 00000100 11110011
 *
 * on flag:
 * on at 16            00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000000 00100000 00000000 00000000  00000000 00000000 00000100 11010101
 * down to 16          00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000000 00100000 00000000 00000000  00000000 00000000 00000100 00110101
 *
 * mode options:
 * auto auto 30        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 00000000 00000000 00000000  00000000 00000000 00000100 11110011
 * cool auto 30        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 10000000 00000000 00000000  00000000 00000000 00000100 01110011
 * dry auto 30         00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 01000000 00000000 00000000  00000000 00000000 00000100 10110011
 * fan (auto) (30)     00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 11000000 00000000 00000000  00000000 00000000 00000100 00110011
 * heat auto 30        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 00100000 00000000 00000000  00000000 00000000 00000100 11010011
 *
 * fan options:
 * heat 30 high        00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  10000111 00100000 10000000 00000000  00000000 00000000 00000100 01010011
 * heat 30 med         00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000111 00100000 01000000 00000000  00000000 00000000 00000100 01010011
 * heat 30 low         00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000111 00100000 11000000 00000000  00000000 00000000 00000100 10010011
 * heat 30 quiet       00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000111 00100000 00100000 00000000  00000000 00000000 00000100 00010011
 *
 * swing options:
 * heat 30 swing vert  00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000111 00100000 00101000 00000000  00000000 00000000 00000100 00011101
 * heat 30 noswing     00101000 11000110 00000000 00001000  00001000 01111111 10010000 00001100  00000111 00100000 00100000 00000000  00000000 00000000 00000100 00010011
 * ```
 */
// clang-format on

class FujitsuGeneralClimate : public climate_ir::ClimateIR {
 public:
  FujitsuGeneralClimate()
      : ClimateIR(FUJITSU_GENERAL_TEMP_MIN, FUJITSU_GENERAL_TEMP_MAX, 1.0f, true, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                   climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
                   climate::CLIMATE_SWING_BOTH}) {}

  /// Transmit via IR a util command.
  void transmit_util(uint8_t command);

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;

  /// Parse incoming message
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Transmit message as IR pulses
  void transmit_(uint8_t const *message, uint8_t length);

  /// Calculate checksum for a state message
  uint8_t checksum_state_(uint8_t const *message);

  /// Calculate cecksum for a util message
  uint8_t checksum_util_(uint8_t const *message);

  // true if currently on - fujitsus transmit an on flag on when the remote moves from off to on
  bool power_{false};
};

}  // namespace fujitsu_general
}  // namespace esphome
