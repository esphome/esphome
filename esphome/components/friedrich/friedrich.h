#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::friedrich {

// Extension point for other units: add new models here and branch on model_ where they differ.
// The default must stay MW12Y3H so existing configurations do not change.
enum class Model : uint8_t {
  MODEL_MW12Y3H,  // MW12Y3H built from a remote that only provided Fahrenheit
};

class FriedrichClimate : public climate_ir::ClimateIR {
 public:
  // The unit reports temperatures in Fahrenheit (see traits()); the unit only accepts even values.
  static constexpr uint8_t TEMP_MIN = 60;  // actually 64 for anything but heating
  static constexpr uint8_t TEMP_MAX = 88;
  static constexpr uint8_t TEMP_STEP = 2;
  static constexpr uint8_t STATE_MESSAGE_LENGTH = 14;

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

  Model model_{Model::MODEL_MW12Y3H};
};

}  // namespace esphome::friedrich
