#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace hitachi_168bit {

/// Models supported by the 168-bit Hitachi protocol.
enum Model {
  MODEL_HCRA31NEWH = 0,  // Temperature range 16–30 (adjust later if different)
};

// Temperature limits (constexpr + kCamelCase for clang-tidy)
constexpr float TEMP_MIN_HCRA31NEWH = 16.0f;
constexpr float TEMP_MAX_HCRA31NEWH = 30.0f;

class Hitachi168bitClimate : public climate_ir::ClimateIR {
 public:
  Hitachi168bitClimate()
      : climate_ir::ClimateIR(temperature_min_(),  // min temp
                              temperature_max_(),  // max temp
                              1.0f,                // temperature step
                              true,                // supports_cool
                              true,                // supports_heat
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void setup() override {
    climate_ir::ClimateIR::setup();
    this->powered_on_assumed = (this->mode != climate::CLIMATE_MODE_OFF);
  }

  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    climate_ir::ClimateIR::control(call);
  }

  void set_model(Model model) { this->model_ = model; }

  // Used to track when to send the power toggle command
  bool powered_on_assumed{false};

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;

  int32_t last_transmit_time_{};
  bool send_swing_cmd_{false};

  // Default to a safe model to avoid uninitialized reads
  Model model_{MODEL_HCRA31NEWH};

  float temperature_min_() const { return TEMP_MIN_HCRA31NEWH; }
  float temperature_max_() const { return TEMP_MAX_HCRA31NEWH; }
};

}  // namespace hitachi_168bit
}  // namespace esphome
