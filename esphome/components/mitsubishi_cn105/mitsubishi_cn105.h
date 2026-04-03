#pragma once

#include <optional>
#include "esphome/components/uart/uart.h"

namespace esphome::mitsubishi_cn105 {

uint32_t get_loop_time_ms();

class MitsubishiCN105 {
 public:
  explicit MitsubishiCN105(uart::UARTDevice &device) : device_(device) {}

  void initialize();
  void update();

  uint32_t get_update_interval() const { return this->update_interval_ms_; }
  void set_update_interval(uint32_t interval_ms) { this->update_interval_ms_ = interval_ms; }

 protected:
  enum class State : uint8_t { NOT_CONNECTED, CONNECTING, CONNECTED, READ_TIMEOUT };

  void set_state_(State new_state);
  void did_transition_(State to);
  void read_incoming_bytes_();
  void process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len);
  void reset_read_position_and_dump_buffer_(const char *prefix);
  void send_packet_(const uint8_t *packet, size_t len);
  template<typename T> void send_packet_(const T &packet) { this->send_packet_(packet.data(), packet.size()); }
  static bool should_transition(State from, State to);
  static const LogString *state_to_string(State state);
  static void dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len);

  uart::UARTDevice &device_;
  uint32_t update_interval_ms_{1000};
  std::optional<uint32_t> write_timeout_start_ms_;
  State state_{State::NOT_CONNECTED};

 private:
  static constexpr size_t READ_BUFFER_SIZE = 32;
  uint8_t read_buffer_[READ_BUFFER_SIZE];
  uint8_t read_pos_{0};
};

}  // namespace esphome::mitsubishi_cn105
