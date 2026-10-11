#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/toshiba_ac_protocol.h"

namespace esphome::toshiba {

// Simple enum to represent models.
enum Model {
  MODEL_GENERIC = 0,           // Temperature range is from 17 to 30
  MODEL_RAC_PT1411HWRU_C = 1,  // Temperature range is from 16 to 30
  MODEL_RAC_PT1411HWRU_F = 2,  // Temperature range is from 16 to 30
  MODEL_RAS_2819T = 3,         // RAS-2819T protocol variant, temperature range 18 to 30
  MODEL_SEIYA = 4,             // Seiya family (RAS-B13E2KVG-E etc.), generic frame with a swing byte
  MODEL_RAS_B10N3KV2 = 5,      // RAS-B13 remote family, generic frame with IRremoteESP8266's TOSHIBA_AC timings
};

// Supported temperature ranges
const float TOSHIBA_GENERIC_TEMP_C_MIN = 17.0;
const float TOSHIBA_GENERIC_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN = 16.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX = 30.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MIN = 60.0;
const float TOSHIBA_RAC_PT1411HWRU_TEMP_F_MAX = 86.0;
const float TOSHIBA_RAS_2819T_TEMP_C_MIN = 18.0;
const float TOSHIBA_RAS_2819T_TEMP_C_MAX = 30.0;

class ToshibaClimate final : public climate_ir::ClimateIR {
 public:
  ToshibaClimate()
      : climate_ir::ClimateIR(TOSHIBA_GENERIC_TEMP_C_MIN, TOSHIBA_GENERIC_TEMP_C_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_QUIET}) {}

  void setup() override;
  void set_model(Model model) { this->model_ = model; }

 protected:
  void transmit_state() override;
  void transmit_generic_();
  void transmit_rac_pt1411hwru_();
  void transmit_rac_pt1411hwru_temp_(bool cs_state = true, bool cs_send_update = true);
  void transmit_ras_2819t_();
  // Process RAS-2819T IR command data
  bool process_ras_2819t_command_(const remote_base::ToshibaAcData &toshiba_data);
  // Returns the header if valid, else returns zero
  uint8_t is_valid_rac_pt1411hwru_header_(const uint8_t *message);
  // Returns true if message is a valid RAC-PT1411HWRU IR message, regardless if first or second packet
  bool is_valid_rac_pt1411hwru_message_(const uint8_t *message);
  // Returns true if message1 and message 2 are the same
  bool compare_rac_pt1411hwru_packets_(const uint8_t *message1, const uint8_t *message2);
  bool on_receive(remote_base::RemoteReceiveData data) override;

 private:
  // Last swing mode sent or received, for models whose swing is a one-shot command
  climate::ClimateSwingMode last_swing_mode_{climate::CLIMATE_SWING_OFF};
  climate::ClimateMode last_mode_{climate::CLIMATE_MODE_OFF};
  optional<climate::ClimateFanMode> last_fan_mode_{};
  float last_target_temperature_{24.0f};

  float temperature_min_() {
    if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F)
      return TOSHIBA_RAC_PT1411HWRU_TEMP_C_MIN;
    if (this->model_ == MODEL_RAS_2819T)
      return TOSHIBA_RAS_2819T_TEMP_C_MIN;
    return TOSHIBA_GENERIC_TEMP_C_MIN;  // Default to GENERIC for unknown models
  }
  float temperature_max_() {
    if (this->model_ == MODEL_RAC_PT1411HWRU_C || this->model_ == MODEL_RAC_PT1411HWRU_F)
      return TOSHIBA_RAC_PT1411HWRU_TEMP_C_MAX;
    if (this->model_ == MODEL_RAS_2819T)
      return TOSHIBA_RAS_2819T_TEMP_C_MAX;
    return TOSHIBA_GENERIC_TEMP_C_MAX;  // Default to GENERIC for unknown models
  }
  climate::ClimateSwingModeMask toshiba_swing_modes_() {
    if (this->model_ == MODEL_GENERIC || this->model_ == MODEL_RAS_B10N3KV2)
      return climate::ClimateSwingModeMask();
    if (this->model_ == MODEL_SEIYA) {
      // No captured code stops the swing, so OFF is not offered; the remote only selects a direction
      return climate::ClimateSwingModeMask{climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
                                           climate::CLIMATE_SWING_BOTH};
    }
    return climate::ClimateSwingModeMask{climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL};
  }
  void encode_(remote_base::RemoteTransmitData *data, const uint8_t *message, uint8_t nbytes, uint8_t repeat);
  bool decode_(remote_base::RemoteReceiveData *data, uint8_t *message, uint8_t nbytes);

  // Shared by every model that uses the F2 0D frame
  uint8_t encode_mode_fan_() const;
  void decode_mode_fan_temperature_(const uint8_t *message);
  uint8_t seiya_swing_code_() const;
  void seiya_decode_swing_(uint8_t code);

  Model model_;
};

}  // namespace esphome::toshiba
