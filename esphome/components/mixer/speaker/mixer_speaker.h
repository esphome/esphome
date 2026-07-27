#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/static_task.h"

#include <ducking.h>  // esp-audio-libs

#include <freertos/event_groups.h>

#include <atomic>

namespace esphome::mixer_speaker {

/* Classes for mixing several source speaker audio streams and writing it to another speaker component.
 *  - Volume controls are passed through to the output speaker
 *  - Source speaker commands are signaled via event group bits and processed in its loop function to ensure thread
 * safety
 *  - Directly handles pausing at the SourceSpeaker level; pause state is not passed through to the output speaker.
 *  - Audio sent to the SourceSpeaker can have 8, 16, 24, or 32 bits per sample. Each source is converted to the output
 *    speaker's bit depth as it is mixed (or copied) into the output buffer.
 *  - Audio sent to the SourceSpeaker can have any number of channels. They are duplicated or ignored as needed to match
 *    the number of channels required for the output speaker.
 *  - In queue mode, the audio sent to the SourceSpeakers can have different sample rates.
 *  - In non-queue mode, the audio sent to the SourceSpeakers must have the same sample rates.
 *  - SourceSpeaker has an internal ring buffer. It also allocates a shared_ptr for an AudioTranserBuffer object.
 *  - Audio Data Flow:
 *      - Audio data played on a SourceSpeaker first writes to its internal ring buffer.
 *      - MixerSpeaker task temporarily takes shared ownership of each SourceSpeaker's AudioTransferBuffer.
 *      - MixerSpeaker calls SourceSpeaker's `process_data_from_source`, which transfers audio from the SourceSpeaker's
 *        ring buffer to its AudioTransferBuffer. Audio ducking is applied at this step.
 *      - In queue mode, MixerSpeaker prioritizes the earliest configured SourceSpeaker with audio data. Audio data is
 *        sent to the output speaker.
 *      - In non-queue mode, MixerSpeaker adds all the audio data in each SourceSpeaker into one stream that is written
 *        to the output speaker.
 */

class MixerSpeaker;

class SourceSpeaker final : public speaker::Speaker, public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }

  void start() override;
  void stop() override;
  void finish() override;

  bool has_buffered_data() const override;

  /// @brief Mute state changes are passed to the parent's output speaker
  void set_mute_state(bool mute_state) override;
  bool get_mute_state() override;

  /// @brief Volume state changes are passed to the parent's output speaker
  void set_volume(float volume) override;
  float get_volume() override;

  void set_pause_state(bool pause_state) override { this->pause_state_ = pause_state; }
  bool get_pause_state() const override { return this->pause_state_; }

  /// @brief Exposes the next ring buffer chunk (zero-copy) and ducks the freshly exposed bytes in place.
  /// If the source still has bytes from a prior partial consume, this is a no-op (those bytes were already
  /// ducked on the fill that exposed them).
  /// @param audio_source Locked shared_ptr to the audio source (must be valid, not null)
  /// @param ticks_to_wait FreeRTOS ticks to wait while waiting to read from the ring buffer.
  /// @return Number of bytes newly exposed from the ring buffer.
  size_t process_data_from_source(std::shared_ptr<audio::RingBufferAudioSource> &audio_source,
                                  TickType_t ticks_to_wait);

  /// @brief Sets the ducking level for the source speaker.
  /// @param decibel_reduction The dB reduction level. For example, 0 is no change, 10 is a reduction by 10 dB
  /// @param duration The number of milliseconds to transition from the current level to the new level
  void apply_ducking(uint8_t decibel_reduction, uint32_t duration);

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_parent(MixerSpeaker *parent) { this->parent_ = parent; }
  void set_timeout(uint32_t ms) { this->timeout_ms_ = ms; }

  std::weak_ptr<audio::RingBufferAudioSource> get_audio_source() { return this->audio_source_; }

 protected:
  friend class MixerSpeaker;
  esp_err_t start_();
  void enter_stopping_state_();
  void send_command_(uint32_t command_bit, bool wake_loop = false);

  MixerSpeaker *parent_;

  std::shared_ptr<audio::RingBufferAudioSource> audio_source_;
  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;

  uint32_t buffer_duration_ms_;
  uint32_t last_seen_data_ms_{0};
  optional<uint32_t> timeout_ms_;
  bool stop_gracefully_{false};

  bool pause_state_{false};

  esp_audio_libs::ducking::DuckingState ducking_state_{};

  std::atomic<uint32_t> pending_playback_frames_{0};
  std::atomic<uint32_t> playback_delay_frames_{0};  // Frames in output pipeline when this source started contributing
  std::atomic<bool> has_contributed_{false};        // Tracks if source has contributed during this session

  EventGroupHandle_t event_group_{nullptr};
  uint32_t stopping_start_ms_{0};
};

class MixerSpeaker final : public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  void init_source_speakers(size_t count) { this->source_speakers_.init(count); }
  void add_source_speaker(SourceSpeaker *source_speaker) { this->source_speakers_.push_back(source_speaker); }

  /// @brief Starts the mixer task. Called by a source speaker giving the current audio stream information
  /// @param stream_info The calling source speaker's audio stream information
  /// @return ESP_ERR_INVALID_ARG if the incoming stream is incompatible to be mixed with the other input audio stream
  ///         ESP_OK if the incoming stream is compatible and the mixer task starts
  esp_err_t start(audio::AudioStreamInfo &stream_info);

  void set_output_channels(uint8_t output_channels) { this->output_channels_ = output_channels; }
  void set_output_bits_per_sample(uint8_t output_bits_per_sample) {
    this->output_bits_per_sample_ = output_bits_per_sample;
  }
  void set_output_speaker(speaker::Speaker *speaker) { this->output_speaker_ = speaker; }
  void set_queue_mode(bool queue_mode) { this->queue_mode_ = queue_mode; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  speaker::Speaker *get_output_speaker() const { return this->output_speaker_; }

  /// @brief Returns the current number of frames in the output pipeline (written but not yet played)
  uint32_t get_frames_in_pipeline() const { return this->frames_in_pipeline_.load(std::memory_order_acquire); }

 protected:
  static void audio_mixer_task(void *params);

  EventGroupHandle_t event_group_{nullptr};

  FixedVector<SourceSpeaker *> source_speakers_;
  speaker::Speaker *output_speaker_{nullptr};

  uint8_t output_bits_per_sample_;
  uint8_t output_channels_;
  bool queue_mode_;
  bool task_stack_in_psram_{false};

  StaticTask task_;

  optional<audio::AudioStreamInfo> audio_stream_info_;

  std::atomic<uint32_t> frames_in_pipeline_{0};  // Frames written to output but not yet played
  uint32_t all_stopped_since_ms_{0};             // Debounce transient all-stopped windows before stopping task
};

}  // namespace esphome::mixer_speaker

#endif
