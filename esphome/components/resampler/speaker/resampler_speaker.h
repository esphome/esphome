#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"

#include <freertos/event_groups.h>
#include <freertos/FreeRTOS.h>

namespace esphome {
namespace resampler {

class ResamplerSpeaker : public Component, public speaker::Speaker {
 public:
  float get_setup_priority() const override { return esphome::setup_priority::DATA; }
  void setup() override;
  void loop() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->output_speaker_->set_pause_state(pause_state); }
  bool get_pause_state() const override { return this->output_speaker_->get_pause_state(); }

  bool has_buffered_data() const override;

  /// @brief Mute state changes are passed to the parent's output speaker
  void set_mute_state(bool mute_state) override;
  bool get_mute_state() override { return this->output_speaker_->get_mute_state(); }

  /// @brief Volume state changes are passed to the parent's output speaker
  void set_volume(float volume) override;
  float get_volume() override { return this->output_speaker_->get_volume(); }

  void set_output_speaker(speaker::Speaker *speaker) { this->output_speaker_ = speaker; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  void set_target_bits_per_sample(uint8_t target_bits_per_sample) {
    this->target_bits_per_sample_ = target_bits_per_sample;
  }
  void set_target_sample_rate(uint32_t target_sample_rate) { this->target_sample_rate_ = target_sample_rate; }

  void set_filters(uint16_t filters) { this->filters_ = filters; }
  void set_taps(uint16_t taps) { this->taps_ = taps; }

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }

 protected:
  /// @brief Starts the output speaker after setting the resampled stream info. If resampling is required, it starts the
  /// task.
  /// @return ESP_OK if resampling is required
  ///         return value of start_task_() if resampling is required
  esp_err_t start_();

  /// @brief Starts the resampler task after allocating the task stack
  /// @return ESP_OK if successful,
  ///         ESP_ERR_NO_MEM if the task stack couldn't be allocated
  ///         ESP_ERR_INVALID_STATE if the task wasn't created
  esp_err_t start_task_();

  /// @brief Stops the output speaker. If the resampling task is running, it sends the stop command.
  void stop_();

  /// @brief Deallocates the task stack and resets the pointers.
  /// @return ESP_OK if successful
  ///         ESP_ERR_INVALID_STATE if the task hasn't stopped itself
  esp_err_t delete_task_();

  inline bool requires_resampling_() const;
  static void resample_task(void *params);

  EventGroupHandle_t event_group_{nullptr};

  std::weak_ptr<RingBuffer> ring_buffer_;

  speaker::Speaker *output_speaker_{nullptr};

  bool task_stack_in_psram_{false};
  bool task_created_{false};

  TaskHandle_t task_handle_{nullptr};
  StaticTask_t task_stack_;
  StackType_t *task_stack_buffer_{nullptr};

  audio::AudioStreamInfo target_stream_info_;

  uint16_t taps_;
  uint16_t filters_;

  uint8_t target_bits_per_sample_;
  uint32_t target_sample_rate_;

  uint32_t buffer_duration_ms_;

  uint64_t callback_remainder_{0};
};

}  // namespace resampler
}  // namespace esphome

#endif
