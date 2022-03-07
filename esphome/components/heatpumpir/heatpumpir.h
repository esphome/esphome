#pragma once

#ifdef USE_ARDUINO

#include "esphome/components/climate_ir/climate_ir.h"

// Forward-declare HeatpumpIR class from library. We cannot include its header here because it has unnamespaced defines
// that conflict with ESPHome.
class HeatpumpIR;

namespace esphome {
namespace heatpumpir {

// Simple enum to represent protocols.
enum Protocol {
  PROTOCOL_AUX,
  PROTOCOL_BALLU,
  PROTOCOL_CARRIER_MCA,
  PROTOCOL_CARRIER_NQV,
  PROTOCOL_DAIKIN_ARC417,
  PROTOCOL_DAIKIN_ARC480,
  PROTOCOL_DAIKIN,
  PROTOCOL_FUEGO,
  PROTOCOL_FUJITSU_AWYZ,
  PROTOCOL_GREE,
  PROTOCOL_GREEYAA,
  PROTOCOL_GREEYAN,
  PROTOCOL_GREEYAC,
  PROTOCOL_HISENSE_AUD,
  PROTOCOL_HITACHI,
  PROTOCOL_HYUNDAI,
  PROTOCOL_IVT,
  PROTOCOL_MIDEA,
  PROTOCOL_MITSUBISHI_FA,
  PROTOCOL_MITSUBISHI_FD,
  PROTOCOL_MITSUBISHI_FE,
  PROTOCOL_MITSUBISHI_HEAVY_FDTC,
  PROTOCOL_MITSUBISHI_HEAVY_ZJ,
  PROTOCOL_MITSUBISHI_HEAVY_ZM,
  PROTOCOL_MITSUBISHI_HEAVY_ZMP,
  PROTOCOL_MITSUBISHI_KJ,
  PROTOCOL_MITSUBISHI_MSC,
  PROTOCOL_MITSUBISHI_MSY,
  PROTOCOL_MITSUBISHI_SEZ,
  PROTOCOL_PANASONIC_CKP,
  PROTOCOL_PANASONIC_DKE,
  PROTOCOL_PANASONIC_JKE,
  PROTOCOL_PANASONIC_LKE,
  PROTOCOL_PANASONIC_NKE,
  PROTOCOL_SAMSUNG_AQV,
  PROTOCOL_SAMSUNG_FJM,
  PROTOCOL_SHARP,
  PROTOCOL_TOSHIBA_DAISEIKAI,
  PROTOCOL_TOSHIBA,
};

// Simple enum to represent horizontal directios
enum HorizontalDirection {
  HORIZONTAL_DIRECTION_AUTO = 0,
  HORIZONTAL_DIRECTION_MIDDLE = 1,
  HORIZONTAL_DIRECTION_LEFT = 2,
  HORIZONTAL_DIRECTION_MLEFT = 3,
  HORIZONTAL_DIRECTION_MRIGHT = 4,
  HORIZONTAL_DIRECTION_RIGHT = 5,
};

// Simple enum to represent vertical directions
enum VerticalDirection {
  VERTICAL_DIRECTION_AUTO = 0,
  VERTICAL_DIRECTION_UP = 1,
  VERTICAL_DIRECTION_MUP = 2,
  VERTICAL_DIRECTION_MIDDLE = 3,
  VERTICAL_DIRECTION_MDOWN = 4,
  VERTICAL_DIRECTION_DOWN = 5,
};

// Temperature
const float TEMP_MIN = 0;    // Celsius
const float TEMP_MAX = 100;  // Celsius

class HeatpumpIRClimate : public climate_ir::ClimateIR {
 public:
  HeatpumpIRClimate()
      : climate_ir::ClimateIR(
            TEMP_MIN, TEMP_MAX, 1.0f, true, true,
            std::set<climate::ClimateFanMode>{climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                                              climate::CLIMATE_FAN_HIGH, climate::CLIMATE_FAN_AUTO},
            std::set<climate::ClimateSwingMode>{climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL,
                                                climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_BOTH}) {}
  void setup() override;
  void set_protocol(Protocol protocol) { this->protocol_ = protocol; }
  void set_horizontal_default(HorizontalDirection horizontal_direction) {
    this->default_horizontal_direction_ = horizontal_direction;
  }
  void set_vertical_default(VerticalDirection vertical_direction) {
    this->default_vertical_direction_ = vertical_direction;
  }

  void set_max_temperature(float temperature) { this->max_temperature_ = temperature; }
  void set_min_temperature(float temperature) { this->min_temperature_ = temperature; }

 protected:
  HeatpumpIR *heatpump_ir_;
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  Protocol protocol_;
  HorizontalDirection default_horizontal_direction_;
  VerticalDirection default_vertical_direction_;

  float max_temperature_;
  float min_temperature_;
};

}  // namespace heatpumpir
}  // namespace esphome

#endif
