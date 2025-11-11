#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include <vector>
#include <array>

namespace esphome {
namespace sonoff_spm {

// Forward declarations
class SonoffSPMSwitch;
class SonoffSPMSensor;

// Protocol constants
static const uint8_t SPM_MAX_MODULES = 32;  // Max 32 SPM-4Relay modules
static const uint8_t SPM_CHANNELS_PER_MODULE = 4;
static const uint8_t SPM_MAX_RELAYS = SPM_MAX_MODULES * SPM_CHANNELS_PER_MODULE;  // 128 relays
static const size_t SPM_SERIAL_BUFFER_SIZE = 548;
static const uint8_t SPM_MODULE_NAME_SIZE = 12;

// Commands from ESP to ARM
static const uint8_t SPM_FUNC_FIND = 0x00;
static const uint8_t SPM_FUNC_SET_RELAY = 0x08;
static const uint8_t SPM_FUNC_GET_MODULE_STATE = 0x09;
static const uint8_t SPM_FUNC_SET_TIME = 0x0C;
static const uint8_t SPM_FUNC_INIT_SCAN = 0x10;
static const uint8_t SPM_FUNC_GET_MAIN_VERSION = 0x15;
static const uint8_t SPM_FUNC_GET_ENERGY_TOTAL = 0x16;
static const uint8_t SPM_FUNC_GET_ENERGY = 0x18;

// Commands from ARM to ESP
static const uint8_t SPM_FUNC_ENERGY_RESULT = 0x06;
static const uint8_t SPM_FUNC_KEY_PRESS = 0x07;
static const uint8_t SPM_FUNC_SCAN_START = 0x0F;
static const uint8_t SPM_FUNC_SCAN_RESULT = 0x13;
static const uint8_t SPM_FUNC_SCAN_DONE = 0x19;

// State machine states
enum SPMState : uint8_t {
  SPM_STATE_IDLE = 0,
  SPM_STATE_RESET,
  SPM_STATE_WAIT_VERSION,
  SPM_STATE_START_SCAN,
  SPM_STATE_SCANNING,
  SPM_STATE_GET_STATES,
  SPM_STATE_RUNNING,
};

// Module information
struct ModuleInfo {
  std::array<uint8_t, SPM_MODULE_NAME_SIZE> module_id{};
  uint32_t firmware_version{0};
  bool online{false};
};

// Energy data per channel
struct ChannelData {
  float voltage{0.0f};
  float current{0.0f};
  float active_power{0.0f};
  float reactive_power{0.0f};
  float apparent_power{0.0f};
  float power_factor{0.0f};
  float energy_today{0.0f};
  float energy_total{0.0f};
  bool relay_state{false};
};

class SonoffSPM : public Component, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  // Configuration
  void set_module_count(uint8_t count) { this->module_count_ = count; }

  // Switch registration
  void register_switch(SonoffSPMSwitch *switch_obj, uint8_t relay_id);

  // Sensor registration
  void register_sensor(SonoffSPMSensor *sensor, uint8_t relay_id);

  // Switch control
  void set_relay_state(uint8_t relay_id, bool state);

  // Get channel data
  const ChannelData *get_channel_data(uint8_t relay_id) const;

  // Get relay state
  bool get_relay_state(uint8_t relay_id) const;

 protected:
  // Protocol functions
  void send_command_(uint8_t command);
  void send_find_();
  void send_init_scan_();
  void send_get_main_version_();
  void send_set_relay_(uint8_t relay_id, bool state);
  void send_get_module_state_(uint8_t module);
  void send_ack_(uint8_t sequence);

  // Message handling
  void process_serial_();
  void handle_received_data_();
  void handle_energy_result_();
  void handle_key_press_();
  void handle_scan_result_();
  void handle_scan_done_();
  void handle_module_state_();

  // Utility functions
  uint16_t calculate_crc_(const uint8_t *data, size_t len);
  void init_send_();
  void send_buffer_(size_t size);
  float get_value_(const uint8_t *buffer);
  void set_value_(uint8_t *buffer, float value);

  // State management
  void update_state_machine_();
  void start_scan_();
  void request_energy_updates_();

  // Data members
  SPMState state_{SPM_STATE_IDLE};
  uint8_t module_count_{SPM_MAX_MODULES};
  uint8_t command_sequence_{0};
  uint8_t scanned_modules_{0};
  uint8_t current_module_{0};
  uint32_t main_version_{0};
  uint32_t last_scan_time_{0};
  uint32_t last_energy_request_{0};
  uint16_t expected_bytes_{0};
  uint16_t serial_in_byte_counter_{0};

  // Buffers and data structures
  std::array<uint8_t, SPM_SERIAL_BUFFER_SIZE> buffer_{};
  std::array<ModuleInfo, SPM_MAX_MODULES> modules_{};
  std::array<ChannelData, SPM_MAX_RELAYS> channels_{};

  // Switch and sensor registration
  std::vector<SonoffSPMSwitch *> switches_{};
  std::vector<SonoffSPMSensor *> sensors_{};
};

}  // namespace sonoff_spm
}  // namespace esphome
