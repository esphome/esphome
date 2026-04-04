#pragma once

#include <optional>
#include "esphome/components/uart/uart.h"

namespace esphome::mitsubishi_cn105 {

uint32_t get_loop_time_ms();

class MitsubishiCN105 {
 public:
  struct Status {
    bool operator==(const Status &) const = default;

    bool power_on{false};
    float target_temperature{NAN};
    float room_temperature{NAN};
  };

  explicit MitsubishiCN105(uart::UARTDevice &device) : device_(device) {}

  void initialize();
  bool update();

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

  const Status &status() const { return this->status_; }
  bool is_status_initialized() const { return !std::isnan(status_.room_temperature); }

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

  void set_state_(State new_state);
  void did_transition_(State to);
  bool read_incoming_bytes_();
  bool process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len);
  bool process_status_packet_(const uint8_t *payload, size_t len);
  bool parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len);
  bool parse_status_settings_(const uint8_t *payload, size_t len);
  bool parse_status_room_temperature_(const uint8_t *payload, size_t len);
  void reset_read_position_and_dump_buffer_(const char *prefix);
  void send_packet_(const uint8_t *packet, size_t len);
  void update_status_();
  void cancel_waiting_and_transition_to_(State state);
  template<typename T> void send_packet_(const T &packet) { this->send_packet_(packet.data(), packet.size()); }
  static bool should_transition(State from, State to);
  static const LogString *state_to_string(State state);
  static void dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len);

  uart::UARTDevice &device_;
  uint32_t update_interval_ms_{1000};
  std::optional<uint32_t> write_timeout_start_ms_;
  std::optional<uint32_t> status_update_start_ms_;
  Status status_{};
  State state_{State::NOT_CONNECTED};
  uint8_t status_msg_index_{0};

 private:
  static constexpr size_t READ_BUFFER_SIZE = 32;
  uint8_t read_buffer_[READ_BUFFER_SIZE];
  uint8_t read_pos_{0};
};

}  // namespace esphome::mitsubishi_cn105
