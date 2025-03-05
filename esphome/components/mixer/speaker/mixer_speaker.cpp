#include "mixer_speaker.h"

#ifdef USE_ESP32

#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {
namespace mixer_speaker {

static const UBaseType_t MIXER_TASK_PRIORITY = 10;

static const uint32_t TRANSFER_BUFFER_DURATION_MS = 50;
static const uint32_t TASK_DELAY_MS = 25;

static const size_t TASK_STACK_SIZE = 4096;

static const int16_t MAX_AUDIO_SAMPLE_VALUE = INT16_MAX;
static const int16_t MIN_AUDIO_SAMPLE_VALUE = INT16_MIN;

static const char *const TAG = "speaker_mixer";

// Gives the Q15 fixed point scaling factor to reduce by 0 dB, 1dB, ..., 50 dB
// dB to PCM scaling factor formula: floating_point_scale_factor = 2^(-db/6.014)
// float to Q15 fixed point formula: q15_scale_factor = floating_point_scale_factor * 2^(15)
static const std::vector<int16_t> DECIBEL_REDUCTION_TABLE = {
    32767, 29201, 26022, 23189, 20665, 18415, 16410, 14624, 13032, 11613, 10349, 9222, 8218, 7324, 6527, 5816, 5183,
    4619,  4116,  3668,  3269,  2913,  2596,  2313,  2061,  1837,  1637,  1459,  1300, 1158, 1032, 920,  820,  731,
    651,   580,   517,   461,   411,   366,   326,   291,   259,   231,   206,   183,  163,  146,  130,  116,  103};

enum MixerEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the mixer task
  STATE_STARTING = (1 << 10),
  STATE_RUNNING = (1 << 11),
  STATE_STOPPING = (1 << 12),
  STATE_STOPPED = (1 << 13),
  ERR_ESP_NO_MEM = (1 << 19),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void SourceSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "Mixer Source Speaker");
  ESP_LOGCONFIG(TAG, "  Buffer Duration: %" PRIu32 " ms", this->buffer_duration_ms_);
  if (this->timeout_ms_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_ms_.value());
  } else {
    ESP_LOGCONFIG(TAG, "  Timeout: never");
  }
}

void SourceSpeaker::setup() {
  this->parent_->get_output_speaker()->add_audio_output_callback(
      [this](uint32_t new_playback_ms, uint32_t remainder_us, uint32_t pending_ms, uint32_t write_timestamp) {
        uint32_t personal_playback_ms = std::min(new_playback_ms, this->pending_playback_ms_);
        if (personal_playback_ms > 0) {
          this->pending_playback_ms_ -= personal_playback_ms;
          this->audio_output_callback_(personal_playback_ms, remainder_us, this->pending_playback_ms_, write_timestamp);
        }
      });
}

void SourceSpeaker::loop() {
  switch (this->state_) {
    case speaker::STATE_STARTING: {
      esp_err_t err = this->start_();
      if (err == ESP_OK) {
        this->state_ = speaker::STATE_RUNNING;
        this->stop_gracefully_ = false;
        this->last_seen_data_ms_ = millis();
        this->status_clear_error();
      } else {
        switch (err) {
          case ESP_ERR_NO_MEM:
            this->status_set_error("Failed to start mixer: not enough memory");
            break;
          case ESP_ERR_NOT_SUPPORTED:
            this->status_set_error("Failed to start mixer: unsupported bits per sample");
            break;
          case ESP_ERR_INVALID_ARG:
            this->status_set_error("Failed to start mixer: audio stream isn't compatible with the other audio stream.");
            break;
          case ESP_ERR_INVALID_STATE:
            this->status_set_error("Failed to start mixer: mixer task failed to start");
            break;
          default:
            this->status_set_error("Failed to start mixer");
            break;
        }

        this->state_ = speaker::STATE_STOPPING;
      }
      break;
    }
    case speaker::STATE_RUNNING:
      if (!this->transfer_buffer_->has_buffered_data()) {
        if ((this->timeout_ms_.has_value() && ((millis() - this->last_seen_data_ms_) > this->timeout_ms_.value())) ||
            this->stop_gracefully_) {
          this->state_ = speaker::STATE_STOPPING;
        }
      }
      break;
    case speaker::STATE_STOPPING:
      this->stop_();
      this->stop_gracefully_ = false;
      this->state_ = speaker::STATE_STOPPED;
      break;
    case speaker::STATE_STOPPED:
      break;
  }
}

size_t SourceSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_stopped()) {
    this->start();
  }
  size_t bytes_written = 0;
  if (this->ring_buffer_.use_count() == 1) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    bytes_written = temp_ring_buffer->write_without_replacement(data, length, ticks_to_wait);
    if (bytes_written > 0) {
      this->last_seen_data_ms_ = millis();
    }
  }
  return bytes_written;
}

void SourceSpeaker::start() { this->state_ = speaker::STATE_STARTING; }

esp_err_t SourceSpeaker::start_() {
  const size_t ring_buffer_size = this->audio_stream_info_.ms_to_bytes(this->buffer_duration_ms_);
  if (this->transfer_buffer_.use_count() == 0) {
    this->transfer_buffer_ =
        audio::AudioSourceTransferBuffer::create(this->audio_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS));

    if (this->transfer_buffer_ == nullptr) {
      return ESP_ERR_NO_MEM;
    }
    std::shared_ptr<RingBuffer> temp_ring_buffer;

    if (!this->ring_buffer_.use_count()) {
      temp_ring_buffer = RingBuffer::create(ring_buffer_size);
      this->ring_buffer_ = temp_ring_buffer;
    }

    if (!this->ring_buffer_.use_count()) {
      return ESP_ERR_NO_MEM;
    } else {
      this->transfer_buffer_->set_source(temp_ring_buffer);
    }
  }

  return this->parent_->start(this->audio_stream_info_);
}

void SourceSpeaker::stop() {
  if (this->state_ != speaker::STATE_STOPPED) {
    this->state_ = speaker::STATE_STOPPING;
  }
}

void SourceSpeaker::stop_() {
  this->transfer_buffer_.reset();  // deallocates the transfer buffer
}

void SourceSpeaker::finish() { this->stop_gracefully_ = true; }

bool SourceSpeaker::has_buffered_data() const {
  return ((this->transfer_buffer_.use_count() > 0) && this->transfer_buffer_->has_buffered_data());
}

void SourceSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->parent_->get_output_speaker()->set_mute_state(mute_state);
}

void SourceSpeaker::set_volume(float volume) {
  this->volume_ = volume;
  this->parent_->get_output_speaker()->set_volume(volume);
}

size_t SourceSpeaker::process_data_from_source(TickType_t ticks_to_wait) {
  if (!this->transfer_buffer_.use_count()) {
    return 0;
  }

  // Store current offset, as these samples are already ducked
  const size_t current_length = this->transfer_buffer_->available();

  size_t bytes_read = this->transfer_buffer_->transfer_data_from_source(ticks_to_wait);

  uint32_t samples_to_duck = this->audio_stream_info_.bytes_to_samples(bytes_read);
  if (samples_to_duck > 0) {
    int16_t *current_buffer = reinterpret_cast<int16_t *>(this->transfer_buffer_->get_buffer_start() + current_length);

    duck_samples(current_buffer, samples_to_duck, &this->current_ducking_db_reduction_,
                 &this->ducking_transition_samples_remaining_, this->samples_per_ducking_step_,
                 this->db_change_per_ducking_step_);
  }

  return bytes_read;
}

void SourceSpeaker::apply_ducking(uint8_t decibel_reduction, uint32_t duration) {
  if (this->target_ducking_db_reduction_ != decibel_reduction) {
    this->current_ducking_db_reduction_ = this->target_ducking_db_reduction_;

    this->target_ducking_db_reduction_ = decibel_reduction;

    uint8_t total_ducking_steps = 0;
    if (this->target_ducking_db_reduction_ > this->current_ducking_db_reduction_) {
      // The dB reduction level is increasing (which results in quieter audio)
      total_ducking_steps = this->target_ducking_db_reduction_ - this->current_ducking_db_reduction_ - 1;
      this->db_change_per_ducking_step_ = 1;
    } else {
      // The dB reduction level is decreasing (which results in louder audio)
      total_ducking_steps = this->current_ducking_db_reduction_ - this->target_ducking_db_reduction_ - 1;
      this->db_change_per_ducking_step_ = -1;
    }
    if ((duration > 0) && (total_ducking_steps > 0)) {
      this->ducking_transition_samples_remaining_ = this->audio_stream_info_.ms_to_samples(duration);

      this->samples_per_ducking_step_ = this->ducking_transition_samples_remaining_ / total_ducking_steps;
      this->ducking_transition_samples_remaining_ =
          this->samples_per_ducking_step_ * total_ducking_steps;  // Adjust for integer division rounding

      this->current_ducking_db_reduction_ += this->db_change_per_ducking_step_;
    } else {
      this->ducking_transition_samples_remaining_ = 0;
      this->current_ducking_db_reduction_ = this->target_ducking_db_reduction_;
    }
  }
}

void SourceSpeaker::duck_samples(int16_t *input_buffer, uint32_t input_samples_to_duck,
                                 int8_t *current_ducking_db_reduction, uint32_t *ducking_transition_samples_remaining,
                                 uint32_t samples_per_ducking_step, int8_t db_change_per_ducking_step) {
  if (*ducking_transition_samples_remaining > 0) {
    // Ducking level is still transitioning

    // Takes the ceiling of input_samples_to_duck/samples_per_ducking_step
    uint32_t ducking_steps_in_batch =
        input_samples_to_duck / samples_per_ducking_step + (input_samples_to_duck % samples_per_ducking_step != 0);

    for (uint32_t i = 0; i < ducking_steps_in_batch; ++i) {
      uint32_t samples_left_in_step = *ducking_transition_samples_remaining % samples_per_ducking_step;

      if (samples_left_in_step == 0) {
        samples_left_in_step = samples_per_ducking_step;
      }

      uint32_t samples_to_duck = std::min(input_samples_to_duck, samples_left_in_step);
      samples_to_duck = std::min(samples_to_duck, *ducking_transition_samples_remaining);

      // Ensure we only point to valid index in the Q15 scaling factor table
      uint8_t safe_db_reduction_index =
          clamp<uint8_t>(*current_ducking_db_reduction, 0, DECIBEL_REDUCTION_TABLE.size() - 1);
      int16_t q15_scale_factor = DECIBEL_REDUCTION_TABLE[safe_db_reduction_index];

      audio::scale_audio_samples(input_buffer, input_buffer, q15_scale_factor, samples_to_duck);

      if (samples_left_in_step - samples_to_duck == 0) {
        // After scaling the current samples, we are ready to transition to the next step
        *current_ducking_db_reduction += db_change_per_ducking_step;
      }

      input_buffer += samples_to_duck;
      *ducking_transition_samples_remaining -= samples_to_duck;
      input_samples_to_duck -= samples_to_duck;
    }
  }

  if ((*current_ducking_db_reduction > 0) && (input_samples_to_duck > 0)) {
    // Audio is ducked, but its not in the middle of a transition step

    uint8_t safe_db_reduction_index =
        clamp<uint8_t>(*current_ducking_db_reduction, 0, DECIBEL_REDUCTION_TABLE.size() - 1);
    int16_t q15_scale_factor = DECIBEL_REDUCTION_TABLE[safe_db_reduction_index];

    audio::scale_audio_samples(input_buffer, input_buffer, q15_scale_factor, input_samples_to_duck);
  }
}

void MixerSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG, "Speaker Mixer:");
  ESP_LOGCONFIG(TAG, "  Number of output channels: %u", this->output_channels_);
}

void MixerSpeaker::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }
}

void MixerSpeaker::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & MixerEventGroupBits::STATE_STARTING) {
    ESP_LOGD(TAG, "Starting speaker mixer");
    xEventGroupClearBits(this->event_group_, MixerEventGroupBits::STATE_STARTING);
  }
  if (event_group_bits & MixerEventGroupBits::ERR_ESP_NO_MEM) {
    this->status_set_error("Failed to allocate the mixer's internal buffer");
    xEventGroupClearBits(this->event_group_, MixerEventGroupBits::ERR_ESP_NO_MEM);
  }
  if (event_group_bits & MixerEventGroupBits::STATE_RUNNING) {
    ESP_LOGD(TAG, "Started speaker mixer");
    this->status_clear_error();
    xEventGroupClearBits(this->event_group_, MixerEventGroupBits::STATE_RUNNING);
  }
  if (event_group_bits & MixerEventGroupBits::STATE_STOPPING) {
    ESP_LOGD(TAG, "Stopping speaker mixer");
    xEventGroupClearBits(this->event_group_, MixerEventGroupBits::STATE_STOPPING);
  }
  if (event_group_bits & MixerEventGroupBits::STATE_STOPPED) {
    if (this->delete_task_() == ESP_OK) {
      xEventGroupClearBits(this->event_group_, MixerEventGroupBits::ALL_BITS);
    }
  }

  if (this->task_handle_ != nullptr) {
    bool all_stopped = true;

    for (auto &speaker : this->source_speakers_) {
      all_stopped &= speaker->is_stopped();
    }

    if (all_stopped) {
      this->stop();
    }
  }
}

esp_err_t MixerSpeaker::start(audio::AudioStreamInfo &stream_info) {
  if (!this->audio_stream_info_.has_value()) {
    if (stream_info.get_bits_per_sample() != 16) {
      // Audio streams that don't have 16 bits per sample are not supported
      return ESP_ERR_NOT_SUPPORTED;
    }

    this->audio_stream_info_ = audio::AudioStreamInfo(stream_info.get_bits_per_sample(), this->output_channels_,
                                                      stream_info.get_sample_rate());
    this->output_speaker_->set_audio_stream_info(this->audio_stream_info_.value());
  } else {
    if (!this->queue_mode_ && (stream_info.get_sample_rate() != this->audio_stream_info_.value().get_sample_rate())) {
      // The two audio streams must have the same sample rate to mix properly if not in queue mode
      return ESP_ERR_INVALID_ARG;
    }
  }

  return this->start_task_();
}

esp_err_t MixerSpeaker::start_task_() {
  if (this->task_stack_buffer_ == nullptr) {
    if (this->task_stack_in_psram_) {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    } else {
      RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
      this->task_stack_buffer_ = stack_allocator.allocate(TASK_STACK_SIZE);
    }
  }

  if (this->task_stack_buffer_ == nullptr) {
    return ESP_ERR_NO_MEM;
  }

  if (this->task_handle_ == nullptr) {
    this->task_handle_ = xTaskCreateStatic(audio_mixer_task, "mixer", TASK_STACK_SIZE, (void *) this,
                                           MIXER_TASK_PRIORITY, this->task_stack_buffer_, &this->task_stack_);
  }

  if (this->task_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

esp_err_t MixerSpeaker::delete_task_() {
  if (!this->task_created_) {
    this->task_handle_ = nullptr;

    if (this->task_stack_buffer_ != nullptr) {
      if (this->task_stack_in_psram_) {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_EXTERNAL);
        stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
      } else {
        RAMAllocator<StackType_t> stack_allocator(RAMAllocator<StackType_t>::ALLOC_INTERNAL);
        stack_allocator.deallocate(this->task_stack_buffer_, TASK_STACK_SIZE);
      }

      this->task_stack_buffer_ = nullptr;
    }

    return ESP_OK;
  }

  return ESP_ERR_INVALID_STATE;
}

void MixerSpeaker::stop() { xEventGroupSetBits(this->event_group_, MixerEventGroupBits::COMMAND_STOP); }

void MixerSpeaker::copy_frames(const int16_t *input_buffer, audio::AudioStreamInfo input_stream_info,
                               int16_t *output_buffer, audio::AudioStreamInfo output_stream_info,
                               uint32_t frames_to_transfer) {
  uint8_t input_channels = input_stream_info.get_channels();
  uint8_t output_channels = output_stream_info.get_channels();
  const uint8_t max_input_channel_index = input_channels - 1;

  if (input_channels == output_channels) {
    size_t bytes_to_copy = input_stream_info.frames_to_bytes(frames_to_transfer);
    memcpy(output_buffer, input_buffer, bytes_to_copy);

    return;
  }

  for (uint32_t frame_index = 0; frame_index < frames_to_transfer; ++frame_index) {
    for (uint8_t output_channel_index = 0; output_channel_index < output_channels; ++output_channel_index) {
      uint8_t input_channel_index = std::min(output_channel_index, max_input_channel_index);
      output_buffer[output_channels * frame_index + output_channel_index] =
          input_buffer[input_channels * frame_index + input_channel_index];
    }
  }
}

void MixerSpeaker::mix_audio_samples(const int16_t *primary_buffer, audio::AudioStreamInfo primary_stream_info,
                                     const int16_t *secondary_buffer, audio::AudioStreamInfo secondary_stream_info,
                                     int16_t *output_buffer, audio::AudioStreamInfo output_stream_info,
                                     uint32_t frames_to_mix) {
  const uint8_t primary_channels = primary_stream_info.get_channels();
  const uint8_t secondary_channels = secondary_stream_info.get_channels();
  const uint8_t output_channels = output_stream_info.get_channels();

  const uint8_t max_primary_channel_index = primary_channels - 1;
  const uint8_t max_secondary_channel_index = secondary_channels - 1;

  for (uint32_t frames_index = 0; frames_index < frames_to_mix; ++frames_index) {
    for (uint8_t output_channel_index = 0; output_channel_index < output_channels; ++output_channel_index) {
      const uint32_t secondary_channel_index = std::min(output_channel_index, max_secondary_channel_index);
      const int32_t secondary_sample = secondary_buffer[frames_index * secondary_channels + secondary_channel_index];

      const uint32_t primary_channel_index = std::min(output_channel_index, max_primary_channel_index);
      const int32_t primary_sample =
          static_cast<int32_t>(primary_buffer[frames_index * primary_channels + primary_channel_index]);

      const int32_t added_sample = secondary_sample + primary_sample;

      output_buffer[frames_index * output_channels + output_channel_index] =
          static_cast<int16_t>(clamp<int32_t>(added_sample, MIN_AUDIO_SAMPLE_VALUE, MAX_AUDIO_SAMPLE_VALUE));
    }
  }
}

void MixerSpeaker::audio_mixer_task(void *params) {
  MixerSpeaker *this_mixer = (MixerSpeaker *) params;

  xEventGroupSetBits(this_mixer->event_group_, MixerEventGroupBits::STATE_STARTING);

  this_mixer->task_created_ = true;

  std::unique_ptr<audio::AudioSinkTransferBuffer> output_transfer_buffer = audio::AudioSinkTransferBuffer::create(
      this_mixer->audio_stream_info_.value().ms_to_bytes(TRANSFER_BUFFER_DURATION_MS));

  if (output_transfer_buffer == nullptr) {
    xEventGroupSetBits(this_mixer->event_group_,
                       MixerEventGroupBits::STATE_STOPPED | MixerEventGroupBits::ERR_ESP_NO_MEM);

    this_mixer->task_created_ = false;
    vTaskDelete(nullptr);
  }

  output_transfer_buffer->set_sink(this_mixer->output_speaker_);

  xEventGroupSetBits(this_mixer->event_group_, MixerEventGroupBits::STATE_RUNNING);

  bool sent_finished = false;

  while (true) {
    uint32_t event_group_bits = xEventGroupGetBits(this_mixer->event_group_);
    if (event_group_bits & MixerEventGroupBits::COMMAND_STOP) {
      break;
    }

    output_transfer_buffer->transfer_data_to_sink(pdMS_TO_TICKS(TASK_DELAY_MS));

    const uint32_t output_frames_free =
        this_mixer->audio_stream_info_.value().bytes_to_frames(output_transfer_buffer->free());

    std::vector<SourceSpeaker *> speakers_with_data;
    std::vector<std::shared_ptr<audio::AudioSourceTransferBuffer>> transfer_buffers_with_data;

    for (auto &speaker : this_mixer->source_speakers_) {
      if (speaker->get_transfer_buffer().use_count() > 0) {
        std::shared_ptr<audio::AudioSourceTransferBuffer> transfer_buffer = speaker->get_transfer_buffer().lock();
        speaker->process_data_from_source(0);  // Transfers and ducks audio from source ring buffers

        if ((transfer_buffer->available() > 0) && !speaker->get_pause_state()) {
          // Store the locked transfer buffers in their own vector to avoid releasing ownership until after the loop
          transfer_buffers_with_data.push_back(transfer_buffer);
          speakers_with_data.push_back(speaker);
        }
      }
    }

    if (transfer_buffers_with_data.empty()) {
      // No audio available for transferring, block task temporarily
      delay(TASK_DELAY_MS);
      continue;
    }

    uint32_t frames_to_mix = output_frames_free;

    if ((transfer_buffers_with_data.size() == 1) || this_mixer->queue_mode_) {
      // Only one speaker has audio data, just copy samples over

      audio::AudioStreamInfo active_stream_info = speakers_with_data[0]->get_audio_stream_info();

      if (active_stream_info.get_sample_rate() ==
          this_mixer->output_speaker_->get_audio_stream_info().get_sample_rate()) {
        // Speaker's sample rate matches the output speaker's, copy directly

        const uint32_t frames_available_in_buffer =
            active_stream_info.bytes_to_frames(transfer_buffers_with_data[0]->available());
        frames_to_mix = std::min(frames_to_mix, frames_available_in_buffer);
        copy_frames(reinterpret_cast<int16_t *>(transfer_buffers_with_data[0]->get_buffer_start()), active_stream_info,
                    reinterpret_cast<int16_t *>(output_transfer_buffer->get_buffer_end()),
                    this_mixer->audio_stream_info_.value(), frames_to_mix);

        // Update source speaker buffer length
        transfer_buffers_with_data[0]->decrease_buffer_length(active_stream_info.frames_to_bytes(frames_to_mix));
        speakers_with_data[0]->accumulated_frames_read_ += frames_to_mix;

        // Add new audio duration to the source speaker pending playback
        speakers_with_data[0]->pending_playback_ms_ +=
            active_stream_info.frames_to_milliseconds_with_remainder(&speakers_with_data[0]->accumulated_frames_read_);

        // Update output transfer buffer length
        output_transfer_buffer->increase_buffer_length(
            this_mixer->audio_stream_info_.value().frames_to_bytes(frames_to_mix));
      } else {
        // Speaker's stream info doesn't match the output speaker's, so it's a new source speaker
        if (!this_mixer->output_speaker_->is_stopped()) {
          if (!sent_finished) {
            this_mixer->output_speaker_->finish();
            sent_finished = true;  // Avoid repeatedly sending the finish command
          }
        } else {
          // Speaker has finished writing the current audio, update the stream information and restart the speaker
          this_mixer->audio_stream_info_ =
              audio::AudioStreamInfo(active_stream_info.get_bits_per_sample(), this_mixer->output_channels_,
                                     active_stream_info.get_sample_rate());
          this_mixer->output_speaker_->set_audio_stream_info(this_mixer->audio_stream_info_.value());
          this_mixer->output_speaker_->start();
          sent_finished = false;
        }
      }
    } else {
      // Determine how many frames to mix
      for (int i = 0; i < transfer_buffers_with_data.size(); ++i) {
        const uint32_t frames_available_in_buffer =
            speakers_with_data[i]->get_audio_stream_info().bytes_to_frames(transfer_buffers_with_data[i]->available());
        frames_to_mix = std::min(frames_to_mix, frames_available_in_buffer);
      }
      int16_t *primary_buffer = reinterpret_cast<int16_t *>(transfer_buffers_with_data[0]->get_buffer_start());
      audio::AudioStreamInfo primary_stream_info = speakers_with_data[0]->get_audio_stream_info();

      // Mix two streams together
      for (int i = 1; i < transfer_buffers_with_data.size(); ++i) {
        mix_audio_samples(primary_buffer, primary_stream_info,
                          reinterpret_cast<int16_t *>(transfer_buffers_with_data[i]->get_buffer_start()),
                          speakers_with_data[i]->get_audio_stream_info(),
                          reinterpret_cast<int16_t *>(output_transfer_buffer->get_buffer_end()),
                          this_mixer->audio_stream_info_.value(), frames_to_mix);

        speakers_with_data[i]->pending_playback_ms_ +=
            speakers_with_data[i]->get_audio_stream_info().frames_to_milliseconds_with_remainder(
                &speakers_with_data[i]->accumulated_frames_read_);

        if (i != transfer_buffers_with_data.size() - 1) {
          // Need to mix more streams together, point primary buffer and stream info to the already mixed output
          primary_buffer = reinterpret_cast<int16_t *>(output_transfer_buffer->get_buffer_end());
          primary_stream_info = this_mixer->audio_stream_info_.value();
        }
      }

      // Update source transfer buffer lengths and add new audio durations to the source speaker pending playbacks
      for (int i = 0; i < transfer_buffers_with_data.size(); ++i) {
        transfer_buffers_with_data[i]->decrease_buffer_length(
            speakers_with_data[i]->get_audio_stream_info().frames_to_bytes(frames_to_mix));
        speakers_with_data[i]->accumulated_frames_read_ += frames_to_mix;

        speakers_with_data[i]->pending_playback_ms_ +=
            speakers_with_data[i]->get_audio_stream_info().frames_to_milliseconds_with_remainder(
                &speakers_with_data[i]->accumulated_frames_read_);
      }

      // Update output transfer buffer length
      output_transfer_buffer->increase_buffer_length(
          this_mixer->audio_stream_info_.value().frames_to_bytes(frames_to_mix));
    }
  }

  xEventGroupSetBits(this_mixer->event_group_, MixerEventGroupBits::STATE_STOPPING);

  output_transfer_buffer.reset();

  xEventGroupSetBits(this_mixer->event_group_, MixerEventGroupBits::STATE_STOPPED);
  this_mixer->task_created_ = false;
  vTaskDelete(nullptr);
}

}  // namespace mixer_speaker
}  // namespace esphome

#endif
