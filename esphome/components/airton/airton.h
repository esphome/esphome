#pragma once

#include "esphome/components/climate_ir/climate_ir.h"
#include "esphome/core/defines.h"
#include "esphome/core/preferences.h"

#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif

namespace esphome::airton {

// Values for Airton SMVH09B-2A2A3NH IR Controllers
// Temperature
static constexpr uint8_t AIRTON_TEMP_MIN = 16;  // Celsius
static constexpr uint8_t AIRTON_TEMP_MAX = 31;  // Celsius

// Modes
static constexpr uint8_t AIRTON_MODE_AUTO = 0b000;
static constexpr uint8_t AIRTON_MODE_COOL = 0b001;
static constexpr uint8_t AIRTON_MODE_HEAT = 0b100;
static constexpr uint8_t AIRTON_MODE_DRY = 0b010;
static constexpr uint8_t AIRTON_MODE_FAN = 0b011;

// Fan Speed
static constexpr uint8_t AIRTON_FAN_AUTO = 0b000;
static constexpr uint8_t AIRTON_FAN_1 = 0b001;
static constexpr uint8_t AIRTON_FAN_2 = 0b010;
static constexpr uint8_t AIRTON_FAN_3 = 0b011;
static constexpr uint8_t AIRTON_FAN_4 = 0b100;
static constexpr uint8_t AIRTON_FAN_5 = 0b101;

// IR Transmission
static constexpr uint32_t AIRTON_IR_FREQUENCY = 38000;
static constexpr uint32_t AIRTON_HEADER_MARK = 6630;
static constexpr uint32_t AIRTON_HEADER_SPACE = 3350;
static constexpr uint32_t AIRTON_BIT_MARK = 400;
static constexpr uint32_t AIRTON_ONE_SPACE = 1260;
static constexpr uint32_t AIRTON_ZERO_SPACE = 430;
static constexpr uint32_t AIRTON_MESSAGE_SPACE = 100000;

// State Frame size
static constexpr uint8_t AIRTON_STATE_FRAME_SIZE = 7;

// Position of the air outlet flap; the values are the nibble the frame carries and the index
// of the option in the vertical direction select
enum class VerticalDirection : uint8_t {
  VERTICAL_DIRECTION_OFF = 0,
  VERTICAL_DIRECTION_SWING = 1,
  VERTICAL_DIRECTION_UP = 2,
  VERTICAL_DIRECTION_MIDDLE_UP = 3,
  VERTICAL_DIRECTION_MIDDLE = 4,
  VERTICAL_DIRECTION_MIDDLE_DOWN = 5,
  VERTICAL_DIRECTION_DOWN = 6,
};

// The settings the unit keeps that the climate state does not carry; saved to flash when set locally
struct AirtonSettings {
  bool sleep_state;
  bool display_state;
  VerticalDirection vertical_direction;
};

class AirtonClimate : public climate_ir::ClimateIR {
 public:
  AirtonClimate()
      : climate_ir::ClimateIR(AIRTON_TEMP_MIN, AIRTON_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}
  void setup() override;

  // send_ir is true for a change made here (switch, select, action): it is sent to the unit and saved.
  // A frame received from the remote only updates the state, so the remote cannot wear the flash.
  void set_sleep_mode_state(bool state, bool send_ir);
  bool get_sleep_mode_state() const { return this->settings_.sleep_state; }
  void set_display_state(bool state, bool send_ir);
  bool get_display_state() const { return this->settings_.display_state; }
  void set_vertical_direction_state(VerticalDirection state, bool send_ir);
  VerticalDirection get_vertical_direction_state() const { return this->settings_.vertical_direction; }
  void control(const climate::ClimateCall &call) override;

#ifdef USE_SWITCH
  void set_sleep_mode_switch(switch_::Switch *sw) { this->sleep_mode_switch_ = sw; }
  void set_display_switch(switch_::Switch *sw) { this->display_switch_ = sw; }
#endif
#ifdef USE_SELECT
  void set_vertical_direction_select(select::Select *sel) { this->vertical_direction_select_ = sel; }
#endif

 protected:
  void publish_settings_();
  void save_settings_();

  // IR transmission payload builder
  void transmit_state() override;

  uint8_t operation_mode_();
  uint16_t fan_speed_();
  bool turbo_control_();
  uint8_t temperature_();
  uint8_t operation_settings_();
  uint8_t checksum_(const uint8_t *r_state);

  // IR receiver buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(uint8_t const frame[]);

#ifdef USE_SWITCH
  switch_::Switch *sleep_mode_switch_{nullptr};
  switch_::Switch *display_switch_{nullptr};
#endif
#ifdef USE_SELECT
  select::Select *vertical_direction_select_{nullptr};
#endif
  ESPPreferenceObject settings_pref_;
  // The unit's own defaults: display lit, no sleep mode, flap still
  AirtonSettings settings_{false, true, VerticalDirection::VERTICAL_DIRECTION_OFF};
  // The last mode sent, so turning off repeats it with the power bit cleared
  uint8_t previous_mode_{0};
};

}  // namespace esphome::airton
