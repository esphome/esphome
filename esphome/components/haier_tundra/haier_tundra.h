#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/components/remote_base/haier_protocol.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::haier_tundra {

// Byte 0 - Header value (Unit ID)
static constexpr uint8_t HAIER_HEADER_UNIT_B = 0x59;
static constexpr uint8_t HAIER_HEADER_UNIT_A = 0xA6;

// Byte 1[7:4] - Temperature (˚C) 0x0..0xE
static constexpr uint8_t HAIER_TEMP_MIN = 16;
static constexpr uint8_t HAIER_TEMP_MAX = 30;

// Byte 1[3:0] - Vertical Swing
static constexpr uint8_t HAIER_SWING_V_OFF = 0x0;   // Power off value; from Auto, stops at current position
static constexpr uint8_t HAIER_SWING_V_2TOP = 0x1;  // [|/   ]
static constexpr uint8_t HAIER_SWING_V_1TOP = 0x2;  // [|    ] (Not available in Heat mode)
static constexpr uint8_t HAIER_SWING_V_2BOT = 0x3;  // [   //]
static constexpr uint8_t HAIER_SWING_V_1BOT = 0xA;  // [    /] (Heat mode only)
static constexpr uint8_t HAIER_SWING_V_AUTO = 0xC;

// Byte 2[7:5] - Horizontal Swing
static constexpr uint8_t HAIER_SWING_H_MIDDLE = 0x0;        // [ || ]
static constexpr uint8_t HAIER_SWING_H_LEFT = 0x3;          // [/   ]
static constexpr uint8_t HAIER_SWING_H_MIDDLE_LEFT = 0x4;   // [ |  ]
static constexpr uint8_t HAIER_SWING_H_MIDDLE_RIGHT = 0x5;  // [  | ]
static constexpr uint8_t HAIER_SWING_H_RIGHT = 0x6;         // [   \]
static constexpr uint8_t HAIER_SWING_H_AUTO = 0x7;          // [/||\]

// Byte 3[7:5] - Timer Mode
static constexpr uint8_t HAIER_TIMER_MODE_DISABLED = 0x0;
static constexpr uint8_t HAIER_TIMER_MODE_OFF = 0x1;     // OFF
static constexpr uint8_t HAIER_TIMER_MODE_ON = 0x2;      // ON
static constexpr uint8_t HAIER_TIMER_MODE_ON_OFF = 0x4;  // ON → OFF
static constexpr uint8_t HAIER_TIMER_MODE_OFF_ON = 0x5;  // ON ← OFF

// Byte 3[1] - Health
// Byte 4[6] - Power

// Byte 5[7:5] - Fan Speed
static constexpr uint8_t HAIER_FAN_HIGH = 0x1;
static constexpr uint8_t HAIER_FAN_MED = 0x2;
static constexpr uint8_t HAIER_FAN_LOW = 0x3;
static constexpr uint8_t HAIER_FAN_AUTO = 0x5;

// Byte 5[4:0] - Timer Off Hours (0-23) 0x00..0x17
// Byte 6[7] - Quiet Mode
// Byte 6[6] - Turbo Mode
// Byte 6[5:0] - Timer Off Minutes (0-59) 0x00..0x3B

// Byte 7[7:5] - Mode
static constexpr uint8_t HAIER_MODE_AUTO = 0x0;
static constexpr uint8_t HAIER_MODE_COOL = 0x1;
static constexpr uint8_t HAIER_MODE_DRY = 0x2;
static constexpr uint8_t HAIER_MODE_HEAT = 0x4;
static constexpr uint8_t HAIER_MODE_FAN = 0x6;

// Byte 7[4:0] - Timer On Hours (0-23) 0x00..0x17
// Byte 8[7] - Sleep Mode
// Byte 8[5:0] - Timer On Minutes (0-59) 0x00..0x3B
// Byte 10[4] - Self Clean

// Byte 12 - Button
static constexpr uint8_t HAIER_BUTTON_TEMP_UP = 0x00;
static constexpr uint8_t HAIER_BUTTON_TEMP_DOWN = 0x01;
static constexpr uint8_t HAIER_BUTTON_SWING_V = 0x02;
static constexpr uint8_t HAIER_BUTTON_SWING_H = 0x03;
static constexpr uint8_t HAIER_BUTTON_FAN_SPEED = 0x04;
static constexpr uint8_t HAIER_BUTTON_POWER = 0x05;
static constexpr uint8_t HAIER_BUTTON_MODE = 0x06;
static constexpr uint8_t HAIER_BUTTON_HEALTH = 0x07;
static constexpr uint8_t HAIER_BUTTON_TURBO_QUIET = 0x08;
static constexpr uint8_t HAIER_BUTTON_SLEEP = 0x0B;
static constexpr uint8_t HAIER_BUTTON_TIMER = 0x10;
static constexpr uint8_t HAIER_BUTTON_LIGHT = 0x15;
static constexpr uint8_t HAIER_BUTTON_SELF_CLEAN = 0x19;

class HaierTundra final : public climate_ir::ClimateIR {
 public:
  HaierTundra()
      : climate_ir::ClimateIR(HAIER_TEMP_MIN, HAIER_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL,
                               climate::CLIMATE_SWING_HORIZONTAL, climate::CLIMATE_SWING_BOTH}) {}

#ifdef USE_SWITCH
  SUB_SWITCH(health);
  SUB_SWITCH(quiet);
  SUB_SWITCH(turbo);
#endif

  void set_health_mode(bool health);
  void set_quiet_mode(bool quiet);
  void set_turbo_mode(bool turbo);

  void toggle_light();
  void start_self_clean();

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;

  uint8_t temperature_();
  uint8_t vertical_swing_();
  uint8_t horizontal_swing_();
  uint8_t health_();
  uint8_t power_();
  uint8_t fan_speed_();
  uint8_t quiet_();
  uint8_t turbo_();
  uint8_t operation_mode_();
  uint8_t self_clean_();
  uint8_t button_();

  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool on_haier_(const remote_base::HaierData &haier);

  float get_temperature_(uint8_t temp);
  climate::ClimateSwingMode get_swing_mode_(uint8_t swing_v, uint8_t swing_h);
  climate::ClimateMode get_operation_mode_(uint8_t power, uint8_t mode);
  climate::ClimateFanMode get_fan_speed_(uint8_t fan);

  bool health_mode_{false};
  bool quiet_mode_{false};
  bool turbo_mode_{false};

  bool light_flag_{false};
  bool self_clean_flag_{false};
};

}  // namespace esphome::haier_tundra
