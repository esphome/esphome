#pragma once

#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#endif
#include "esphome/components/climate/climate.h"
#include "mitp_listener.h"
#include "mitp_packet.h"
#include "mitp_bridge.h"
#include "mitp_mhk.h"
#include <map>

namespace esphome {
namespace mitsubishi_itp {

static constexpr char TAG[] = "mitsubishi_itp";

const uint8_t MITP_MIN_TEMP = 16;  // Degrees C
const uint8_t MITP_MAX_TEMP = 31;  // Degrees C
const float MITP_TEMPERATURE_STEP = 0.5;

const std::string TEMPERATURE_SOURCE_INTERNAL = "Internal";
const std::string TEMPERATURE_SOURCE_THERMOSTAT = "Thermostat";

const uint32_t TEMPERATURE_SOURCE_TIMEOUT_MS = 420000;  // (7min) The heatpump will revert on its own in ~10min

const auto MAX_RECALL_MODE_INDEX = climate::ClimateMode::CLIMATE_MODE_DRY;

class MitsubishiUART : public PollingComponent, public climate::Climate, public PacketProcessor {
 public:
  /**
   * Create a new MitsubishiUART with the specified esphome::uart::UARTComponent.
   */
  MitsubishiUART(uart::UARTComponent *hp_uart_comp);

  // Used to restore state of previous MITP-specific settings (like temperature source or pass-thru mode)
  // Most other climate-state is preserved by the heatpump itself and will be retrieved after connection
  void setup() override;

  // Called repeatedly (used for UART receiving/forwarding)
  void loop() override;

  // Called periodically as PollingComponent (used for UART sending periodically)
  void update() override;

  // Returns default traits for MITP
  climate::ClimateTraits traits() override { return climate_traits_; }

  // Returns a reference to traits for MITP to be used during configuration
  // TODO: Maybe replace this with specific functions for the traits needed in configuration (a la the override
  // fuctions)
  climate::ClimateTraits &config_traits() { return climate_traits_; }

  // Dumps some configuration data that we may have missed in the real-time logs
  void dump_config() override;

  // Called to instruct a change of the climate controls
  void control(const climate::ClimateCall &call) override;

  // Set thermostat UART component
  void set_thermostat_uart(uart::UARTComponent *uart);

  // Listener-sensors
  void register_listener(MITPListener *listener) { this->listeners_.push_back(listener); }

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
  void set_enhanced_mhk_support(const bool supports) { enhanced_mhk_support_ = supports; }

  // Enables the recall setpoint feature
  void set_recall_setpoint(const bool enabled) { recall_setpoint_ = enabled; }

#ifdef USE_TIME
  void set_time_source(time::RealTimeClock *rtc) { time_source_ = rtc; }
#endif

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
  // Default climate_traits for MITP
  climate::ClimateTraits climate_traits_ = []() -> climate::ClimateTraits {
    climate::ClimateTraits ct = climate::ClimateTraits();

    ct.set_supports_action(true);
    ct.set_supports_current_temperature(true);
    ct.set_supports_two_point_target_temperature(false);
    ct.set_visual_min_temperature(MITP_MIN_TEMP);
    ct.set_visual_max_temperature(MITP_MAX_TEMP);
    ct.set_visual_temperature_step(MITP_TEMPERATURE_STEP);

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

// Time Source
#ifdef USE_TIME
  time::RealTimeClock *time_source_ = nullptr;
#endif

  // Listener-sensors
  std::vector<MITPListener *> listeners_{};
  template<typename T> void alert_listeners_(const T &packet) const {
    for (auto *listener : this->listeners_) {
      listener->process_packet(packet);
    }
  }

  // Temperature select extras
  std::string current_temperature_source_;
  uint32_t last_received_temperature_ = millis();
  bool temperature_source_timeout_ = false;  // Has the current source timed out?

  void send_if_active_(const Packet &packet);
  bool active_mode_ = true;

  // used to track whether to support/handle the enhanced MHK protocol packets
  bool enhanced_mhk_support_ = false;

  // If enabled, switching modes will recall target mode's previous setpoint
  bool recall_setpoint_ = false;
  // Array stores a float setpoint for each climate mode up to DRY.
  std::array<float, MAX_RECALL_MODE_INDEX + 1> mode_recall_setpoints_;

  MHKState mhk_state_;

  // Preferences
  void save_preferences_();
  void restore_preferences_();
  ESPPreferenceObject preferences_;
};

struct MITPPreferences {
  // Array stores a float setpoint for each climate mode up to DRY.
  std::array<float, MAX_RECALL_MODE_INDEX + 1> modeRecallSetpoints;
};

}  // namespace mitsubishi_itp
}  // namespace esphome
