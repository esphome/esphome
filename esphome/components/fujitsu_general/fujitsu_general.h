#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace fujitsu_general {

const uint8_t FUJITSU_GENERAL_TEMP_MIN = 16;  // Celsius // TODO 16 for heating, 18 for cooling, unsupported in ESPH
const uint8_t FUJITSU_GENERAL_TEMP_MAX = 30;  // Celsius

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

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Transmit via IR power off command.
  void transmit_off_();

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
