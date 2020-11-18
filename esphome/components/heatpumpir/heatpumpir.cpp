#include <map>
#include "heatpumpir.h"
#include "esphome/core/log.h"
#include "ir_sender_esphome.h"

namespace esphome {
namespace heatpumpir {

static const char *TAG = "heatpumpir.climate";

std::map<Protocol, std::function<HeatpumpIR *()>> protocol_constructor_map = {
    {PROTOCOL_AUX, []() { return new AUXHeatpumpIR(); }},
    {PROTOCOL_BALLU, []() { return new BalluHeatpumpIR(); }},
    {PROTOCOL_CARRIER_MCA, []() { return new CarrierMCAHeatpumpIR(); }},
    {PROTOCOL_CARRIER_NQV, []() { return new CarrierNQVHeatpumpIR(); }},
    {PROTOCOL_DAIKIN_ARC417, []() { return new DaikinHeatpumpARC417IR(); }},
    {PROTOCOL_DAIKIN_ARC480, []() { return new DaikinHeatpumpARC480A14IR(); }},
    {PROTOCOL_DAIKIN, []() { return new DaikinHeatpumpIR(); }},
    {PROTOCOL_FUEGO, []() { return new FuegoHeatpumpIR(); }},
    {PROTOCOL_FUJITSU_AWYZ, []() { return new FujitsuHeatpumpIR(); }},
    {PROTOCOL_GREE, []() { return new GreeGenericHeatpumpIR(); }},
    {PROTOCOL_GREEYAA, []() { return new GreeYAAHeatpumpIR(); }},
    {PROTOCOL_GREEYAN, []() { return new GreeYANHeatpumpIR(); }},
    {PROTOCOL_HISENSE_AUD, []() { return new HisenseHeatpumpIR(); }},
    {PROTOCOL_HITACHI, []() { return new HitachiHeatpumpIR(); }},
    {PROTOCOL_HYUNDAI, []() { return new HyundaiHeatpumpIR(); }},
    {PROTOCOL_IVT, []() { return new IVTHeatpumpIR(); }},
    {PROTOCOL_MIDEA, []() { return new MideaHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_FA, []() { return new MitsubishiFAHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_FD, []() { return new MitsubishiFDHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_FE, []() { return new MitsubishiFEHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_HEAVY_FDTC, []() { return new MitsubishiHeavyFDTCHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_HEAVY_ZJ, []() { return new MitsubishiHeavyZJHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_HEAVY_ZM, []() { return new MitsubishiHeavyZMHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_HEAVY_ZMP, []() { return new MitsubishiHeavyZMPHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_KJ, []() { return new MitsubishiKJHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_MSC, []() { return new MitsubishiMSCHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_MSY, []() { return new MitsubishiMSYHeatpumpIR(); }},
    {PROTOCOL_MITSUBISHI_SEZ, []() { return new MitsubishiSEZKDXXHeatpumpIR(); }},
    {PROTOCOL_PANASONIC_CKP, []() { return new PanasonicCKPHeatpumpIR(); }},
    {PROTOCOL_PANASONIC_DKE, []() { return new PanasonicDKEHeatpumpIR(); }},
    {PROTOCOL_PANASONIC_JKE, []() { return new PanasonicJKEHeatpumpIR(); }},
    {PROTOCOL_PANASONIC_LKE, []() { return new PanasonicLKEHeatpumpIR(); }},
    {PROTOCOL_PANASONIC_NKE, []() { return new PanasonicNKEHeatpumpIR(); }},
    {PROTOCOL_SAMSUNG_AQV, []() { return new SamsungAQVHeatpumpIR(); }},
    {PROTOCOL_SAMSUNG_FJM, []() { return new SamsungFJMHeatpumpIR(); }},
    {PROTOCOL_SHARP, []() { return new SharpHeatpumpIR(); }},
    {PROTOCOL_TOSHIBA_DAISEIKAI, []() { return new ToshibaDaiseikaiHeatpumpIR(); }},
    {PROTOCOL_TOSHIBA, []() { return new ToshibaHeatpumpIR(); }},
};

void HeatpumpIRClimate::setup() {
  auto protocol_constructor = protocol_constructor_map.find(protocol_);
  if (protocol_constructor == protocol_constructor_map.end()) {
    ESP_LOGE(TAG, "Invalid protocol");
    return;
  }
  this->heatpump_ir_ = protocol_constructor->second();
  climate_ir::ClimateIR::setup();
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

  switch (this->fan_mode) {
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

  IRSenderESPHome esp_sender(0, this->transmitter_);

  heatpump_ir_->send(esp_sender, power_mode_cmd, operating_mode_cmd, fan_speed_cmd, temperature_cmd, swing_v_cmd,
                     swing_h_cmd);
}

}  // namespace heatpumpir
}  // namespace esphome
