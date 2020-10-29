#include <map>
#include "heatpumpir.h"
#include "esphome/core/log.h"
#include "ir_sender_esphome.h"
#include <HeatpumpIRFactory.h>

namespace esphome {
namespace heatpumpir {

static const char *TAG = "heatpumpir.climate";

std::map<Protocol, std::function<HeatpumpIR*()>> protocol_constructor_map = {
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

void HeatpumpIRClimate::transmit_state() {
  uint8_t powerModeCmd;
  uint8_t operatingModeCmd;
  uint8_t temperatureCmd;
  uint8_t fanSpeedCmd;

  uint8_t swingVCmd;
  switch (default_vertical_direction_) {
    case VERTICAL_DIRECTION_AUTO:
      swingVCmd = VDIR_AUTO;
      break; 
    case VERTICAL_DIRECTION_UP:
      swingVCmd = VDIR_UP;
      break; 
    case VERTICAL_DIRECTION_MUP:
      swingVCmd = VDIR_MUP;
      break; 
    case VERTICAL_DIRECTION_MIDDLE:
      swingVCmd = VDIR_MIDDLE;
      break; 
    case VERTICAL_DIRECTION_MDOWN:
      swingVCmd = VDIR_MDOWN;
      break; 
    case VERTICAL_DIRECTION_DOWN:
      swingVCmd = VDIR_DOWN;
      break; 
    default:
      ESP_LOGE(TAG, "Invalid default vertical direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_VERTICAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swingVCmd = VDIR_SWING;
  }

  uint8_t swingHCmd;
  switch (default_horizontal_direction_) {
    case HORIZONTAL_DIRECTION_AUTO:
      swingHCmd = HDIR_AUTO;
      break; 
    case HORIZONTAL_DIRECTION_MIDDLE:
      swingHCmd = HDIR_MIDDLE;
      break; 
    case HORIZONTAL_DIRECTION_LEFT:
      swingHCmd = HDIR_LEFT;
      break; 
    case HORIZONTAL_DIRECTION_MLEFT:
      swingHCmd = HDIR_MLEFT;
      break; 
    case HORIZONTAL_DIRECTION_MRIGHT:
      swingHCmd = HDIR_MRIGHT;
      break; 
    case HORIZONTAL_DIRECTION_RIGHT:
      swingHCmd = HDIR_RIGHT;
      break; 
    default:
      ESP_LOGE(TAG, "Invalid default horizontal direction");
      return;
  }
  if ((this->swing_mode == climate::CLIMATE_SWING_HORIZONTAL) || (this->swing_mode == climate::CLIMATE_SWING_BOTH)) {
    swingHCmd = HDIR_SWING;
  }

  switch (this->fan_mode) {
    case climate::CLIMATE_FAN_LOW:
      fanSpeedCmd = FAN_2;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      fanSpeedCmd = FAN_3;
      break;
    case climate::CLIMATE_FAN_HIGH:
      fanSpeedCmd = FAN_4;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      fanSpeedCmd = FAN_AUTO;
      break;
  }

 switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      powerModeCmd = POWER_ON;
      operatingModeCmd = MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      powerModeCmd = POWER_ON;
      operatingModeCmd = MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_AUTO:
      powerModeCmd = POWER_ON;
      operatingModeCmd = MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      powerModeCmd = POWER_ON;
      operatingModeCmd = MODE_FAN;
      break;
    case climate::CLIMATE_ACTION_DRYING:
      powerModeCmd = POWER_ON;
      operatingModeCmd = MODE_DRY;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      powerModeCmd = POWER_OFF;
      operatingModeCmd = MODE_AUTO;
      break;
  }

  temperatureCmd = (uint8_t) clamp(this->target_temperature, this->min_temperature_, this->max_temperature_);

  // TODO: Is this a memory leak?
  IRSenderESPHome espSender(0, this->transmitter_);

  auto protocol_constructor = protocol_constructor_map.find(protocol_);
  if ( protocol_constructor == protocol_constructor_map.end()) {
    ESP_LOGE(TAG, "Invalid protocol");
    return;
  }

// TODO: Is this a memory leak?
  HeatpumpIR *heatpumpIR = protocol_constructor->second();
  heatpumpIR->send(espSender, powerModeCmd, operatingModeCmd, fanSpeedCmd, temperatureCmd, swingVCmd, swingHCmd);
}

}  // namespace heatpumpir
}  // namespace esphome
