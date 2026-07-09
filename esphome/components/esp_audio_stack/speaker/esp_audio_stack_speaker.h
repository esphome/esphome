#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/speaker/speaker.h"
#include "../esp_audio_stack.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <atomic>

namespace esphome::esp_audio_stack {

class ESPAudioStackSpeaker : public speaker::Speaker, public Component, public Parented<ESPAudioStack> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // speaker::Speaker interface
  void start() override;
  void stop() override;
  void finish() override;

  size_t play(const uint8_t *data, size_t length) override;
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;

  bool has_buffered_data() const override;

  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;

  void set_pause_state(bool pause_state) override;
  bool get_pause_state() const override { return this->pause_state_; }
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

 protected:
  static void speaker_output_callback(void *ctx, uint32_t frames, int64_t timestamp);

  bool pause_state_{false};
  bool finishing_{false};  // Non-blocking drain: finish() sets flag, loop() handles drain+stop
  optional<uint32_t> timeout_;
  uint32_t last_write_ms_{0};
  // Reference counting for multiple listeners (media_player, voice_assistant, call components, etc.)
  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
  // Idempotency guard: prevents multiple xSemaphoreTake per stream session.
  // Without this, play() calling start() before loop() sets STATE_RUNNING causes
  // semaphore count to leak (take N times, give 1 time → never reaches MAX_LISTENERS
  // → speaker never enters STATE_STOPPING → is_playing stays true forever).
  std::atomic<bool> listener_registered_{false};
};

}  // namespace esphome::esp_audio_stack

#endif  // USE_ESP32
