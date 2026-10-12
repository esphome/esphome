#pragma once

#ifdef USE_ESP32

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"

#include <driver/gptimer.h>
#include <driver/sdm.h>

namespace esphome::sigma_delta {

class SigmaDeltaSpeaker final : public Component, public speaker::Speaker {
 public:
  explicit SigmaDeltaSpeaker(InternalGPIOPin *pin) : pin_(pin) {}

  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;

  void start() override;
  void stop() override;
  void finish() override;
  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  bool has_buffered_data() const override;

  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;

 protected:
  static bool timer_isr(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx);

  /// Copies as many whole frames as fit into the ring; returns the number of bytes consumed.
  size_t write_frames_(const uint8_t *data, size_t length);
  void update_gain_();
  void report_played_();
  void apply_command_();
  void arm_timer_();
  void disarm_timer_();

  // Single producer (play) / single consumer (timer ISR) ring of 8-bit pulse densities.
  // Indices run freely and are masked on access.
  static constexpr size_t RING_SIZE = 4096;
  static_assert((RING_SIZE & (RING_SIZE - 1)) == 0, "RING_SIZE must be a power of two");
  std::array<int8_t, RING_SIZE> ring_{};

  InternalGPIOPin *pin_;
  sdm_channel_handle_t sdm_handle_{nullptr};
  gptimer_handle_t timer_handle_{nullptr};
  std::atomic<uint32_t> ring_read_{0};   // Moved only by the ISR
  std::atomic<uint32_t> ring_write_{0};  // Moved only by play()
  std::atomic<int32_t> gain_q15_{1 << 15};
  uint32_t last_data_ms_{0};
  uint32_t last_reported_read_{0};        // Main loop only
  std::atomic<bool> timer_armed_{false};  // Written only from the main loop
  // start(), stop() and finish() may run on an audio task; they only post a request here
  // and loop() applies it to state_, so state_ is written from one task only.
  enum Command : uint8_t { COMMAND_NONE, COMMAND_START, COMMAND_STOP, COMMAND_FINISH };
  std::atomic<uint8_t> command_{COMMAND_NONE};

 private:
  void teardown_();
};

}  // namespace esphome::sigma_delta

#endif  // USE_ESP32
