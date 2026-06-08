#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio_resampler.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/static_task.h"

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/semphr.h>

namespace esphome::resampler {

class ResamplerMicrophone : public Component, public microphone::Microphone, public audio::AudioSinkCallback {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
  void setup() override;
  void loop() override;

  void start() override;
  void stop() override;

  size_t audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) override;

  void set_microphone_source(microphone::MicrophoneSource *microphone_source) {
    this->microphone_source_ = microphone_source;
  }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }
  void set_target_sample_rate(uint32_t target_sample_rate) { this->target_sample_rate_ = target_sample_rate; }
  void set_filters(uint16_t filters) { this->filters_ = filters; }
  void set_taps(uint16_t taps) { this->taps_ = taps; }
  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }

 protected:
  esp_err_t start_task_();
  bool requires_resampling_() const;
  void configure_stream_settings_();
  static void resample_task(void *params);

  uint32_t target_sample_rate_;
  uint32_t buffer_duration_ms_;
  uint16_t taps_;
  uint16_t filters_;
  bool task_stack_in_psram_{false};

  EventGroupHandle_t event_group_{nullptr};
  SemaphoreHandle_t active_listeners_semaphore_{nullptr};
  microphone::MicrophoneSource *microphone_source_{nullptr};

  StaticTask task_;
  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;
};

}  // namespace esphome::resampler

#endif  // USE_ESP32
