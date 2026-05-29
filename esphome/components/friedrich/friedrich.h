#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace friedrich {

// Simple enum to represent models.
// Stub for future development of other models, currently nothing depends upon Model.
enum Model {
  MODEL_MW12Y3H = 0,  // MW12Y3H built from a remote that only provided Fahrenheit
};

const uint8_t TEMP_MIN = 60;  // F, actually 64 For anything but Heating
const uint8_t TEMP_MAX = 88;  // F

const uint8_t STATE_MESSAGE_LENGTH = 14;

class FriedrichClimate : public climate_ir::ClimateIR {
 public:
  FriedrichClimate()
      : ClimateIR(roundf(fahrenheit_to_celsius(TEMP_MIN)), roundf(fahrenheit_to_celsius(TEMP_MAX)), 1.0f, true, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_MEDIUM,
                   climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_QUIET},
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  // Retained for future Celsius support; use_fahrenheit: false is currently rejected at config validation.
  void set_fahrenheit(bool value) {
    this->fahrenheit_ = value;
    this->temperature_step_ = value ? 2.0f : 1.0f;
  }

  void set_model(Model model) { this->model_ = model; }

 protected:
  void dump_config() override;
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Transmit via IR power off command.
  void transmit_off_();

  /// Parse incoming message
  bool on_receive(remote_base::RemoteReceiveData src) override;

  /// Transmit message as IR pulses
  void transmit_(const uint8_t *data, uint8_t len);

  /// Calculate checksum for a state message
  uint8_t checksum_state_(const uint8_t *data);

  /// Calculate checksum for a util message
  uint8_t checksum_util_(const uint8_t *data);

  // true if currently on - friedrich transmit an on flag on when the remote moves from off to on
  bool power_{false};
  bool fahrenheit_{true};
  Model model_;
};

}  // namespace friedrich
}  // namespace esphome
