#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::fujitsu_general {

constexpr uint8_t FUJITSU_GENERAL_TEMP_MIN = 16;  // Celsius // TODO 16 for heating, 18 for cooling, unsupported in ESPH
constexpr uint8_t FUJITSU_GENERAL_TEMP_MAX = 30;  // Celsius

// State header. The second byte is the "protocol version" in IRremoteESP8266, and it also selects
// how the temperature is encoded: see the Model enum below.
constexpr uint8_t FUJITSU_GENERAL_STATE_HEADER_BYTE0 = 0x09;
constexpr uint8_t FUJITSU_GENERAL_PROTOCOL_ARRAH2E = 0x30;
constexpr uint8_t FUJITSU_GENERAL_PROTOCOL_ARREW4E = 0x31;

/// Remote models differ in that protocol byte and in the temperature encoding.
enum Model : uint8_t {
  MODEL_ARRAH2E = 0,  /// 0x30 protocol byte, temperature in whole degrees
  MODEL_ARREW4E = 1,  /// 0x31 protocol byte, temperature in half degrees
};

// Stateless util commands the button platform sends; OFF is a util message too but changes the
// climate state, so it stays with transmit_state() in the .cpp
constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_ECONOMY = 0x09;
constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_VERTICAL = 0x6C;
constexpr uint8_t FUJITSU_GENERAL_MESSAGE_TYPE_NUDGE_HORIZONTAL = 0x79;

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
 * The column markers show which bits varied in these captures, not field widths.
 */
// clang-format on

// Bits are reversed within each byte, so an odd nibble index is the low half of its byte.
constexpr uint8_t get_nibble(const uint8_t *message, uint8_t nibble) {
  return (message[nibble / 2] >> ((nibble % 2) ? 0 : 4)) & 0b00001111;
}

/// Write a nibble into a zero-initialised frame.
constexpr void set_nibble(uint8_t *message, uint8_t nibble, uint8_t value) {
  message[nibble / 2] |= (value & 0b00001111) << ((nibble % 2) ? 0 : 4);
}

// Nibble indices of the state frame fields.
constexpr uint8_t FUJITSU_GENERAL_POWER_ON_NIBBLE = 17;
constexpr uint8_t FUJITSU_GENERAL_MODE_NIBBLE = 19;
constexpr uint8_t FUJITSU_GENERAL_SWING_NIBBLE = 20;
constexpr uint8_t FUJITSU_GENERAL_FAN_NIBBLE = 21;

// The temperature shares byte 8 with the power and Fahrenheit flags: bits 0 and 1 are those, bits
// 2-7 hold the temperature. Encoding the value in degrees differs between models.
constexpr uint8_t FUJITSU_GENERAL_TEMPERATURE_BYTE = 8;
constexpr uint8_t FUJITSU_GENERAL_TEMPERATURE_SHIFT = 2;
constexpr uint8_t FUJITSU_GENERAL_TEMPERATURE_MASK = 0b00111111;

constexpr uint8_t get_temperature_field(const uint8_t *message) {
  return (message[FUJITSU_GENERAL_TEMPERATURE_BYTE] >> FUJITSU_GENERAL_TEMPERATURE_SHIFT) &
         FUJITSU_GENERAL_TEMPERATURE_MASK;
}

constexpr void set_temperature_field(uint8_t *message, uint8_t value) {
  message[FUJITSU_GENERAL_TEMPERATURE_BYTE] =
      (message[FUJITSU_GENERAL_TEMPERATURE_BYTE] &
       ~(FUJITSU_GENERAL_TEMPERATURE_MASK << FUJITSU_GENERAL_TEMPERATURE_SHIFT)) |
      ((value & FUJITSU_GENERAL_TEMPERATURE_MASK) << FUJITSU_GENERAL_TEMPERATURE_SHIFT);
}

/// Field value for a target temperature in degrees. Mirrors setTemp() in IRremoteESP8266.
uint8_t encode_temperature(Model model, float temperature);

/// Target temperature in degrees for a field value. Mirrors getTemp() in IRremoteESP8266.
float decode_temperature(Model model, uint8_t field);

/// Unassigned values keep the current mode, except that OFF becomes HEAT_COOL.
climate::ClimateMode decode_mode(uint8_t mode_field, climate::ClimateMode current_mode);

/// Unassigned values keep the current fan mode.
optional<climate::ClimateFanMode> decode_fan_mode(uint8_t fan_field, optional<climate::ClimateFanMode> current_mode);

climate::ClimateSwingMode decode_swing_mode(uint8_t swing_field);

class FujitsuGeneralClimate final : public climate_ir::ClimateIR {
 public:
  FujitsuGeneralClimate()
      : ClimateIR(FUJITSU_GENERAL_TEMP_MIN, FUJITSU_GENERAL_TEMP_MAX, 1.0f, true, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                   climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET},
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
                   climate::CLIMATE_SWING_BOTH}) {}

  /// Transmit a util message that does not change the climate state (economy, vane steps)
  void transmit_util(uint8_t command);

  /// Select the remote model, which fixes the protocol byte and the temperature step
  void set_model(Model model) {
    this->model_ = model;
    this->temperature_step_ = model == MODEL_ARREW4E ? 0.5f : 1.0f;
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;

  /// Parse incoming message
  bool on_receive(remote_base::RemoteReceiveData data) override;

  /// Transmit message as IR pulses
  void transmit_(uint8_t const *message, uint8_t length);

  /// Calculate checksum for a state message
  uint8_t checksum_state_(uint8_t const *message);

  /// Calculate checksum for a util message
  uint8_t checksum_util_(uint8_t const *message);

  // true if currently on - fujitsus transmit an on flag on when the remote moves from off to on
  bool power_{false};

  Model model_{MODEL_ARRAH2E};
};

}  // namespace esphome::fujitsu_general
