#pragma once
#include <cstdint>

namespace esphome {
namespace jablotron {

class ResponseAwaiter {
 public:
  explicit ResponseAwaiter(uint32_t send_wait_time = 2000);

  bool is_waiting_for_response() const;
  void request_sent();
  void response_received();

 private:
  bool is_waiting_for_response_ = false;
  uint32_t last_send_time_ = 0;
  const uint32_t send_wait_time_;
};

}  // namespace jablotron
}  // namespace esphome
