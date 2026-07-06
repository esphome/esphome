#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::hisense_arg {

// Temperature (Variant A: 16-30C)
static constexpr uint8_t HISENSE_ARG_TEMP_MIN = 16;
static constexpr uint8_t HISENSE_ARG_TEMP_MAX = 30;
static constexpr uint8_t HISENSE_ARG_TEMP_FAN = 23;  // Fixed temp for fan-only mode

// IR Timing from firmware RE (microseconds)
static constexpr uint32_t HISENSE_ARG_IR_FREQUENCY = 38000;
static constexpr uint32_t HISENSE_ARG_HEADER_MARK = 8960;
static constexpr uint32_t HISENSE_ARG_HEADER_SPACE = 4480;
static constexpr uint32_t HISENSE_ARG_BIT_MARK = 560;
static constexpr uint32_t HISENSE_ARG_ONE_SPACE = 1680;
static constexpr uint32_t HISENSE_ARG_ZERO_SPACE = 560;
static constexpr uint32_t HISENSE_ARG_SEPARATOR_SPACE = 7840;
static constexpr uint32_t HISENSE_ARG_TRAIL_SPACE = 39200;

// Mode codes (byte[3] bits[2:0])
static constexpr uint8_t HISENSE_ARG_MODE_HEAT = 0x00;
static constexpr uint8_t HISENSE_ARG_MODE_AUTO = 0x01;
static constexpr uint8_t HISENSE_ARG_MODE_COOL = 0x02;
static constexpr uint8_t HISENSE_ARG_MODE_DRY = 0x03;
static constexpr uint8_t HISENSE_ARG_MODE_FAN = 0x04;

// Fan speed codes (byte[2] bits[1:0])
static constexpr uint8_t HISENSE_ARG_FAN_AUTO = 0x00;
static constexpr uint8_t HISENSE_ARG_FAN_HIGH = 0x01;
static constexpr uint8_t HISENSE_ARG_FAN_MEDIUM = 0x02;
static constexpr uint8_t HISENSE_ARG_FAN_LOW = 0x03;

// Power transition (byte[15])
static constexpr uint8_t HISENSE_ARG_POWER_TOGGLE = 0x01;     // Power state change (on->off or off->on)
static constexpr uint8_t HISENSE_ARG_POWER_NO_CHANGE = 0x02;  // No power change, settings update only

// Power transition flag (byte[2] bit 2)
static constexpr uint8_t HISENSE_ARG_POWER_FLAG = 0x04;  // Set during any power transition

// Swing (byte[8])
static constexpr uint8_t HISENSE_ARG_SWING_H = 0x80;  // Horizontal swing
static constexpr uint8_t HISENSE_ARG_SWING_V = 0x40;  // Vertical swing

// Variant flag (byte[18] bit 3)
static constexpr uint8_t HISENSE_ARG_VARIANT_A = 0x08;

// Frame sizes
static constexpr uint8_t HISENSE_ARG_FRAME1_SIZE = 6;  // bytes 0-5
static constexpr uint8_t HISENSE_ARG_FRAME2_SIZE = 8;  // bytes 6-13
static constexpr uint8_t HISENSE_ARG_FRAME3_SIZE = 7;  // bytes 14-20
static constexpr uint8_t HISENSE_ARG_STATE_SIZE = 21;

class HisenseArgClimate : public climate_ir::ClimateIR {
 public:
  HisenseArgClimate()
      : climate_ir::ClimateIR(HISENSE_ARG_TEMP_MIN, HISENSE_ARG_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  /// Force the component to assume the AC is off.
  /// Use when external power monitoring detects the unit is not running.
  void set_power_off() {
    this->prev_power_on_ = false;
    this->mode = climate::CLIMATE_MODE_OFF;
    this->publish_state();
  }

  /// Force the component to assume the AC is on.
  /// Use when external power monitoring detects the unit is running.
  void set_power_on() {
    this->prev_power_on_ = true;
    this->mode = climate::CLIMATE_MODE_HEAT_COOL;
    this->publish_state();
  }

 protected:
  void transmit_state() override;
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);

 private:
  uint8_t operation_mode_() const;
  uint8_t fan_speed_() const;
  uint8_t temperature_() const;
  uint8_t swing_() const;
  bool prev_power_on_{false};  // Track power state for transitions
};

}  // namespace esphome::hisense_arg
