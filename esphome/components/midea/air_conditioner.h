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

// Offsets below follow the payload layout documented by mill1000/midea-msmart (msmart/device/AC/command.py):
// byte 0 is the message ID (0xC0, 0xC1), the last byte is the CRC8 and the one before it the
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

/// How group 4 encodes energy values; differs between models
enum class MideaEnergyFormat : uint8_t {
  MIDEA_ENERGY_FORMAT_BCD = 0,
  MIDEA_ENERGY_FORMAT_BINARY,
};

/// Values MideaUART does not track. Floats are NAN and tri-state flags -1 until reported.
struct ApplianceState {
  // 0xC0 status
  uint8_t error_code{0};
  bool status_valid{false};
  bool filter_alert{false};
  bool display_on{false};
  bool follow_me{false};
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

/// MideaUART air conditioner with the group data and status fields of midea-msmart
class Appliance : public dudanov::midea::ac::AirConditioner {
 public:
  /* Configuration, before setup() */
  void request_group(MideaGroup group) { this->polled_groups_ |= 1U << static_cast<uint8_t>(group); }
  void set_energy_format(MideaEnergyFormat format) { this->energy_format_ = format; }

  /* State */
  const ApplianceState &get_appliance_state() const { return this->state_; }

 protected:
  void m_onIdle() override;
  void queue_status_query_();
  bool queue_next_data_query_();
  void queue_group_query_(MideaGroup group);

  bool read_status_fields_(const dudanov::midea::FrameData &data);
  dudanov::midea::ResponseStatus read_group_(MideaGroup group, const dudanov::midea::FrameData &data);

  ApplianceState state_{};
  uint8_t polled_groups_{0};
  // Index of the next group query; group queries alternate with the status query
  uint8_t next_query_{0};
  bool status_turn_{true};
  MideaEnergyFormat energy_format_{MideaEnergyFormat::MIDEA_ENERGY_FORMAT_BCD};
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
  void do_power_on() { this->base_.setPowerState(true); }
  void do_power_off() { this->base_.setPowerState(false); }
  void do_power_toggle() { this->base_.setPowerState(this->mode == ClimateMode::CLIMATE_MODE_OFF); }
  void set_supported_modes(ClimateModeMask modes) { this->supported_modes_ = modes; }
  void set_supported_swing_modes(ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }
  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }

  /* Sensor and binary_sensor platforms */
  void set_energy_format(MideaEnergyFormat format) { this->base_.set_energy_format(format); }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ClimateModeMask supported_modes_{};
  ClimateSwingModeMask supported_swing_modes_{};
  ClimatePresetMask supported_presets_{};
  void request_appliance_data_();
  void publish_appliance_state_();
  bool frost_protection_set_{false};
  DisplayControl display_control_{DisplayControl::DISPLAY_CONTROL_AUTO};
};

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
