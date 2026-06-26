#include "receiver_mitsubishi_heavy.h"
#include "heatpumpir.h"

#if defined(USE_ARDUINO) || defined(USE_ESP32)

#include <HeatpumpIRFactory.h>

namespace esphome::heatpumpir {

bool decode_mitsubishi_heavy_zmp(const uint8_t frame[11], HeatpumpIRClimate &climate) {
  static const uint8_t byte_swing_h = 5;
  static const uint8_t byte_fan = 7;
  static const uint8_t fan_speed_mask = 0xE0;
  static const uint8_t horizontal_swing_mask = 0xDC;
  static const uint8_t horizontal_swing_active = 0x5C;
  static const uint8_t vs_mask_byte5 = 0x02;
  static const uint8_t vs_mask_byte7 = 0x18;

  uint8_t fan = frame[byte_fan] & fan_speed_mask;
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
      break;
    case MITSUBISHI_HEAVY_ZMP_ECONO:
      climate.preset = climate::CLIMATE_PRESET_ECO;
      break;
  }

  uint8_t swing_h = frame[byte_swing_h] & horizontal_swing_mask;
  uint8_t swing_v = (frame[byte_swing_h] & vs_mask_byte5) | (frame[byte_fan] & vs_mask_byte7);
  bool h_swing = (swing_h == horizontal_swing_active);
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
