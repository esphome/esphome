#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && \
    (defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/components/audio/audio.h"
#include "esphome/components/speaker/speaker.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

#include <atomic>

namespace esphome::usb_audio {

class USBAudioSpeaker : public speaker::Speaker, public Component, public Parented<USBAudioComponent> {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;

  void start() override;
  void stop() override;
  void finish() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override;
  bool has_buffered_data() const override;

  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }
  void set_bits_per_sample(uint16_t bits) { this->bits_per_sample_ = bits; }
  void set_channels(uint8_t channels) { this->channels_ = channels; }
  void set_write_timeout(uint32_t timeout_ms) { this->write_timeout_ms_ = timeout_ms; }

  void set_volume(float volume) override;
  void set_mute_state(bool mute_state) override;

 protected:
  bool ensure_started_();
  size_t play_internal_(const uint8_t *data, size_t length, uint32_t time_budget_ms);
  bool device_unavailable_(uint32_t wait_ms);

  uint32_t sample_rate_{48000};
  uint32_t write_timeout_ms_{20};
  uint32_t last_write_ms_{0};
  uint32_t finish_deadline_ms_{0};
  size_t preferred_chunk_size_{0};
  size_t chunk_upper_bound_{0};
  size_t chunk_success_streak_{0};
  uint16_t bits_per_sample_{16};
  uint8_t channels_{2};
  // Set on the main loop when the device goes away; play() runs on the caller's task.
  std::atomic<bool> device_lost_{false};
  bool was_connected_{false};
  bool finish_requested_{false};
};

}  // namespace esphome::usb_audio

#endif
