#pragma once

#include <chrono>
#include <set>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/automation.h"
// HaierProtocol
#include <protocol/haier_protocol.h>

namespace esphome {
namespace haier {

enum class ActionRequest : uint8_t {
  SEND_CUSTOM_COMMAND = 0,
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
  bool valid_connection() const { return this->protocol_phase_ >= ProtocolPhases::IDLE; };
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
  void send_custom_command(const haier_protocol::HaierMessage &message);
  void add_status_message_callback(std::function<void(const char *, size_t)> &&callback);

 protected:
  enum class ProtocolPhases {
    UNKNOWN = -1,
    // INITIALIZATION
    SENDING_INIT_1 = 0,
    SENDING_INIT_2,
    SENDING_FIRST_STATUS_REQUEST,
    SENDING_FIRST_ALARM_STATUS_REQUEST,
    // FUNCTIONAL STATE
    IDLE,
    SENDING_STATUS_REQUEST,
    SENDING_UPDATE_SIGNAL_REQUEST,
    SENDING_SIGNAL_LEVEL,
    SENDING_CONTROL,
    SENDING_ACTION_COMMAND,
    SENDING_ALARM_STATUS_REQUEST,
    NUM_PROTOCOL_PHASES
  };
  const char *phase_to_string_(ProtocolPhases phase);
  virtual void set_handlers() = 0;
  virtual void process_phase(std::chrono::steady_clock::time_point now) = 0;
  virtual haier_protocol::HaierMessage get_control_message() = 0;          // NOLINT(readability-identifier-naming)
  virtual haier_protocol::HaierMessage get_power_message(bool state) = 0;  // NOLINT(readability-identifier-naming)
  virtual void initialization(){};
  virtual bool prepare_pending_action();
  virtual void process_protocol_reset();
  esphome::climate::ClimateTraits traits() override;
  // Answer handlers
  haier_protocol::HandlerError answer_preprocess_(haier_protocol::FrameType request_message_type,
                                                  haier_protocol::FrameType expected_request_message_type,
                                                  haier_protocol::FrameType answer_message_type,
                                                  haier_protocol::FrameType expected_answer_message_type,
                                                  ProtocolPhases expected_phase);
  haier_protocol::HandlerError report_network_status_answer_handler_(haier_protocol::FrameType request_type,
                                                                     haier_protocol::FrameType message_type,
                                                                     const uint8_t *data, size_t data_size);
  // Timeout handler
  haier_protocol::HandlerError timeout_default_handler_(haier_protocol::FrameType request_type);
  // Helper functions
  void send_message_(const haier_protocol::HaierMessage &command, bool use_crc, uint8_t num_repeats = 0,
                     std::chrono::milliseconds interval = std::chrono::milliseconds::zero());
  virtual void set_phase(ProtocolPhases phase);
  void reset_phase_();
  void reset_to_idle_();
  bool is_message_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_status_request_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_control_message_interval_exceeded_(std::chrono::steady_clock::time_point now);
  bool is_protocol_initialisation_interval_exceeded_(std::chrono::steady_clock::time_point now);
#ifdef USE_WIFI
  haier_protocol::HaierMessage get_wifi_signal_message_();
#endif

  struct HvacSettings {
    esphome::optional<esphome::climate::ClimateMode> mode;
    esphome::optional<esphome::climate::ClimateFanMode> fan_mode;
    esphome::optional<esphome::climate::ClimateSwingMode> swing_mode;
    esphome::optional<float> target_temperature;
    esphome::optional<esphome::climate::ClimatePreset> preset;
    bool valid;
    HvacSettings() : valid(false){};
    HvacSettings(const HvacSettings &) = default;
    HvacSettings &operator=(const HvacSettings &) = default;
    void reset();
  };
  struct PendingAction {
    ActionRequest action;
    esphome::optional<haier_protocol::HaierMessage> message;
  };
  haier_protocol::ProtocolHandler haier_protocol_;
  ProtocolPhases protocol_phase_;
  esphome::optional<PendingAction> action_request_;
  uint8_t fan_mode_speed_;
  uint8_t other_modes_fan_speed_;
  bool display_status_;
  bool health_mode_;
  bool force_send_control_;
  bool forced_request_status_;
  bool reset_protocol_request_;
  bool send_wifi_signal_;
  bool use_crc_;
  esphome::climate::ClimateTraits traits_;
  HvacSettings current_hvac_settings_;
  HvacSettings next_hvac_settings_;
  std::unique_ptr<uint8_t[]> last_status_message_{nullptr};
  std::chrono::steady_clock::time_point last_request_timestamp_;       // For interval between messages
  std::chrono::steady_clock::time_point last_valid_status_timestamp_;  // For protocol timeout
  std::chrono::steady_clock::time_point last_status_request_;          // To request AC status
  std::chrono::steady_clock::time_point last_signal_request_;          // To send WiFI signal level
  CallbackManager<void(const char *, size_t)> status_message_callback_{};
};

class StatusMessageTrigger : public Trigger<const char *, size_t> {
 public:
  explicit StatusMessageTrigger(HaierClimateBase *parent) {
    parent->add_status_message_callback([this](const char *data, size_t data_size) { this->trigger(data, data_size); });
  }
};

}  // namespace haier
}  // namespace esphome
