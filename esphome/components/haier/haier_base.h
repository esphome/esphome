#pragma once

#include <chrono>
#include <set>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
// HaierProtocol
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

enum class ActionRequest : uint8_t {
  NO_ACTION = 0,
  TURN_POWER_ON = 1,
  TURN_POWER_OFF = 2,
  TOGGLE_POWER = 3,
  START_SELF_CLEAN = 4,   // only hOn
  START_STERI_CLEAN = 5,  // only hOn
};

class HaierClimateBase : public esphome::Component,
                         public esphome::climate::Climate,
                         public esphome::uart::UARTDevice,
                         public haier_protocol::ProtocolStream {
 public:
  HaierClimateBase();
  HaierClimateBase(const HaierClimateBase &) = delete;
  HaierClimateBase &operator=(const HaierClimateBase &) = delete;
  ~HaierClimateBase();
  void setup() override;
  void loop() override;
  void control(const esphome::climate::ClimateCall &call) override;
  void dump_config() override;
  float get_setup_priority() const override { return esphome::setup_priority::HARDWARE; }
  void set_fahrenheit(bool fahrenheit);
  void set_display_state(bool state);
  bool get_display_state() const;
  void set_health_mode(bool state);
  bool get_health_mode() const;
  void send_power_on_command();
  void send_power_off_command();
  void toggle_power();
  void reset_protocol() { this->reset_protocol_request_ = true; };
  void set_supported_modes(const std::set<esphome::climate::ClimateMode> &modes);
  void set_supported_swing_modes(const std::set<esphome::climate::ClimateSwingMode> &modes);
  void set_supported_presets(const std::set<esphome::climate::ClimatePreset> &presets);
  size_t available() noexcept override { return esphome::uart::UARTDevice::available(); };
  size_t read_array(uint8_t *data, size_t len) noexcept override {
    return esphome::uart::UARTDevice::read_array(data, len) ? len : 0;
  };
  void write_array(const uint8_t *data, size_t len) noexcept override {
    esphome::uart::UARTDevice::write_array(data, len);
  };
  bool can_send_message() const { return haier_protocol_.get_outgoing_queue_size() == 0; };
  void set_answer_timeout(uint32_t timeout);
  void set_send_wifi(bool send_wifi);

 protected:
  enum class ProtocolPhases {
    UNKNOWN = -1,
    // INITIALIZATION
    SENDING_INIT_1 = 0,
    WAITING_INIT_1_ANSWER = 1,
    SENDING_INIT_2 = 2,
    WAITING_INIT_2_ANSWER = 3,
    SENDING_FIRST_STATUS_REQUEST = 4,
    WAITING_FIRST_STATUS_ANSWER = 5,
    SENDING_ALARM_STATUS_REQUEST = 6,
    WAITING_ALARM_STATUS_ANSWER = 7,
    // FUNCTIONAL STATE
    IDLE = 8,
    SENDING_STATUS_REQUEST = 10,
    WAITING_STATUS_ANSWER = 11,
    SENDING_UPDATE_SIGNAL_REQUEST = 12,
    WAITING_UPDATE_SIGNAL_ANSWER = 13,
    SENDING_SIGNAL_LEVEL = 14,
    WAITING_SIGNAL_LEVEL_ANSWER = 15,
    SENDING_CONTROL = 16,
    WAITING_CONTROL_ANSWER = 17,
    SENDING_POWER_ON_COMMAND = 18,
    WAITING_POWER_ON_ANSWER = 19,
    SENDING_POWER_OFF_COMMAND = 20,
    WAITING_POWER_OFF_ANSWER = 21,
    NUM_PROTOCOL_PHASES
  };
#if (HAIER_LOG_LEVEL > 4)
  const char *phase_to_string_(ProtocolPhases phase);
#endif
  virtual void set_handlers() = 0;
  virtual void process_phase(std::chrono::steady_clock::time_point now) = 0;
  virtual haier_protocol::HaierMessage get_control_message() = 0;
  virtual bool is_message_invalid(uint8_t message_type) = 0;
  virtual void process_pending_action();
  esphome::climate::ClimateTraits traits() override;
  // Answers handlers
  haier_protocol::HandlerError answer_preprocess_(uint8_t request_message_type, uint8_t expected_request_message_type,
                                                  uint8_t answer_message_type, uint8_t expected_answer_message_type,
                                                  ProtocolPhases expected_phase);
  // Timeout handler
  haier_protocol::HandlerError timeout_default_handler_(uint8_t request_type);
  // Helper functions
  void set_force_send_control_(bool status);
  void send_message_(const haier_protocol::HaierMessage &command, bool use_crc);
  virtual void set_phase(ProtocolPhases phase);
  bool check_timeout_(std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point tpoint,
                      size_t timeout);
  bool is_message_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_status_request_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_control_message_timeout_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_control_message_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_protocol_initialisation_interval_exceeded_(std::chrono::steady_clock::time_point now);
#ifdef USE_WIFI
  haier_protocol::HaierMessage get_wifi_signal_message_(uint8_t message_type);
#endif

  struct HvacSettings {
    esphome::optional<esphome::climate::ClimateMode> mode;
    esphome::optional<esphome::climate::ClimateFanMode> fan_mode;
    esphome::optional<esphome::climate::ClimateSwingMode> swing_mode;
    esphome::optional<float> target_temperature;
    esphome::optional<esphome::climate::ClimatePreset> preset;
    bool valid;
    HvacSettings() : valid(false){};
    void reset();
  };
  haier_protocol::ProtocolHandler haier_protocol_;
  ProtocolPhases protocol_phase_;
  ActionRequest action_request_;
  uint8_t fan_mode_speed_;
  uint8_t other_modes_fan_speed_;
  bool display_status_;
  bool health_mode_;
  bool force_send_control_;
  bool forced_publish_;
  bool forced_request_status_;
  bool first_control_attempt_;
  bool reset_protocol_request_;
  esphome::climate::ClimateTraits traits_;
  HvacSettings hvac_settings_;
  std::chrono::steady_clock::time_point last_request_timestamp_;       // For interval between messages
  std::chrono::steady_clock::time_point last_valid_status_timestamp_;  // For protocol timeout
  std::chrono::steady_clock::time_point last_status_request_;          // To request AC status
  std::chrono::steady_clock::time_point control_request_timestamp_;    // To send control message
  optional<std::chrono::milliseconds> answer_timeout_;                 // Message answer timeout
  bool send_wifi_signal_;
  std::chrono::steady_clock::time_point last_signal_request_;  // To send WiFI signal level
};

}  // namespace haier
}  // namespace esphome
