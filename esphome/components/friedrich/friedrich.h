#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::friedrich {

// Simple enum to represent models.
// Stub for future development of other models, currently nothing depends upon Model.
enum Model {
  MODEL_MW12Y3H = 0,  // MW12Y3H built from a remote that only provided Fahrenheit
};

// The unit reports temperatures in Fahrenheit (see traits()); the unit only accepts even values.
const uint8_t TEMP_MIN = 60;  // actually 64 for anything but heating
const uint8_t TEMP_MAX = 88;
const uint8_t TEMP_MIN_NOT_HEAT = 64;
const uint8_t TEMP_STEP = 2;
const uint8_t TEMP_DEFAULT = 72;

const uint8_t STATE_MESSAGE_LENGTH = 14;

class FriedrichClimate : public climate_ir::ClimateIR {
 public:
  FriedrichClimate()
      : ClimateIR(TEMP_MIN, TEMP_MAX, TEMP_STEP, true, true,
                  {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_MEDIUM,
                   climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_QUIET},
                  {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void set_model(Model model) { this->model_ = model; }

 protected:
  void setup() override;
  void dump_config() override;
  climate::ClimateTraits traits() override;
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

  Model model_;
};

}  // namespace esphome::friedrich
