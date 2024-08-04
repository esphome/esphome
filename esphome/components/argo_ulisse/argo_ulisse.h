#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace argo_ulisse {

// Values for Argo Ulisse 13 DCI Eco (WREM-3 remote) IR Controllers
// Originally reverse-engineered by @mbronk
// Temperature
const uint8_t ARGO_TEMP_MIN = 10;         // Celsius
const uint8_t ARGO_TEMP_MAX = 32;         // Celsius
const uint8_t ARGO_SENSOR_TEMP_MIN = 0;   // Celsius
const uint8_t ARGO_SENSOR_TEMP_MAX = 35;  // Celsius
const uint8_t ARGO_TEMP_OFFSET = 4;       // Celsius

// Modes
const uint8_t ARGO_MODE_OFF = 0x00;
const uint8_t ARGO_MODE_ON = 0x01;
const uint8_t ARGO_MODE_COOL = 0x01;
const uint8_t ARGO_MODE_DRY = 0x02;
const uint8_t ARGO_MODE_HEAT = 0x03;
const uint8_t ARGO_MODE_FAN = 0x04;
const uint8_t ARGO_MODE_AUTO = 0x05;

// Fan Speed
const uint8_t ARGO_FAN_AUTO = 0x00;
const uint8_t ARGO_FAN_SILENT = 0x01;  // lowest
const uint8_t ARGO_FAN_LOWEST = 0x02;
const uint8_t ARGO_FAN_LOW = 0x03;
const uint8_t ARGO_FAN_MEDIUM = 0x04;
const uint8_t ARGO_FAN_HIGH = 0x05;
const uint8_t ARGO_FAN_HIGHEST = 0x06;  // highest

// Flap position (swing modes)
const uint8_t ARGO_FLAP_HALF_AUTO = 0x00;
const uint8_t ARGO_FLAP_HIGHEST = 0x01;
const uint8_t ARGO_FLAP_HIGH = 0x02;
const uint8_t ARGO_FLAP_MIDDLE_HIGH = 0x03;
const uint8_t ARGO_FLAP_MIDDLE_LOW = 0x04;
const uint8_t ARGO_FLAP_LOW = 0x05;
const uint8_t ARGO_FLAP_LOWEST = 0x06;
const uint8_t ARGO_FLAP_FULL_AUTO = 0x07;

// IR Transmission
const uint32_t ARGO_IR_FREQUENCY = 38000;

const uint32_t ARGO_HEADER_MARK = 6400;   // 00F7 = 247 * 26.3 = 6496.1 µs
const uint32_t ARGO_HEADER_SPACE = 3200;  // 007C = 124 * 26.3 = 3261.2 µs
const uint32_t ARGO_BIT_MARK = 400;       // 0010 = 16 * 26.3 = 420.8 µs
const uint32_t ARGO_ONE_SPACE = 2200;     // 0054 = 84 * 26.3 = 2209.2 µs
const uint32_t ARGO_ZERO_SPACE = 900;     // 0022 = 34 * 26.3 = 894.2 µs

const uint8_t ARGO_PREAMBLE = 0xB;  // 0b1011
const uint8_t ARGO_POST1 = 0x30;    // unknown, always 0b110000 (TempScale?)
const size_t ARGO_STATE_LENGTH = 12;

// Native representation of A/C IR message for WREM-3 remote
#pragma pack(push, 1)  // Add a packing directive to ensure the structure is tightly packed:
union ArgoProtocolWREM3 {
  uint8_t raw[ARGO_STATE_LENGTH];  ///< The state in native IR code form
  struct {
    // Byte 0 (same definition across the union)
    uint8_t Pre1 : 4;           /// Preamble: 0b1011 @ref ARGO_PREAMBLE
    uint8_t IrChannel : 2;      /// 0..3 range
    uint8_t IrCommandType : 2;  /// @ref argoIrMessageType_t
    // Byte 1
    uint8_t RoomTemp : 5;  // in Celsius, range:  4..35 (offset by -4[*C])
    uint8_t Mode : 3;      /// @ref argoMode_t
    // Byte 2
    uint8_t Temp : 5;  // in Celsius, range: 10..32 (offset by -4[*C])
    uint8_t Fan : 3;   /// @ref argoFan_t
    // Byte3
    uint8_t Flap : 3;   /// SwingV @ref argoFlap_t
    uint8_t Power : 1;  // boost mode
    uint8_t iFeel : 1;
    uint8_t Night : 1;
    uint8_t Eco : 1;
    uint8_t Boost : 1;  ///< a.k.a. Turbo
    // Byte4
    uint8_t Filter : 1;
    uint8_t Light : 1;
    uint8_t Post1 : 6;  /// Unknown, always 0b110000 (TempScale?)
    // Byte5
    uint8_t Sum : 8;  /// Checksum
  };
  struct iFeel {
    // Byte 0 (same definition across the union)
    uint8_t : 8;  // {Pre1 | IrChannel | IrCommandType}
    // Byte 1
    uint8_t SensorT : 5;  // in Celsius, range:  4..35 (offset by -4[*C])
    uint8_t CheckHi : 3;  // Checksum (short)
  } ifeel;
  struct Timer {
    // Byte 0 (same definition across the union)
    uint8_t : 8;  // {Pre1 | IrChannel | IrCommandType}
    // Byte 1
    uint8_t IsOn : 1;
    uint8_t TimerType : 3;
    uint8_t CurrentTimeLo : 4;
    // Byte 2
    uint8_t CurrentTimeHi : 7;
    uint8_t CurrentWeekdayLo : 1;
    // Byte 3
    uint8_t CurrentWeekdayHi : 2;
    uint8_t DelayTimeLo : 6;
    // Byte 4
    uint8_t DelayTimeHi : 5;
    uint8_t TimerStartLo : 3;
    // Byte 5
    uint8_t TimerStartHi : 8;
    // Byte 6
    uint8_t TimerEndLo : 8;
    // Byte 7
    uint8_t TimerEndHi : 3;
    uint8_t TimerActiveDaysLo : 5;  // Bitmap (LSBit is Sunday)
    // Byte 8
    uint8_t TimerActiveDaysHi : 2;  // Bitmap (LSBit is Sunday)
    uint8_t Post1 : 1;              // Unknown, always 1
    uint8_t Checksum : 5;
  } timer;
  struct Config {
    uint8_t : 8;           // Byte 0 {Pre1 | IrChannel | IrCommandType}
    uint8_t Key : 8;       // Byte 1
    uint8_t Value : 8;     // Byte 2
    uint8_t Checksum : 8;  // Byte 3
  } config;
};
#pragma pack(pop)

typedef enum _ArgoIRMessageType {
  ArgoIRMessageType_AC_CONTROL = 0,
  ArgoIRMessageType_IFEEL_TEMP_REPORT = 1,
  ArgoIRMessageType_TIMER_COMMAND = 2,
  ArgoIRMessageType_CONFIG_PARAM_SET = 3,
} ArgoIRMessageType;

// raw byte length depends on message type
typedef enum _ArgoIRMessageLength {
  ArgoIRMessageLength_AC_CONTROL = 6,
  ArgoIRMessageLength_IFEEL_TEMP_REPORT = 2,
  ArgoIRMessageLength_TIMER_COMMAND = 9,
  ArgoIRMessageLength_CONFIG_PARAM_SET = 4,
} ArgoIRMessageLength;

class ArgoUlisseClimate : public climate_ir::ClimateIR {
 public:
  ArgoUlisseClimate()
      : climate_ir::ClimateIR(
            ARGO_TEMP_MIN, ARGO_TEMP_MAX, 1.0f, true, true,
            {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
             climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_VERTICAL},
            {
                climate::CLIMATE_PRESET_NONE,
                climate::CLIMATE_PRESET_ECO,
                climate::CLIMATE_PRESET_BOOST,
                climate::CLIMATE_PRESET_SLEEP,
            }) {}

  void setup() override;

  // Setters for the additional features (e.g. for template switches)
  void set_ifeel(bool ifeel) {
    this->ifeel_ = ifeel;
    transmit_state();
  }
  void set_light(bool light) {
    this->light_ = light;
    transmit_state();
  }
  void set_filter(bool filter) {
    this->filter_ = filter;
    transmit_state();
  }

  bool get_ifeel() { return this->ifeel_; }
  bool get_light() { return this->light_; }
  bool get_filter() { return this->filter_; }

 protected:
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  int32_t last_transmit_time_{};
  void transmit_state() override;
  void transmit_ifeel_();
  uint8_t calc_checksum_(const ArgoProtocolWREM3 *data, size_t size);
  climate::ClimateTraits traits() override;
  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t swing_mode_();
  uint8_t temperature_();
  uint8_t sensor_temperature_();
  uint8_t process_temperature_(float temperature);

  // booleans to handle the additional features
  bool ifeel_{true};
  bool light_{true};
  bool filter_{true};
};

}  // namespace argo_ulisse
}  // namespace esphome
