#pragma once

#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

// MideaUART
#include <Appliance/AirConditioner/AirConditioner.h>

#include <cmath>

#include "esphome/core/defines.h"
#include "appliance_base.h"
#include "esphome/components/sensor/sensor.h"
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif

namespace esphome::midea::ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;
using climate::ClimateModeMask;
using climate::ClimateSwingModeMask;
using climate::ClimatePresetMask;

// How the display toggle reaches the appliance; the AUTO rules are in do_display_toggle()
enum class DisplayControl : uint8_t {
  DISPLAY_CONTROL_UART,  // always the UART request
  DISPLAY_CONTROL_AUTO,  // UART when the appliance reports it or no transmitter is built in, else IR
  DISPLAY_CONTROL_IR,    // always the IR remote, needs a transmitter
};

// On/off settings of the switch platform
enum class MideaSwitch : uint8_t {
  MIDEA_SWITCH_PURIFIER,
  MIDEA_SWITCH_FAHRENHEIT,
  MIDEA_SWITCH_JET_COOL,
  MIDEA_SWITCH_BREEZE_AWAY,
  MIDEA_SWITCH_BREEZELESS,
  MIDEA_SWITCH_OUT_SILENT,
  MIDEA_SWITCH_IECO,
  MIDEA_SWITCH_SOUND,
};

// Settings of the select platform; the options follow the value tables in air_conditioner.cpp
enum class MideaSelect : uint8_t {
  MIDEA_SELECT_AUX_HEAT_MODE,
  MIDEA_SELECT_VERTICAL_SWING_ANGLE,
  MIDEA_SELECT_HORIZONTAL_SWING_ANGLE,
  MIDEA_SELECT_CASCADE,
  MIDEA_SELECT_FRESH_AIR,
  MIDEA_SELECT_RATE_SELECT,
  MIDEA_SELECT_BREEZE_MODE,
  MIDEA_SELECT_BREEZE_AWAY_DIRECTION,
};

// Offsets below follow the payload layout documented by mill1000/midea-msmart (msmart/device/AC/command.py):
// byte 0 is the message ID (0xC0, 0xC1, 0xB0, 0xB1), the last byte is the CRC8 and the one before it the
// message counter.

/// Optional C1 group data reports. Each is polled only when an entity that needs it is configured.
enum class MideaGroup : uint8_t {
  MIDEA_GROUP_1 = 0,   // compressor and refrigerant circuit
  MIDEA_GROUP_2 = 1,   // indoor fan and condensate pump
  MIDEA_GROUP_4 = 2,   // energy
  MIDEA_GROUP_5 = 3,   // indoor humidity, outdoor fan, defrost
  MIDEA_GROUP_7 = 4,   // outdoor unit power
  MIDEA_GROUP_11 = 5,  // louver angles
  MIDEA_GROUP_COUNT = 6,
};

/// B0/B1 properties. Each is polled only when an entity that needs it is configured.
enum class MideaProperty : uint8_t {
  MIDEA_PROPERTY_SWING_UD_ANGLE = 0,
  MIDEA_PROPERTY_SWING_LR_ANGLE,
  MIDEA_PROPERTY_BREEZELESS,
  MIDEA_PROPERTY_SELF_CLEAN,
  MIDEA_PROPERTY_BREEZE_AWAY,
  MIDEA_PROPERTY_BREEZE_CONTROL,
  MIDEA_PROPERTY_RATE_SELECT,
  MIDEA_PROPERTY_FRESH_AIR,
  MIDEA_PROPERTY_BREEZE_AWAY_DIRECTION,
  MIDEA_PROPERTY_CASCADE,
  MIDEA_PROPERTY_JET_COOL,
  MIDEA_PROPERTY_OUT_SILENT,
  MIDEA_PROPERTY_IECO,
  MIDEA_PROPERTY_SOUND,
  MIDEA_PROPERTY_COUNT,
};

/// Auxiliary heater modes, in the order of the aux heat select options
enum class MideaAuxHeatMode : uint8_t {
  MIDEA_AUX_HEAT_MODE_OFF = 0,
  MIDEA_AUX_HEAT_MODE_AUX_HEAT,
  MIDEA_AUX_HEAT_MODE_AUX_ONLY,
};

/// How group 4 encodes energy values; differs between models
enum class MideaEnergyFormat : uint8_t {
  MIDEA_ENERGY_FORMAT_BCD = 0,
  MIDEA_ENERGY_FORMAT_BINARY,
};

/// Status controls not yet confirmed by a status report, sent with the next 0x40 set status request
struct PendingStatus {
  enum : uint8_t {
    FAN_SPEED = 1 << 0,
    PURIFIER = 1 << 1,
    FAHRENHEIT = 1 << 2,
    TARGET_HUMIDITY = 1 << 3,
    AUX_HEAT_MODE = 1 << 4,
  };
  uint8_t fields{0};  // which of the values below are set
  uint8_t fan_speed{0};
  uint8_t target_humidity{0};
  bool purifier{false};
  bool fahrenheit{false};
  MideaAuxHeatMode aux_heat_mode{MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_OFF};
};

/// Values MideaUART does not track. Floats are NAN and tri-state flags -1 until reported.
struct ApplianceState {
  // 0xC0 status
  uint8_t fan_speed{0};  // raw: 1-100 percent, 102 auto
  uint8_t error_code{0};
  uint8_t target_humidity{0};
  bool status_valid{false};
  bool filter_alert{false};
  bool display_on{false};
  bool follow_me{false};
  bool purifier{false};
  bool fahrenheit{false};
  MideaAuxHeatMode aux_heat_mode{MideaAuxHeatMode::MIDEA_AUX_HEAT_MODE_OFF};
  // Group 1
  float compressor_frequency{NAN};
  float compressor_target_frequency{NAN};
  float compressor_current{NAN};
  float compressor_voltage{NAN};
  float indoor_coil_temperature{NAN};
  float outdoor_coil_temperature{NAN};
  float discharge_temperature{NAN};
  // Group 2
  float indoor_fan_speed{NAN};
  float indoor_fan_target_speed{NAN};
  int8_t water_pump{-1};
  // Group 4
  float total_energy{NAN};
  float current_energy{NAN};
  float real_time_power{NAN};
  // Group 5
  float indoor_humidity{NAN};
  float outdoor_fan_speed{NAN};
  int8_t defrost{-1};
  // Group 7
  float outdoor_unit_power{NAN};
  // Group 11
  float horizontal_louver_angle{NAN};
  float vertical_louver_angle{NAN};
};

/// MideaUART air conditioner with the group data, properties and status fields of midea-msmart
class Appliance : public dudanov::midea::ac::AirConditioner {
 public:
  /* Configuration, before setup() */
  void request_group(MideaGroup group) { this->polled_groups_ |= 1U << static_cast<uint8_t>(group); }
  void request_property(MideaProperty prop) { this->polled_properties_ |= 1U << static_cast<uint8_t>(prop); }
  void set_energy_format(MideaEnergyFormat format) { this->energy_format_ = format; }
  void set_ieco_number(uint8_t number) { this->ieco_number_ = number; }

  /* State */
  const ApplianceState &get_appliance_state() const { return this->state_; }
  bool has_property(MideaProperty prop) const { return this->known_properties_ & (1U << static_cast<uint8_t>(prop)); }
  uint8_t get_property(MideaProperty prop) const { return this->property_values_[static_cast<uint8_t>(prop)]; }

  /* Climate controls; use these instead of control() and setPowerState(), so they are sent in order with the
   * controls below */
  void request_control(const dudanov::midea::ac::Control &control);
  void request_power(bool state);

  /* Controls sent with the 0x40 set status request */
  void set_fan_speed(uint8_t speed);
  void set_purifier(bool state);
  void set_fahrenheit(bool state);
  void set_target_humidity(uint8_t humidity);
  void set_aux_heat_mode(MideaAuxHeatMode mode);

  /* Controls sent with the 0xB0 set properties request */
  void set_property(MideaProperty prop, uint8_t value);
  void start_self_clean() { this->set_property(MideaProperty::MIDEA_PROPERTY_SELF_CLEAN, 1); }

 protected:
  void m_onIdle() override;
  void queue_status_query_();
  bool queue_next_data_query_();
  void queue_group_query_(MideaGroup group);
  void queue_properties_query_();

  dudanov::midea::ResponseStatus read_status_(dudanov::midea::FrameData data);
  bool read_status_fields_(const dudanov::midea::FrameData &data);
  dudanov::midea::ResponseStatus read_group_(MideaGroup group, const dudanov::midea::FrameData &data);
  dudanov::midea::ResponseStatus read_properties_(const dudanov::midea::FrameData &data);

  bool queue_next_control_();
  void send_climate_control_(const dudanov::midea::ac::Control &control);
  bool status_ready_() const;
  void set_pending_status_(uint8_t field);
  void queue_status_control_();
  void on_status_control_done_();
  void queue_properties_control_();

  ApplianceState state_{};
  PendingStatus pending_status_{};
  dudanov::midea::ac::Control deferred_control_{};  // climate controls held while a control request is out
  uint8_t pending_property_values_[static_cast<uint8_t>(MideaProperty::MIDEA_PROPERTY_COUNT)]{};
  uint16_t pending_properties_{0};  // properties to write with the next 0xB0 set properties request
  bool status_dirty_{false};        // pending_status_ changed since the last 0x40 request was built
  bool control_in_flight_{false};   // a 0x40 or 0xB0 request of these controls is queued or awaiting its reply
  bool has_deferred_control_{false};
  uint8_t property_values_[static_cast<uint8_t>(MideaProperty::MIDEA_PROPERTY_COUNT)]{};
  uint16_t known_properties_{0};
  uint16_t polled_properties_{0};
  uint16_t failed_properties_{0};  // properties whose failed poll result was already logged
  uint8_t polled_groups_{0};
  uint8_t short_groups_{0};  // groups whose short reply was already logged
  // Index of the next group or properties query; these alternate with the status query
  uint8_t next_query_{0};
  bool status_turn_{true};
  MideaEnergyFormat energy_format_{MideaEnergyFormat::MIDEA_ENERGY_FORMAT_BCD};
  uint8_t ieco_number_{1};
};

class AirConditioner final : public ApplianceBase<Appliance>, public climate::Climate {
  SUB_SENSOR(outdoor_temperature)
  SUB_SENSOR(humidity_setpoint)
  SUB_SENSOR(power)
  // Sensor platform
  SUB_SENSOR(error_code)
  SUB_SENSOR(compressor_frequency)
  SUB_SENSOR(compressor_target_frequency)
  SUB_SENSOR(compressor_current)
  SUB_SENSOR(compressor_voltage)
  SUB_SENSOR(indoor_coil_temperature)
  SUB_SENSOR(outdoor_coil_temperature)
  SUB_SENSOR(discharge_temperature)
  SUB_SENSOR(indoor_fan_speed)
  SUB_SENSOR(indoor_fan_target_speed)
  SUB_SENSOR(total_energy)
  SUB_SENSOR(current_energy)
  SUB_SENSOR(real_time_power)
  SUB_SENSOR(indoor_humidity)
  SUB_SENSOR(outdoor_fan_speed)
  SUB_SENSOR(outdoor_unit_power)
  SUB_SENSOR(horizontal_louver_angle)
  SUB_SENSOR(vertical_louver_angle)
#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(defrost)
  SUB_BINARY_SENSOR(water_pump)
  SUB_BINARY_SENSOR(filter_alert)
  SUB_BINARY_SENSOR(display)
  SUB_BINARY_SENSOR(follow_me)
  SUB_BINARY_SENSOR(self_cleaning)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(purifier)
  SUB_SWITCH(fahrenheit)
  SUB_SWITCH(jet_cool)
  SUB_SWITCH(breeze_away)
  SUB_SWITCH(breezeless)
  SUB_SWITCH(out_silent)
  SUB_SWITCH(ieco)
  SUB_SWITCH(sound)
#endif
#ifdef USE_SELECT
  SUB_SELECT(aux_heat_mode)
  SUB_SELECT(vertical_swing_angle)
  SUB_SELECT(horizontal_swing_angle)
  SUB_SELECT(cascade)
  SUB_SELECT(fresh_air)
  SUB_SELECT(rate_select)
  SUB_SELECT(breeze_mode)
  SUB_SELECT(breeze_away_direction)
#endif
#ifdef USE_NUMBER
  SUB_NUMBER(fan_speed)
  SUB_NUMBER(target_humidity)
#endif

 public:
  void setup() override;
  void dump_config() override;
  void set_display_control(DisplayControl control) { this->display_control_ = control; }
  void on_status_change() override;

  /* ############### */
  /* ### ACTIONS ### */
  /* ############### */

  void do_follow_me(float temperature, bool use_fahrenheit, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  void do_beeper_on() { this->set_beeper_feedback(true); }
  void do_beeper_off() { this->set_beeper_feedback(false); }
  void do_power_on() { this->base_.request_power(true); }
  void do_power_off() { this->base_.request_power(false); }
  void do_power_toggle() { this->base_.request_power(this->mode == ClimateMode::CLIMATE_MODE_OFF); }
  void set_supported_modes(ClimateModeMask modes) { this->supported_modes_ = modes; }
  void set_supported_swing_modes(ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }
  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }

  /* Sensor, binary_sensor, switch, select, number and button platforms */
  void set_energy_format(MideaEnergyFormat format) { this->base_.set_energy_format(format); }
  void set_ieco_number(uint8_t number) { this->base_.set_ieco_number(number); }
  void set_rate_select_levels(uint8_t levels) { this->rate_select_levels_ = levels; }
  void control_switch(MideaSwitch setting, bool state);
  void control_select(MideaSelect setting, size_t index);
  void control_fan_speed(float speed) { this->base_.set_fan_speed(static_cast<uint8_t>(std::lround(speed))); }
  void control_target_humidity(float humidity) {
    this->base_.set_target_humidity(static_cast<uint8_t>(std::lround(humidity)));
  }
  void do_self_clean() { this->base_.start_self_clean(); }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ClimateModeMask supported_modes_{};
  ClimateSwingModeMask supported_swing_modes_{};
  ClimatePresetMask supported_presets_{};
  void request_appliance_data_();
  void publish_appliance_state_();
  bool frost_protection_set_{false};
  uint8_t rate_select_levels_{5};
  // Last reported value without a select option, per MideaSelect, so it is logged once
  uint8_t unmapped_values_[8]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  DisplayControl display_control_{DisplayControl::DISPLAY_CONTROL_AUTO};
};

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
