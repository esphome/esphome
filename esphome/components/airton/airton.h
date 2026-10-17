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
static constexpr uint8_t AIRTON_TEMP_MIN = 16;        // Celsius
static constexpr uint8_t AIRTON_TEMP_MAX = 31;        // Celsius
static constexpr uint8_t AIRTON_AUTO_MODE_TEMP = 25;  // The fixed setpoint the unit uses in auto mode

static constexpr uint8_t AIRTON_MODE_AUTO = 0b000;
static constexpr uint8_t AIRTON_MODE_COOL = 0b001;
static constexpr uint8_t AIRTON_MODE_HEAT = 0b100;
static constexpr uint8_t AIRTON_MODE_DRY = 0b010;
static constexpr uint8_t AIRTON_MODE_FAN = 0b011;

static constexpr uint8_t AIRTON_FAN_AUTO = 0b000;
static constexpr uint8_t AIRTON_FAN_1 = 0b001;
static constexpr uint8_t AIRTON_FAN_2 = 0b010;
static constexpr uint8_t AIRTON_FAN_3 = 0b011;
static constexpr uint8_t AIRTON_FAN_4 = 0b100;
static constexpr uint8_t AIRTON_FAN_5 = 0b101;

static constexpr uint32_t AIRTON_IR_FREQUENCY = 38000;
static constexpr uint32_t AIRTON_HEADER_MARK = 6630;
static constexpr uint32_t AIRTON_HEADER_SPACE = 3350;
static constexpr uint32_t AIRTON_BIT_MARK = 400;
static constexpr uint32_t AIRTON_ONE_SPACE = 1260;
static constexpr uint32_t AIRTON_ZERO_SPACE = 430;
static constexpr uint32_t AIRTON_MESSAGE_SPACE = 100000;

static constexpr uint8_t AIRTON_STATE_FRAME_SIZE = 7;
// Every frame starts with these two bytes
static constexpr uint8_t AIRTON_HEADER_BYTE0 = 0xD3;
static constexpr uint8_t AIRTON_HEADER_BYTE1 = 0x11;

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

// The on/off settings the switch platform exposes
enum class AirtonSetting : uint8_t {
  AIRTON_SETTING_SLEEP_MODE,
  AIRTON_SETTING_DISPLAY,
};

// The settings the unit keeps that the climate state does not carry; saved to flash when they change
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
  void control(const climate::ClimateCall &call) override;

  // The direction the settings start from until a saved one is restored in setup()
  void set_default_vertical_direction(VerticalDirection direction) { this->settings_.vertical_direction = direction; }

  // The setters update the state, publish their entity and save; they return whether anything changed.
  // A change made here (switch, select, action) is then sent with send_settings(); a frame from the
  // remote only reports what the unit already did.
  bool set_sleep_mode_state(bool state);
  bool set_display_state(bool state);
  bool set_vertical_direction_state(VerticalDirection direction);
  void send_settings();
  void set_setting(AirtonSetting setting, bool state) {
    const bool changed = setting == AirtonSetting::AIRTON_SETTING_DISPLAY ? this->set_display_state(state)
                                                                          : this->set_sleep_mode_state(state);
    if (changed)
      this->send_settings();
  }

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
  void sync_swing_mode_();

  void transmit_state() override;
  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t temperature_();
  uint8_t operation_settings_();

  bool on_receive(remote_base::RemoteReceiveData data) override;
  void parse_state_frame_(const uint8_t *frame);

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
