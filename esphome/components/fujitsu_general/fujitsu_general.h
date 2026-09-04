#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::fujitsu_general {

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
 *
 * The markers above the columns record which bits varied across these captures, not how wide each
 * field is. Swing is marked with a single bit but is two bits wide, and the weight eight slot over
 * the mode is blank only because no capture was taken with the feature that bit belongs to active.
 * The field widths come from the protocol, not from this table.
 */
// clang-format on

// A byte's bits are reversed for Fujitsu, so nibbles are ordered 1, 0, 3, 2, 5, 4, and so on: an
// odd index is a byte's low half and an even one its high half.
constexpr uint8_t get_nibble(const uint8_t *message, uint8_t nibble) {
  return (message[nibble / 2] >> ((nibble % 2) ? 0 : 4)) & 0b00001111;
}

/// Write a nibble into a zero-initialised frame.
inline void set_nibble(uint8_t *message, uint8_t nibble, uint8_t value) {
  message[nibble / 2] |= (value & 0b00001111) << ((nibble % 2) ? 0 : 4);
}

// Where each field of a state frame lives, as nibble indices into the frame above.
const uint8_t FUJITSU_GENERAL_TEMPERATURE_NIBBLE = 16;
const uint8_t FUJITSU_GENERAL_POWER_ON_NIBBLE = 17;
const uint8_t FUJITSU_GENERAL_MODE_NIBBLE = 19;
const uint8_t FUJITSU_GENERAL_SWING_NIBBLE = 20;
const uint8_t FUJITSU_GENERAL_FAN_NIBBLE = 21;

/// Turn the mode field of a received frame into a climate mode.
///
/// Only the low three bits carry the mode; the fourth bit belongs to the clean feature and is
/// ignored here. Values the protocol does not assign leave the mode as it was, except that a state
/// frame never describes a unit that is off, so an off current mode becomes automatic.
climate::ClimateMode decode_mode(uint8_t mode_field, climate::ClimateMode current_mode);

/// Turn the fan speed field of a received frame into a fan mode.
///
/// Like the mode, only the low three bits carry the value. Speeds the protocol does not assign
/// leave the fan mode as it was.
optional<climate::ClimateFanMode> decode_fan_mode(uint8_t fan_field, optional<climate::ClimateFanMode> current_mode);

/// Turn the swing field of a received frame into a swing mode.
///
/// Only the low two bits carry the swing setting; the two above them are reserved. The protocol
/// assigns all four values, so every field value maps to a swing mode.
climate::ClimateSwingMode decode_swing_mode(uint8_t swing_field);

class FujitsuGeneralClimate final : public climate_ir::ClimateIR {
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

}  // namespace esphome::fujitsu_general
