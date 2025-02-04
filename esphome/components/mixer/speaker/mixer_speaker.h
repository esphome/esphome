#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/speaker/speaker.h"

#include "esphome/core/component.h"

#include <freertos/event_groups.h>
#include <freertos/FreeRTOS.h>

namespace esphome {
namespace mixer_speaker {

/* Classes for mixing several source speaker audio streams and writing it to another speaker component.
 *  - Volume controls are passed through to the output speaker
 *  - Directly handles pausing at the SourceSpeaker level; pause state is not passed through to the output speaker.
 *  - Audio sent to the SourceSpeaker's must have 16 bits per sample.
 *  - Audio sent to the SourceSpeaker can have any number of channels. They are duplicated or ignored as needed to match
 *    the number of channels required for the output speaker.
 *  - In queue mode, the audio sent to the SoureSpeakers can have different sample rates.
 *  - In non-queue mode, the audio sent to the SourceSpeakers must have the same sample rates.
 *  - SourceSpeaker has an internal ring buffer. It also allocates a shared_ptr for an AudioTranserBuffer object.
 *  - Audio Data Flow:
 *      - Audio data played on a SourceSpeaker first writes to its internal ring buffer.
 *      - MixerSpeaker task temporarily takes shared ownership of each SourceSpeaker's AudioTransferBuffer.
 *      - MixerSpeaker calls SourceSpeaker's `process_data_from_source`, which tranfers audio from the SourceSpeaker's
 *        ring buffer to its AudioTransferBuffer. Audio ducking is applied at this step.
 *      - In queue mode, MixerSpeaker prioritizes the earliest configured SourceSpeaker with audio data. Audio data is
 *        sent to the output speaker.
 *      - In non-queue mode, MixerSpeaker adds all the audio data in each SourceSpeaker into one stream that is written
 *        to the output speaker.
 */

class MixerSpeaker;

class SourceSpeaker : public speaker::Speaker, public Component {
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

  /// @brief Volume state changes are passed to the parent's output speaker
  void set_volume(float volume) override;

  void set_pause_state(bool pause_state) override { this->pause_state_ = pause_state; }
  bool get_pause_state() const override { return this->pause_state_; }

  /// @brief Transfers audio from the ring buffer into the transfer buffer. Ducks audio while transferring.
  /// @param ticks_to_wait FreeRTOS ticks to wait while waiting to read from the ring buffer.
  /// @return Number of bytes transferred from the ring buffer.
  size_t process_data_from_source(TickType_t ticks_to_wait);

  /// @brief Sets the ducking level for the source speaker.
  /// @param decibel_reduction (uint8_t) The dB reduction level. For example, 0 is no change, 10 is a reduction by 10 dB
  /// @param duration (uint32_t) The number of milliseconds to transition from the current level to the new level
  void apply_ducking(uint8_t decibel_reduction, uint32_t duration);

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }
  void set_parent(MixerSpeaker *parent) { this->parent_ = parent; }
  void set_timeout(uint32_t ms) { this->timeout_ms_ = ms; }

  std::weak_ptr<audio::AudioSourceTransferBuffer> get_transfer_buffer() { return this->transfer_buffer_; }

 protected:
  friend class MixerSpeaker;
  esp_err_t start_();
  void stop_();

  /// @brief Ducks audio samples by a specified amount. When changing the ducking amount, it can transition gradually
  /// over a specified amount of samples.
  /// @param input_buffer buffer with audio samples to be ducked in place
  /// @param input_samples_to_duck number of samples to process in ``input_buffer``
  /// @param current_ducking_db_reduction pointer to the current dB reduction
  /// @param ducking_transition_samples_remaining pointer to the total number of samples left before the the
  ///         transition is finished
  /// @param samples_per_ducking_step total number of samples per ducking step for the transition
  /// @param db_change_per_ducking_step the change in dB reduction per step
  static void duck_samples(int16_t *input_buffer, uint32_t input_samples_to_duck, int8_t *current_ducking_db_reduction,
                           uint32_t *ducking_transition_samples_remaining, uint32_t samples_per_ducking_step,
                           int8_t db_change_per_ducking_step);

  MixerSpeaker *parent_;

  std::shared_ptr<audio::AudioSourceTransferBuffer> transfer_buffer_;
  std::weak_ptr<RingBuffer> ring_buffer_;

  uint32_t buffer_duration_ms_;
  uint32_t last_seen_data_ms_{0};
  optional<uint32_t> timeout_ms_;
  bool stop_gracefully_{false};

  bool pause_state_{false};

  int8_t target_ducking_db_reduction_{0};
  int8_t current_ducking_db_reduction_{0};
  int8_t db_change_per_ducking_step_{1};
  uint32_t ducking_transition_samples_remaining_{0};
  uint32_t samples_per_ducking_step_{0};

  uint32_t accumulated_frames_read_{0};

  uint32_t pending_playback_ms_{0};
};

class MixerSpeaker : public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  void add_source_speaker(SourceSpeaker *source_speaker) { this->source_speakers_.push_back(source_speaker); }

  /// @brief Starts the mixer task. Called by a source speaker giving the current audio stream information
  /// @param stream_info The calling source speakers audio stream information
  /// @return ESP_ERR_NOT_SUPPORTED if the incoming stream is incompatible due to unsupported bits per sample
  ///         ESP_ERR_INVALID_ARG if the incoming stream is incompatible to be mixed with the other input audio stream
  ///         ESP_ERR_NO_MEM if there isn't enough memory for the task's stack
  ///         ESP_ERR_INVALID_STATE if the task fails to start
  ///         ESP_OK if the incoming stream is compatible and the mixer task starts
  esp_err_t start(audio::AudioStreamInfo &stream_info);

  void stop();

  void set_output_channels(uint8_t output_channels) { this->output_channels_ = output_channels; }
  void set_output_speaker(speaker::Speaker *speaker) { this->output_speaker_ = speaker; }
  void set_queue_mode(bool queue_mode) { this->queue_mode_ = queue_mode; }
  void set_task_stack_in_psram(bool task_stack_in_psram) { this->task_stack_in_psram_ = task_stack_in_psram; }

  speaker::Speaker *get_output_speaker() const { return this->output_speaker_; }

 protected:
  /// @brief Copies audio frames from the input buffer to the output buffer taking into account the number of channels
  /// in each stream. If the output stream has more channels, the input samples are duplicated. If the output stream has
  /// less channels, the extra channel input samples are dropped.
  /// @param input_buffer
  /// @param input_stream_info
  /// @param output_buffer
  /// @param output_stream_info
  /// @param frames_to_transfer number of frames (consisting of a sample for each channel) to copy from the input buffer
  static void copy_frames(const int16_t *input_buffer, audio::AudioStreamInfo input_stream_info, int16_t *output_buffer,
                          audio::AudioStreamInfo output_stream_info, uint32_t frames_to_transfer);

  /// @brief Mixes the primary and secondary streams taking into account the number of channels in each stream. Primary
  /// and secondary samples are duplicated or dropped as necessary to ensure the output stream has the configured number
  /// of channels. Output samples are clamped to the corresponding int16 min or max values if the mixed sample
  /// overflows.
  /// @param primary_buffer (int16_t *) samples buffer for the primary stream
  /// @param primary_stream_info stream info for the primary stream
  /// @param secondary_buffer (int16_t *) samples buffer for secondary stream
  /// @param secondary_stream_info stream info for the secondary stream
  /// @param output_buffer (int16_t *) buffer for the mixed samples
  /// @param output_stream_info stream info for the output buffer
  /// @param frames_to_mix number of frames in the primary and secondary buffers to mix together
  static void mix_audio_samples(const int16_t *primary_buffer, audio::AudioStreamInfo primary_stream_info,
                                const int16_t *secondary_buffer, audio::AudioStreamInfo secondary_stream_info,
                                int16_t *output_buffer, audio::AudioStreamInfo output_stream_info,
                                uint32_t frames_to_mix);

  static void audio_mixer_task(void *params);

  /// @brief Starts the mixer task after allocating memory for the task stack.
  /// @return ESP_ERR_NO_MEM if there isn't enough memory for the task's stack
  ///         ESP_ERR_INVALID_STATE if the task didn't start
  ///         ESP_OK if successful
  esp_err_t start_task_();

  /// @brief If the task is stopped, it sets the task handle to the nullptr and deallocates its stack
  /// @return ESP_OK if the task was stopped, ESP_ERR_INVALID_STATE otherwise.
  esp_err_t delete_task_();

  EventGroupHandle_t event_group_{nullptr};

  std::vector<SourceSpeaker *> source_speakers_;
  speaker::Speaker *output_speaker_{nullptr};

  uint8_t output_channels_;
  bool queue_mode_;
  bool task_stack_in_psram_{false};

  bool task_created_{false};

  TaskHandle_t task_handle_{nullptr};
  StaticTask_t task_stack_;
  StackType_t *task_stack_buffer_{nullptr};

  optional<audio::AudioStreamInfo> audio_stream_info_;
};

}  // namespace mixer_speaker
}  // namespace esphome

#endif
