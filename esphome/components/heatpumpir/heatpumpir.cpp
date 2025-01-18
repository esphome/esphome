#include "heatpumpir.h"

#ifdef USE_ARDUINO

#include <map>
#include "ir_sender_esphome.h"
#include "HeatpumpIRFactory.h"
#include "esphome/core/log.h"

namespace esphome {
namespace heatpumpir {

static const char *const TAG = "heatpumpir.climate";

const std::map<Protocol, std::function<HeatpumpIR *()>> PROTOCOL_CONSTRUCTOR_MAP = {
    {PROTOCOL_AUX, []() { return new AUXHeatpumpIR(); }},                                    // NOLINT
    {PROTOCOL_BALLU, []() { return new BalluHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_CARRIER_MCA, []() { return new CarrierMCAHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_CARRIER_NQV, []() { return new CarrierNQVHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_DAIKIN_ARC417, []() { return new DaikinHeatpumpARC417IR(); }},                 // NOLINT
    {PROTOCOL_DAIKIN_ARC480, []() { return new DaikinHeatpumpARC480A14IR(); }},              // NOLINT
    {PROTOCOL_DAIKIN, []() { return new DaikinHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_ELECTROLUXYAL, []() { return new ElectroluxYALHeatpumpIR(); }},                // NOLINT
    {PROTOCOL_FUEGO, []() { return new FuegoHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_FUJITSU_AWYZ, []() { return new FujitsuHeatpumpIR(); }},                       // NOLINT
    {PROTOCOL_GREE, []() { return new GreeGenericHeatpumpIR(); }},                           // NOLINT
    {PROTOCOL_GREEYAA, []() { return new GreeYAAHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYAN, []() { return new GreeYANHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYAC, []() { return new GreeYACHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_GREEYT, []() { return new GreeYTHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_GREEYAP, []() { return new GreeYAPHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_HISENSE_AUD, []() { return new HisenseHeatpumpIR(); }},                        // NOLINT
    {PROTOCOL_HITACHI, []() { return new HitachiHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_HYUNDAI, []() { return new HyundaiHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_IVT, []() { return new IVTHeatpumpIR(); }},                                    // NOLINT
    {PROTOCOL_MIDEA, []() { return new MideaHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_MITSUBISHI_FA, []() { return new MitsubishiFAHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_FD, []() { return new MitsubishiFDHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_FE, []() { return new MitsubishiFEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_FDTC, []() { return new MitsubishiHeavyFDTCHeatpumpIR(); }},  // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZJ, []() { return new MitsubishiHeavyZJHeatpumpIR(); }},      // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZM, []() { return new MitsubishiHeavyZMHeatpumpIR(); }},      // NOLINT
    {PROTOCOL_MITSUBISHI_HEAVY_ZMP, []() { return new MitsubishiHeavyZMPHeatpumpIR(); }},    // NOLINT
    {PROTOCOL_MITSUBISHI_KJ, []() { return new MitsubishiKJHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_MITSUBISHI_MSC, []() { return new MitsubishiMSCHeatpumpIR(); }},               // NOLINT
    {PROTOCOL_MITSUBISHI_MSY, []() { return new MitsubishiMSYHeatpumpIR(); }},               // NOLINT
    {PROTOCOL_MITSUBISHI_SEZ, []() { return new MitsubishiSEZKDXXHeatpumpIR(); }},           // NOLINT
    {PROTOCOL_PANASONIC_CKP, []() { return new PanasonicCKPHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_DKE, []() { return new PanasonicDKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_JKE, []() { return new PanasonicJKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_LKE, []() { return new PanasonicLKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_PANASONIC_NKE, []() { return new PanasonicNKEHeatpumpIR(); }},                 // NOLINT
    {PROTOCOL_SAMSUNG_AQV, []() { return new SamsungAQVHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_SAMSUNG_FJM, []() { return new SamsungFJMHeatpumpIR(); }},                     // NOLINT
    {PROTOCOL_SHARP, []() { return new SharpHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_TOSHIBA_DAISEIKAI, []() { return new ToshibaDaiseikaiHeatpumpIR(); }},         // NOLINT
    {PROTOCOL_TOSHIBA, []() { return new ToshibaHeatpumpIR(); }},                            // NOLINT
    {PROTOCOL_ZHLT01, []() { return new ZHLT01HeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_NIBE, []() { return new NibeHeatpumpIR(); }},                                  // NOLINT
    {PROTOCOL_QLIMA_1, []() { return new Qlima1HeatpumpIR(); }},                             // NOLINT
    {PROTOCOL_QLIMA_2, []() { return new Qlima2HeatpumpIR(); }},                             // NOLINT
    {PROTOCOL_SAMSUNG_AQV12MSAN, []() { return new SamsungAQV12MSANHeatpumpIR(); }},         // NOLINT
    {PROTOCOL_ZHJG01, []() { return new ZHJG01HeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_AIRWAY, []() { return new AIRWAYHeatpumpIR(); }},                              // NOLINT
    {PROTOCOL_BGH_AUD, []() { return new BGHHeatpumpIR(); }},                                // NOLINT
    {PROTOCOL_PANASONIC_ALTDKE, []() { return new PanasonicAltDKEHeatpumpIR(); }},           // NOLINT
    {PROTOCOL_VAILLANTVAI8, []() { return new VaillantHeatpumpIR(); }},                      // NOLINT
    {PROTOCOL_R51M, []() { return new R51MHeatpumpIR(); }},                                  // NOLINT
};

void HeatpumpIRClimate::setup() {
  auto protocol_constructor = PROTOCOL_CONSTRUCTOR_MAP.find(protocol_);
  if (protocol_constructor == PROTOCOL_CONSTRUCTOR_MAP.end()) {
    ESP_LOGE(TAG, "Invalid protocol");
    return;
  }
  this->heatpump_ir_ = protocol_constructor->second();
  climate_ir::ClimateIR::setup();
  if (this->sensor_) {
    this->sensor_->add_on_state_callback([this](float state) {
      this->current_temperature = state;

      IRSenderESPHome esp_sender(this->transmitter_);
      this->heatpump_ir_->send(esp_sender, uint8_t(lround(this->current_temperature + 0.5)));

      // current temperature changed, publish state
      this->publish_state();
    });
    this->current_temperature = this->sensor_->state;
  } else {
    this->current_temperature = NAN;
  }
}

void HeatpumpIRClimate::transmit_state() {
  uint8_t power_mode_cmd;
  uint8_t operating_mode_cmd;
  uint8_t temperature_cmd;
  uint8_t fan_speed_cmd;

  uint8_t swing_v_cmd;
  switch (default_vertical_direction_) {
    case VERTICAL_DIRECTION_AUTO:
      swing_v_cmd = VDIR_AUTO;
      break;
    case VERTICAL_DIRECTION_UP:
      swing_v_cmd = VDIR_UP;
      break;
    case VERTICAL_DIRECTION_MUP:
      swing_v_cmd = VDIR_MUP;
      break;
    case VERTICAL_DIRECTION_MIDDLE:
      swing_v_cmd = VDIR_MIDDLE;
      break;
    case VERTICAL_DIRECTION_MDOWN:
      swing_v_cmd = VDIR_MDOWN;
      break;
    case VERTICAL_DIRECTION_DOWN:
      swing_v_cmd = VDIR_DOWN;
      break;
    default:
      ESP_LOGE(TAG, "Invalid default vertical direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swing_v_cmd = VDIR_SWING;
  }

  uint8_t swing_h_cmd;
  switch (default_horizontal_direction_) {
    case HORIZONTAL_DIRECTION_AUTO:
      swing_h_cmd = HDIR_AUTO;
      break;
    case HORIZONTAL_DIRECTION_MIDDLE:
      swing_h_cmd = HDIR_MIDDLE;
      break;
    case HORIZONTAL_DIRECTION_LEFT:
      swing_h_cmd = HDIR_LEFT;
      break;
    case HORIZONTAL_DIRECTION_MLEFT:
      swing_h_cmd = HDIR_MLEFT;
      break;
    case HORIZONTAL_DIRECTION_MRIGHT:
      swing_h_cmd = HDIR_MRIGHT;
      break;
    case HORIZONTAL_DIRECTION_RIGHT:
      swing_h_cmd = HDIR_RIGHT;
      break;
    default:
      ESP_LOGE(TAG, "Invalid default horizontal direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swing_h_cmd = HDIR_SWING;
  }

  switch (this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO)) {
    case climate::CLIMATE_FAN_LOW:
      fan_speed_cmd = FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fan_speed_cmd = FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fan_speed_cmd = FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fan_speed_cmd = FAN_AUTO;
      break;
  }

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_AUTO:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_FAN;
      break;
    case climate::CLIMATE_MODE_DRY:
      power_mode_cmd = POWER_ON;
      operating_mode_cmd = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      power_mode_cmd = POWER_OFF;
      operating_mode_cmd = MODE_AUTO;
      break;
  }

  temperature_cmd = (uint8_t) clamp(this->target_temperature, this->min_temperature_, this->max_temperature_);

  IRSenderESPHome esp_sender(this->transmitter_);
  heatpump_ir_->send(esp_sender, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd,
                     swing_h_cmd);
}

}  // namespace heatpumpir
}  // namespace esphome

#endif
