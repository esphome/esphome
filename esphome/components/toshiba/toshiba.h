#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace toshiba {

// Enum for the different Toshiba remotes
enum Model {
  MODEL_GENERIC,
  MODEL_WH_TA01LE,
};

const uint8_t TOSHIBA_TEMP_MIN = 17;
const uint8_t TOSHIBA_TEMP_MAX = 30;

class ToshibaClimate : public climate_ir::ClimateIR {
 public:
  ToshibaClimate()
      : climate_ir::ClimateIR(TOSHIBA_TEMP_MIN, TOSHIBA_TEMP_MAX, 1.0f, true, true,
                              std::vector<climate::ClimateFanMode>{
                                  climate::CLIMATE_FAN_AUTO,
                                  climate::CLIMATE_FAN_LOW,
                                  climate::CLIMATE_FAN_MEDIUM,
                                  climate::CLIMATE_FAN_MIDDLE,
                                  climate::CLIMATE_FAN_DIFFUSE,
                                  climate::CLIMATE_FAN_HIGH,
                                  climate::CLIMATE_FAN_FOCUS,
                              },
                              std::vector<climate::ClimateSwingMode>{
                                  climate::CLIMATE_SWING_OFF,
                                  climate::CLIMATE_SWING_VERTICAL,
                              }) {}

  void set_model(Model model) { this->model_ = model; }

 protected:
  void setup() override;
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void add_default_command_data_(uint8_t* frame);
  void add_motion_command_data_(uint8_t* frame);
  void transmit_frame_(uint8_t* frame);

  climate::ClimateMode current_mode_{climate::CLIMATE_MODE_OFF};
  climate::ClimateFanMode current_fan_mode_{climate::CLIMATE_FAN_AUTO};
  climate::ClimateSwingMode current_swing_mode_{climate::CLIMATE_SWING_VERTICAL};
  uint8_t current_temperature_ = 22;
  Model model_;
};

} /* namespace toshiba */
} /* namespace esphome */
