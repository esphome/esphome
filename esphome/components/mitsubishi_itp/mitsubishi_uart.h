#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "muart_mhk.h"
#include "muart_packet.h"
#include "muart_bridge.h"
#include <map>

namespace esphome {
namespace mitsubishi_itp {

static constexpr char TAG[] = "mitsubishi_itp";

const uint8_t MUART_MIN_TEMP = 16;  // Degrees C
const uint8_t MUART_MAX_TEMP = 31;  // Degrees C
const float MUART_TEMPERATURE_STEP = 0.5;

const std::string FAN_MODE_VERYHIGH = "Very High";

const std::string TEMPERATURE_SOURCE_INTERNAL = "Internal";
const uint32_t TEMPERATURE_SOURCE_TIMEOUT_MS = 420000;  // (7min) The heatpump will revert on its own in ~10min

const std::string TEMPERATURE_SOURCE_THERMOSTAT = "Thermostat";

// These are named to match with set fan speeds where possible.  "Very Low" is a special speed
// for e.g. preheating or thermal off
const std::array<std::string, 7> ACTUAL_FAN_SPEED_NAMES = {"Off",  "Very Low",        "Low",  "Medium",
                                                           "High", FAN_MODE_VERYHIGH, "Quiet"};

const std::array<std::string, 5> THERMOSTAT_BATTERY_STATE_NAMES = {"OK", "Low", "Critical", "Replace", "Unknown"};

class MitsubishiUART : public PollingComponent, public climate::Climate, public PacketProcessor {
 public:
  /**
   * Create a new MitsubishiUART with the specified esphome::uart::UARTComponent.
   */
  MitsubishiUART(uart::UARTComponent *hp_uart_comp);

  // Used to restore state of previous MUART-specific settings (like temperature source or pass-thru mode)
  // Most other climate-state is preserved by the heatpump itself and will be retrieved after connection
  void setup() override;

  // Called repeatedly (used for UART receiving/forwarding)
  void loop() override;

  // Called periodically as PollingComponent (used for UART sending periodically)
  void update() override;

  // Returns default traits for MUART
  climate::ClimateTraits traits() override { return climate_traits_; }

  // Returns a reference to traits for MUART to be used during configuration
  // TODO: Maybe replace this with specific functions for the traits needed in configuration (a la the override
  // fuctions)
  climate::ClimateTraits &config_traits() { return climate_traits_; }

  // Dumps some configuration data that we may have missed in the real-time logs
  void dump_config() override;

  // Called to instruct a change of the climate controls
  void control(const climate::ClimateCall &call) override;

  // Set thermostat UART component
  void set_thermostat_uart(uart::UARTComponent *uart);

  // Sensor setters
  void set_thermostat_temperature_sensor(sensor::Sensor *sensor) { thermostat_temperature_sensor_ = sensor; };
  void set_outdoor_temperature_sensor(sensor::Sensor *sensor) { outdoor_temperature_sensor_ = sensor; };
  void set_thermostat_humidity_sensor(sensor::Sensor *sensor) { thermostat_humidity_sensor_ = sensor; }
  void set_compressor_frequency_sensor(sensor::Sensor *sensor) { compressor_frequency_sensor_ = sensor; };
  void set_actual_fan_sensor(text_sensor::TextSensor *sensor) { actual_fan_sensor_ = sensor; };
  void set_filter_status_sensor(binary_sensor::BinarySensor *sensor) { filter_status_sensor_ = sensor; };
  void set_defrost_sensor(binary_sensor::BinarySensor *sensor) { defrost_sensor_ = sensor; };
  void set_preheat_sensor(binary_sensor::BinarySensor *sensor) { preheat_sensor_ = sensor; };
  void set_standby_sensor(binary_sensor::BinarySensor *sensor) { standby_sensor_ = sensor; };
  void set_isee_status_sensor(binary_sensor::BinarySensor *sensor) { isee_status_sensor_ = sensor; }
  void set_error_code_sensor(text_sensor::TextSensor *sensor) { error_code_sensor_ = sensor; };
  void set_thermostat_battery_sensor(text_sensor::TextSensor *sensor) { thermostat_battery_sensor_ = sensor; }

  // Select setters
  void set_temperature_source_select(select::Select *select) { temperature_source_select_ = select; };
  void set_vane_position_select(select::Select *select) { vane_position_select_ = select; };
  void set_horizontal_vane_position_select(select::Select *select) { horizontal_vane_position_select_ = select; };

  // Returns true if select was valid (even if not yet successful) to indicate select component
  // should optimistically publish
  bool select_temperature_source(const std::string &state);
  bool select_vane_position(const std::string &state);
  bool select_horizontal_vane_position(const std::string &state);

  // Used by external sources to report a temperature
  void temperature_source_report(const std::string &temperature_source, const float &v);

  // Button triggers
  void reset_filter_status();

  // Turns on or off actively sending packets
  void set_active_mode(const bool active) { active_mode_ = active; };

  // Turns on or off Kumo emulation mode
  void set_enhanced_mhk_support(const bool mode) { enhanced_mhk_support_ = mode; }

  void set_time_source(time::RealTimeClock *rtc) { time_source_ = rtc; }

 protected:
  void route_packet_(const Packet &packet);

  void process_packet(const Packet &packet) override;
  void process_packet(const ConnectRequestPacket &packet) override;
  void process_packet(const ConnectResponsePacket &packet) override;
  void process_packet(const CapabilitiesRequestPacket &packet) override;
  void process_packet(const CapabilitiesResponsePacket &packet) override;
  void process_packet(const GetRequestPacket &packet) override;
  void process_packet(const SettingsGetResponsePacket &packet) override;
  void process_packet(const CurrentTempGetResponsePacket &packet) override;
  void process_packet(const StatusGetResponsePacket &packet) override;
  void process_packet(const RunStateGetResponsePacket &packet) override;
  void process_packet(const ErrorStateGetResponsePacket &packet) override;
  void process_packet(const SettingsSetRequestPacket &packet) override;
  void process_packet(const RemoteTemperatureSetRequestPacket &packet) override;
  void process_packet(const ThermostatSensorStatusPacket &packet) override;
  void process_packet(const ThermostatHelloPacket &packet) override;
  void process_packet(const ThermostatStateUploadPacket &packet) override;
  void process_packet(const ThermostatAASetRequestPacket &packet) override;
  void process_packet(const SetResponsePacket &packet) override;

  void handle_thermostat_state_download_request(const GetRequestPacket &packet) override;
  void handle_thermostat_ab_get_request(const GetRequestPacket &packet) override;

  void do_publish_();

 private:
  // Default climate_traits for MUART
  climate::ClimateTraits climate_traits_ = []() -> climate::ClimateTraits {
    climate::ClimateTraits ct = climate::ClimateTraits();

    ct.set_supports_action(true);
    ct.set_supports_current_temperature(true);
    ct.set_supports_two_point_target_temperature(false);
    ct.set_visual_min_temperature(MUART_MIN_TEMP);
    ct.set_visual_max_temperature(MUART_MAX_TEMP);
    ct.set_visual_temperature_step(MUART_TEMPERATURE_STEP);

    return ct;
  }();

  // UARTComponent connected to heatpump
  const uart::UARTComponent &hp_uart_;
  // UART packet wrapper for heatpump
  HeatpumpBridge hp_bridge_;
  // UARTComponent connected to thermostat
  uart::UARTComponent *ts_uart_ = nullptr;
  // UART packet wrapper for heatpump
  std::unique_ptr<ThermostatBridge> ts_bridge_ = nullptr;

  // Are we connected to the heatpump?
  bool hp_connected_ = false;
  // Should we call publish on the next update?
  bool publish_on_update_ = false;
  // Are we still discovering information about the device?
  bool in_discovery_ = true;
  // Number of times update() has been called in discovery mode
  size_t discovery_updates_ = 0;

  optional<CapabilitiesResponsePacket> capabilities_cache_;
  bool capabilities_requested_ = false;
  // Have we received at least one RunState response?
  bool run_state_received_ = false;

  // Preferences
  void save_preferences_();
  void restore_preferences_();

  ESPPreferenceObject preferences_;

  // Time Source
  time::RealTimeClock *time_source_ = nullptr;

  // Internal sensors
  sensor::Sensor *thermostat_temperature_sensor_ = nullptr;
  sensor::Sensor *thermostat_humidity_sensor_ = nullptr;
  sensor::Sensor *compressor_frequency_sensor_ = nullptr;
  sensor::Sensor *outdoor_temperature_sensor_ = nullptr;
  text_sensor::TextSensor *actual_fan_sensor_ = nullptr;
  binary_sensor::BinarySensor *filter_status_sensor_ = nullptr;
  binary_sensor::BinarySensor *defrost_sensor_ = nullptr;
  binary_sensor::BinarySensor *preheat_sensor_ = nullptr;
  binary_sensor::BinarySensor *standby_sensor_ = nullptr;
  binary_sensor::BinarySensor *isee_status_sensor_ = nullptr;
  text_sensor::TextSensor *error_code_sensor_ = nullptr;
  text_sensor::TextSensor *thermostat_battery_sensor_ = nullptr;

  // Selects
  select::Select *temperature_source_select_;
  select::Select *vane_position_select_;
  select::Select *horizontal_vane_position_select_;

  // Temperature select extras
  std::map<std::string, size_t> temp_select_map_;  // Used to map strings to indexes for preference storage
  std::string current_temperature_source_ = TEMPERATURE_SOURCE_INTERNAL;
  uint32_t last_received_temperature_ = millis();

  void send_if_active_(const Packet &packet);
  bool active_mode_ = true;

  // used to track whether to support/handle the enhanced MHK protocol packets
  bool enhanced_mhk_support_ = false;

  MHKState mhk_state_;
};

struct MUARTPreferences {
  optional<size_t> currentTemperatureSourceIndex = nullopt;  // Index of selected value
  // optional<uint32_t> currentTemperatureSourceHash = nullopt; // Hash of selected value (to make sure it hasn't
  // changed since last save)
};

}  // namespace mitsubishi_itp
}  // namespace esphome
