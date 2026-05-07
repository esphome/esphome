#pragma once

#include "esphome/core/log.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/aeha_protocol.h"

namespace esphome {
namespace friedrich {

// Simple enum to represent models.
// Stub for future develpment of other models, currently nothing depends upon Model.
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
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {
    this->remote_state_.resize(STATE_MESSAGE_LENGTH);
  }

  /// Set use of Fahrenheit units
  void set_fahrenheit(bool value) {
    this->fahrenheit_ = value;
    this->temperature_step_ = value ? 2.0f : 1.0f;
  }

  void set_model(Model model) { this->model_ = model; }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Transmit via IR power off command.
  void transmit_off_();

  /// Parse incoming message
  bool on_receive(remote_base::RemoteReceiveData src) override;

  /// Transmit message as IR pulses
  void transmit_(std::vector<uint8_t> *message);

  /// Calculate checksum for a state message
  uint8_t checksum_state_(std::vector<uint8_t> *message);

  /// Calculate checksum for a util message
  uint8_t checksum_util_(std::vector<uint8_t> *message);

  std::vector<uint8_t> remote_state_;

  // true if currently on - friedrich transmit an on flag on when the remote moves from off to on
  bool power_{false};
  bool fahrenheit_{true};
  Model model_;

  remote_base::AEHAData data_;
  remote_base::AEHAProtocol protocol_;
};

}  // namespace friedrich
}  // namespace esphome
