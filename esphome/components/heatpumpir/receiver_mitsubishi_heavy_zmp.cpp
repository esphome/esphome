#include "receiver_mitsubishi_heavy.h"
#include "heatpumpir.h"

#if defined(USE_ARDUINO) || defined(USE_ESP32)

#include <HeatpumpIRFactory.h>

namespace esphome::heatpumpir {

bool decode_mitsubishi_heavy_zmp(const uint8_t frame[11], HeatpumpIRClimate &climate) {
  static const uint8_t BYTE_SWING_H = 5;
  static const uint8_t BYTE_FAN = 7;
  static const uint8_t FAN_SPEED_MASK = 0xE0;
  static const uint8_t HORIZONTAL_SWING_MASK = 0xDC;
  static const uint8_t HORIZONTAL_SWING_ACTIVE = 0x5C;
  static const uint8_t VS_MASK_BYTE5 = 0x02;
  static const uint8_t VS_MASK_BYTE7 = 0x18;

  uint8_t fan = frame[BYTE_FAN] & FAN_SPEED_MASK;
  climate.preset = climate::CLIMATE_PRESET_NONE;
  switch (fan) {
    case MITSUBISHI_HEAVY_ZMP_FAN_AUTO:
      climate.fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case MITSUBISHI_HEAVY_ZMP_FAN1:
      climate.fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case MITSUBISHI_HEAVY_ZMP_FAN2:
      climate.fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case MITSUBISHI_HEAVY_ZMP_FAN3:
      climate.fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case MITSUBISHI_HEAVY_ZMP_HIPOWER:
      climate.preset = climate::CLIMATE_PRESET_BOOST;
      break;  // fan_mode intentionally preserved — remote restores it on preset exit
    case MITSUBISHI_HEAVY_ZMP_ECONO:
      climate.preset = climate::CLIMATE_PRESET_ECO;
      break;  // fan_mode intentionally preserved — remote restores it on preset exit
  }

  uint8_t swing_h = frame[BYTE_SWING_H] & HORIZONTAL_SWING_MASK;
  uint8_t swing_v = (frame[BYTE_SWING_H] & VS_MASK_BYTE5) | (frame[BYTE_FAN] & VS_MASK_BYTE7);
  bool h_swing = (swing_h == HORIZONTAL_SWING_ACTIVE);
  bool v_swing = (swing_v == MITSUBISHI_HEAVY_ZMP_VS_SWING);

  if (h_swing && v_swing) {
    climate.swing_mode = climate::CLIMATE_SWING_BOTH;
  } else if (h_swing) {
    climate.swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
  } else if (v_swing) {
    climate.swing_mode = climate::CLIMATE_SWING_VERTICAL;
  } else {
    climate.swing_mode = climate::CLIMATE_SWING_OFF;
  }

  return true;
}

}  // namespace esphome::heatpumpir

#endif
