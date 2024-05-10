#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/uart/uart.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/select/select.h"
#include "esphome/components/sensor/sensor.h"
#include "muart_packet.h"
#include "muart_bridge.h"
#include <map>

namespace esphome {
namespace mitsubishi_uart {

static const char *TAG = "mitsubishi_uart";

const uint8_t MUART_MIN_TEMP = 16;  // Degrees C
const uint8_t MUART_MAX_TEMP = 31;  // Degrees C
const float MUART_TEMPERATURE_STEP = 0.5;

const std::string FAN_MODE_VERYHIGH = "Very High";

const std::string TEMPERATURE_SOURCE_INTERNAL = "Internal";
const uint32_t TEMPERATURE_SOURCE_TIMEOUT_MS = 420000;  // (7min) The heatpump will revert on its own in ~10min

const std::string TEMPERATURE_SOURCE_THERMOSTAT = "Thermostat";

// these names come from Kumo. They are bad, but I am also too lazy to think of better names. they also
// may not map perfectly yet?
const std::array<std::string, 7> ACTUAL_FAN_SPEED_NAMES = {"Off",      "Very Low",       "Quiet",      "Low",
                                                           "Powerful", "Super Powerful", "Super Quiet"};

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
  void dump_config();

  // Called to instruct a change of the climate controls
  void control(const climate::ClimateCall &call) override;

  // Set thermostat UART component
  void set_thermostat_uart(uart::UARTComponent *uart) {
    ESP_LOGCONFIG(TAG, "Thermostat uart was set.");
    ts_uart = uart;
    ts_bridge = new ThermostatBridge(ts_uart, static_cast<PacketProcessor *>(this));
  }

  // Sensor setters
  void set_thermostat_temperature_sensor(sensor::Sensor *sensor) { thermostat_temperature_sensor = sensor; };
  void set_compressor_frequency_sensor(sensor::Sensor *sensor) { compressor_frequency_sensor = sensor; };
  void set_actual_fan_sensor(text_sensor::TextSensor *sensor) { actual_fan_sensor = sensor; };
  void set_service_filter_sensor(binary_sensor::BinarySensor *sensor) { service_filter_sensor = sensor; };
  void set_defrost_sensor(binary_sensor::BinarySensor *sensor) { defrost_sensor = sensor; };
  void set_hot_adjust_sensor(binary_sensor::BinarySensor *sensor) { hot_adjust_sensor = sensor; };
  void set_standby_sensor(binary_sensor::BinarySensor *sensor) { standby_sensor = sensor; };
  void set_error_code_sensor(text_sensor::TextSensor *sensor) { error_code_sensor = sensor; };

  // Select setters
  void set_temperature_source_select(select::Select *select) { temperature_source_select = select; };
  void set_vane_position_select(select::Select *select) { vane_position_select = select; };
  void set_horizontal_vane_position_select(select::Select *select) { horizontal_vane_position_select = select; };

  // Returns true if select was valid (even if not yet successful) to indicate select component
  // should optimistically publish
  bool select_temperature_source(const std::string &state);
  bool select_vane_position(const std::string &state);
  bool select_horizontal_vane_position(const std::string &state);

  // Used by external sources to report a temperature
  void temperature_source_report(const std::string &temperature_source, const float &v);

  // Turns on or off actively sending packets
  void set_active_mode(const bool active) { active_mode = active; };

 protected:
  void routePacket(const Packet &packet);

  void processPacket(const Packet &packet);
  void processPacket(const ConnectRequestPacket &packet);
  void processPacket(const ConnectResponsePacket &packet);
  void processPacket(const ExtendedConnectRequestPacket &packet);
  void processPacket(const ExtendedConnectResponsePacket &packet);
  void processPacket(const GetRequestPacket &packet);
  void processPacket(const SettingsGetResponsePacket &packet);
  void processPacket(const CurrentTempGetResponsePacket &packet);
  void processPacket(const StatusGetResponsePacket &packet);
  void processPacket(const StandbyGetResponsePacket &packet);
  void processPacket(const ErrorStateGetResponsePacket &packet);
  void processPacket(const RemoteTemperatureSetRequestPacket &packet);
  void processPacket(const SetResponsePacket &packet);

  void doPublish();

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
  const uart::UARTComponent &hp_uart;
  // UART packet wrapper for heatpump
  HeatpumpBridge hp_bridge;
  // UARTComponent connected to thermostat
  uart::UARTComponent *ts_uart = nullptr;
  // UART packet wrapper for heatpump
  ThermostatBridge *ts_bridge = nullptr;

  // Are we connected to the heatpump?
  bool hpConnected = false;
  // Should we call publish on the next update?
  bool publishOnUpdate = false;

  optional<ExtendedConnectResponsePacket> _capabilitiesCache;
  bool _capabilitiesRequested = false;

  // Preferences
  void save_preferences();
  void restore_preferences();

  ESPPreferenceObject preferences_;

  // Internal sensors
  sensor::Sensor *thermostat_temperature_sensor = nullptr;
  sensor::Sensor *compressor_frequency_sensor = nullptr;
  text_sensor::TextSensor *actual_fan_sensor = nullptr;
  binary_sensor::BinarySensor *service_filter_sensor = nullptr;
  binary_sensor::BinarySensor *defrost_sensor = nullptr;
  binary_sensor::BinarySensor *hot_adjust_sensor = nullptr;
  binary_sensor::BinarySensor *standby_sensor = nullptr;
  text_sensor::TextSensor *error_code_sensor = nullptr;

  // Selects
  select::Select *temperature_source_select;
  select::Select *vane_position_select;
  select::Select *horizontal_vane_position_select;

  // Temperature select extras
  std::map<std::string, size_t> temp_select_map;  // Used to map strings to indexes for preference storage
  std::string currentTemperatureSource = TEMPERATURE_SOURCE_INTERNAL;
  uint32_t lastReceivedTemperature = millis();

  void sendIfActive(const Packet &packet);
  bool active_mode = true;
};

struct MUARTPreferences {
  optional<size_t> currentTemperatureSourceIndex = nullopt;  // Index of selected value
  // optional<uint32_t> currentTemperatureSourceHash = nullopt; // Hash of selected value (to make sure it hasn't
  // changed since last save)
};

}  // namespace mitsubishi_uart
}  // namespace esphome
