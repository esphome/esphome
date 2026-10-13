#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::daikin {

// Values for Daikin ARC43XXX IR Controllers
// Temperature
const uint8_t DAIKIN_TEMP_MIN = 10;  // Celsius
const uint8_t DAIKIN_TEMP_MAX = 30;  // Celsius

// Modes
const uint8_t DAIKIN_MODE_AUTO = 0x00;
const uint8_t DAIKIN_MODE_COOL = 0x30;
const uint8_t DAIKIN_MODE_HEAT = 0x40;
const uint8_t DAIKIN_MODE_DRY = 0x20;
const uint8_t DAIKIN_MODE_FAN = 0x60;
const uint8_t DAIKIN_MODE_OFF = 0x00;
const uint8_t DAIKIN_MODE_ON = 0x01;

// Fan Speed
const uint8_t DAIKIN_FAN_AUTO = 0xA0;
const uint8_t DAIKIN_FAN_SILENT = 0xB0;
const uint8_t DAIKIN_FAN_1 = 0x30;
const uint8_t DAIKIN_FAN_2 = 0x40;
const uint8_t DAIKIN_FAN_3 = 0x50;
const uint8_t DAIKIN_FAN_4 = 0x60;
const uint8_t DAIKIN_FAN_5 = 0x70;

// Presets: bit flags in byte 6 (comfort), byte 29 (powerful, quiet) and byte 32 (sensor, econo)
const uint8_t DAIKIN_PRESET_COMFORT_ON = 0x10;
const uint8_t DAIKIN_PRESET_POWERFUL_ON = 0x01;
const uint8_t DAIKIN_PRESET_QUIET_ON = 0x20;
const uint8_t DAIKIN_PRESET_ECONO_ON = 0x04;
const uint8_t DAIKIN_PRESET_SENSOR_ON = 0x02;
// The unit leaves powerful mode by itself after 20 minutes; mirror that in the entity
const uint32_t DAIKIN_BOOST_DURATION_MS = 20 * 60 * 1000;
// The three frames of one transmission arrive well inside this window
const uint32_t DAIKIN_TRANSMISSION_MS = 1000;

// IR Transmission
const uint32_t DAIKIN_IR_FREQUENCY = 38000;
const uint32_t DAIKIN_HEADER_MARK = 3360;
const uint32_t DAIKIN_HEADER_SPACE = 1760;
const uint32_t DAIKIN_BIT_MARK = 520;
const uint32_t DAIKIN_ONE_SPACE = 1370;
const uint32_t DAIKIN_ZERO_SPACE = 360;
const uint32_t DAIKIN_MESSAGE_SPACE = 32300;

// Frame types (byte 4) and sizes
const uint8_t DAIKIN_FRAME_TYPE_STATE = 0x00;
const uint8_t DAIKIN_FRAME_TYPE_COMFORT = 0xC5;
const uint8_t DAIKIN_STATE_FRAME_SIZE = 19;
const uint8_t DAIKIN_COMFORT_FRAME_SIZE = 8;

class DaikinClimate final : public climate_ir::ClimateIR {
 public:
  DaikinClimate()
      : climate_ir::ClimateIR(
            DAIKIN_TEMP_MIN, DAIKIN_TEMP_MAX, 0.5f, true, true,
            {climate::CLIMATE_FAN_QUIET, climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW,
             climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH},
            {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL,
             climate::CLIMATE_SWING_BOTH},
            {climate::CLIMATE_PRESET_ECO, climate::CLIMATE_PRESET_BOOST, climate::CLIMATE_PRESET_COMFORT,
             climate::CLIMATE_PRESET_SLEEP, climate::CLIMATE_PRESET_ACTIVITY, climate::CLIMATE_PRESET_NONE}) {}

 protected:
  void setup() override;
  void control(const climate::ClimateCall &call) override;
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  uint8_t operation_mode_() const;
  uint16_t fan_speed_() const;
  uint8_t temperature_() const;
  void set_presets_(uint8_t *remote_state) const;
  /// Arms or cancels the boost timer when the preset moves into or out of boost.
  void update_boost_timer_(bool was_boost);
  void cancel_boost_mode_();
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);

  /// What the 0xC5 frame of the current transmission said about comfort, applied by the state frame
  enum class PendingComfort : uint8_t { PENDING_COMFORT_UNKNOWN, PENDING_COMFORT_OFF, PENDING_COMFORT_ON };
  PendingComfort pending_comfort_{PendingComfort::PENDING_COMFORT_UNKNOWN};
  uint32_t pending_comfort_at_{0};
};

}  // namespace esphome::daikin
