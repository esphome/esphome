#include "remote_transmitter_mock.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::remote_transmitter_mock {

static const char *const TAG = "remote_transmitter_mock";

void MockRemoteTransmitter::dump_config() { ESP_LOGCONFIG(TAG, "Mock Remote Transmitter"); }

void MockRemoteTransmitter::flush_pending_completion() {
  if (!this->busy_)
    return;
  // The RMT backend blocks here until the hardware is idle; the mock only reports the overlap
  ESP_LOGW(TAG, "Overlap: a frame was requested while seq=%" PRIu16 " was in flight", this->current_seq_);
  this->cancel_timeout("complete");
  this->finish_();
}

void MockRemoteTransmitter::send_internal(uint32_t send_times, uint32_t send_wait) {
  uint64_t total_us = static_cast<uint64_t>(send_wait) * (send_times - 1);
  for (int32_t value : this->temp_.get_data()) {
    total_us += static_cast<uint64_t>(value < 0 ? -value : value) * send_times;
  }
  const auto duration_ms = static_cast<uint32_t>(total_us / 1000);

  this->busy_ = true;
  ESP_LOGI(TAG, "TX seq=%" PRIu16 " timings=%zu repeat=%" PRIu32 " duration=%" PRIu32 "ms", this->current_seq_,
           this->temp_.get_data().size(), send_times, duration_ms);
  this->set_timeout("complete", duration_ms, [this]() { this->finish_(); });
}

void MockRemoteTransmitter::finish_() {
  this->busy_ = false;
  ESP_LOGI(TAG, "Complete seq=%" PRIu16, this->current_seq_);
  this->notify_complete_(true);
}

}  // namespace esphome::remote_transmitter_mock
