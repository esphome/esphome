#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace toshiba {

// Simple enum to represent models.
enum Model {
  MODEL_GENERIC = 0,           // Temperature range is from 17 to 30
  MODEL_RAC_PT1411HWRU_C = 1,  // Temperature range is from 16 to 30
  MODEL_RAC_PT1411HWRU_F = 2,  // Temperature range is from 16 to 30
};

// Supported temperature ranges
const float TOSHIBA_GENERIC_TEMP_C_MIN = 17.0;
const float TOSHIBA_GENERIC_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN = 16.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN = 60.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MAX = 86.0;

class ToshibaClimate : public climate_ir::ClimateIR {
 public:
  ToshibaClimate() : climate_ir::ClimateIR(TOSHIBA_GENERIC_TEMP_C_MIN, TOSHIBA_GENERIC_TEMP_C_MAX, 1.0f) {}

  void setup() override;
  void set_model(Model model) { this->model_ = model; }

 protected:
  void transmit_state() override;
  void transmit_generic_();
  void transmit_rac_pt1411hwru_();
  void transmit_rac_pt1411hwru_temp_(bool cs_state = true, bool cs_send_update = true);
  // Returns the header if valid, else returns zero
  uint8_t is_valid_rac_pt1411hwru_header_(const uint8_t *message);
  // Returns true if message is a valid RAC-PT1411HWRU IR message, regardless if first or second packet
  bool is_valid_rac_pt1411hwru_message_(const uint8_t *message);
  // Returns true if message1 and message 2 are the same
  bool compare_rac_pt1411hwru_packets_(const uint8_t *message1, const uint8_t *message2);
  bool on_receive(remote_base::RemoteReceiveData data) override;

  float temperature_min_() {
    return (this->model_ == MODEL_GENERIC) ? TOSHIBA_GENERIC_TEMP_C_MIN : TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
  }
  float temperature_max_() {
    return (this->model_ == MODEL_GENERIC) ? TOSHIBA_GENERIC_TEMP_C_MAX : TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX;
  }
  bool toshiba_supports_dry_() {
    return ((this->model_ == MODEL_RAC_PT1411HWRU_C) || (this->model_ == MODEL_RAC_PT1411HWRU_F));
  }
  bool toshiba_supports_fan_only_() {
    return ((this->model_ == MODEL_RAC_PT1411HWRU_C) || (this->model_ == MODEL_RAC_PT1411HWRU_F));
  }
  std::set<climate::ClimateFanMode> toshiba_fan_modes_() {
    return (this->model_ == MODEL_GENERIC)
               ? std::set<climate::ClimateFanMode>{}
               : std::set<climate::ClimateFanMode>{climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
                                                   climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH};
  }
  std::set<climate::ClimateSwingMode> toshiba_swing_modes_() {
    return (this->model_ == MODEL_GENERIC)
               ? std::set<climate::ClimateSwingMode>{}
               : std::set<climate::ClimateSwingMode>{climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL};
  }
  void encode_(remote_base::RemoteTransmitData *data, const uint8_t *message, uint8_t nbytes, uint8_t repeat);
  bool decode_(remote_base::RemoteReceiveData *data, uint8_t *message, uint8_t nbytes);

  Model model_;
};

}  // namespace toshiba
}  // namespace esphome
