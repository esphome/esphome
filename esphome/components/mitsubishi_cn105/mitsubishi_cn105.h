#pragma once

#include <optional>
#include "esphome/components/uart/uart.h"

namespace esphome::mitsubishi_cn105 {

uint32_t get_loop_time_ms();

class MitsubishiCN105 {
 public:
  enum class Mode : uint8_t {
    HEAT,
    DRY,
    COOL,
    FAN_ONLY,
    AUTO,
    UNKNOWN,
  };

  enum class FanMode : uint8_t {
    AUTO,
    QUIET,
    SPEED_1,
    SPEED_2,
    SPEED_3,
    SPEED_4,
    UNKNOWN,
  };

  struct Status {
    bool power_on{false};
    float target_temperature{NAN};
    Mode mode{Mode::UNKNOWN};
    FanMode fan_mode{FanMode::UNKNOWN};
    float room_temperature{NAN};
  };

  explicit MitsubishiCN105(uart::UARTDevice &device) : device_(device) {}

  void initialize();
  bool update();

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

  uint32_t get_room_temperature_min_interval() const { return this->room_temperature_min_interval_ms_; }
  bool is_room_temperature_enabled() const { return this->room_temperature_min_interval_ms_ != SCHEDULER_DONT_RUN; }
  void set_room_temperature_min_interval(uint32_t interval_ms) {
    this->room_temperature_min_interval_ms_ = interval_ms;
  }

  const Status &status() const { return this->status_; }
  bool is_status_initialized() const {
    return this->is_room_temperature_enabled() ? !std::isnan(this->status_.room_temperature)
                                               : !std::isnan(this->status_.target_temperature);
  }

 protected:
  enum class State : uint8_t {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    UPDATING_STATUS,
    STATUS_UPDATED,
    SCHEDULE_NEXT_STATUS_UPDATE,
    WAITING_FOR_SCHEDULED_STATUS_UPDATE,
    READ_TIMEOUT
  };

  class FrameParser {
   public:
    template<typename Callback> bool read_and_parse(uart::UARTDevice &device, Callback &&callback);
    void reset() { read_pos_ = 0; }
    static void dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len);

   protected:
    void reset_and_dump_buffer_(const char *prefix);

   private:
    static constexpr size_t READ_BUFFER_SIZE = 32;
    uint8_t read_buffer_[READ_BUFFER_SIZE];
    uint8_t read_pos_{0};
  };

  void set_state_(State new_state);
  void did_transition_(State to);
  bool process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len);
  bool process_status_packet_(const uint8_t *payload, size_t len);
  bool parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len);
  bool parse_status_settings_(const uint8_t *payload, size_t len);
  bool parse_status_room_temperature_(const uint8_t *payload, size_t len);
  void send_packet_(const uint8_t *packet, size_t len);
  void update_status_();
  void cancel_waiting_and_transition_to_(State state);
  bool should_request_room_temperature_() const;
  template<typename T> void send_packet_(const T &packet) { this->send_packet_(packet.data(), packet.size()); }
  static bool should_transition(State from, State to);
  static const LogString *state_to_string(State state);

  uart::UARTDevice &device_;
  uint32_t update_interval_ms_{1000};
  uint32_t room_temperature_min_interval_ms_{60000};
  std::optional<uint32_t> write_timeout_start_ms_;
  std::optional<uint32_t> status_update_start_ms_;
  std::optional<uint32_t> last_room_temperature_update_ms_;
  Status status_{};
  State state_{State::NOT_CONNECTED};
  uint8_t current_status_msg_type_{0};
  FrameParser frame_parser_;
};

}  // namespace esphome::mitsubishi_cn105
