#include "response_awaiter.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace jablotron {

ResponseAwaiter::ResponseAwaiter(uint32_t send_wait_time) : send_wait_time_{send_wait_time} {}

bool ResponseAwaiter::is_waiting_for_response() const {
  return is_waiting_for_response_ && (millis() - this->last_send_time_) < this->send_wait_time_;
}

void ResponseAwaiter::request_sent() {
  this->is_waiting_for_response_ = true;
  this->last_send_time_ = millis();
}

void ResponseAwaiter::response_received() { this->is_waiting_for_response_ = false; }

}  // namespace jablotron
}  // namespace esphome
