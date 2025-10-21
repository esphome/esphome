#include "resampler_speaker.h"

#ifdef USE_ESP32

#include "esphome/components/audio/audio_resampler.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cstring>

namespace esphome {
namespace resampler {

static const UBaseType_t RESAMPLER_TASK_PRIORITY = 1;

static const uint32_t TRANSFER_BUFFER_DURATION_MS = 50;

static const uint32_t TASK_DELAY_MS = 20;
static const uint32_t TASK_STACK_SIZE = 3072;

static const char *const TAG = "resampler_speaker";

enum ResamplingEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),  // stops the resampler task
  STATE_STARTING = (1 << 10),
  STATE_RUNNING = (1 << 11),
  STATE_STOPPING = (1 << 12),
  STATE_STOPPED = (1 << 13),
  ERR_ESP_NO_MEM = (1 << 19),
  ERR_ESP_NOT_SUPPORTED = (1 << 20),
  ERR_ESP_FAIL = (1 << 21),
  ALL_BITS = 0x00FFFFFF,  // All valid FreeRTOS event group bits
};

void ResamplerSpeaker::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->output_speaker_->add_audio_output_callback([this](uint32_t new_frames, int64_t write_timestamp) {
    if (this->audio_stream_info_.get_sample_rate() != this->target_stream_info_.get_sample_rate()) {
      // Convert the number of frames from the target sample rate to the source sample rate. Track the remainder to
      // avoid losing frames from integer division truncation.
      const uint64_t numerator = new_frames * this->audio_stream_info_.get_sample_rate() + this->callback_remainder_;
      const uint64_t denominator = this->target_stream_info_.get_sample_rate();
      this->callback_remainder_ = numerator % denominator;
      this->audio_output_callback_(numerator / denominator, write_timestamp);
    } else {
      this->audio_output_callback_(new_frames, write_timestamp);
    }
  });
}

void ResamplerSpeaker::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & ResamplingEventGroupBits::STATE_STARTING) {
    ESP_LOGD(TAG, "Starting resampler task");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::STATE_STARTING);
  }

  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_NO_MEM) {
    this->status_set_error("Resampler task failed to allocate the internal buffers");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_NO_MEM);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED) {
    this->status_set_error("Cannot resample due to an unsupported audio stream");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_FAIL) {
    this->status_set_error("Resampler task failed");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
    this->state_ = speaker::STATE_STOPPING;
  }

  if (event_group_bits & ResamplingEventGroupBits::STATE_RUNNING) {
    ESP_LOGD(TAG, "Started resampler task");
    this->status_clear_error();
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::STATE_RUNNING);
  }
  if (event_group_bits & ResamplingEventGroupBits::STATE_STOPPING) {
    ESP_LOGD(TAG, "Stopping resampler task");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::STATE_STOPPING);
  }
  if (event_group_bits & ResamplingEventGroupBits::STATE_STOPPED) {
    if (this->delete_task_() == ESP_OK) {
      ESP_LOGD(TAG, "Stopped resampler task");
      xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ALL_BITS);
    }
  }

  switch (this->state_) {
    case speaker::STATE_STARTING: {
      esp_err_t err = this->start_();
      if (err == ESP_OK) {
        this->status_clear_error();
        this->state_ = speaker::STATE_RUNNING;
      } else {
        switch (err) {
          case ESP_ERR_INVALID_STATE:
            this->status_set_error("Failed to start resampler: resampler task failed to start");
            break;
          case ESP_ERR_NO_MEM:
            this->status_set_error("Failed to start resampler: not enough memory for task stack");
          default:
            this->status_set_error("Failed to start resampler");
            break;
        }

        this->state_ = speaker::STATE_STOPPING;
      }
      break;
    }
    case speaker::STATE_RUNNING:
      if (this->output_speaker_->is_stopped()) {
        this->state_ = speaker::STATE_STOPPING;
      }

      break;
    case speaker::STATE_STOPPING:
      this->stop_();
      this->state_ = speaker::STATE_STOPPED;
      break;
    case speaker::STATE_STOPPED:
      break;
  }
}

size_t ResamplerSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_stopped()) {
    this->start();
  }

  size_t bytes_written = 0;
  if ((this->output_speaker_->is_running()) && (!this->requires_resampling_())) {
    bytes_written = this->output_speaker_->play(data, length, ticks_to_wait);
  } else {
    if (this->ring_buffer_.use_count() == 1) {
      std::shared_ptr<RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
      bytes_written = temp_ring_buffer->write_without_replacement(data, length, ticks_to_wait);
    }
  }

  return bytes_written;
}

void ResamplerSpeaker::start() { this->state_ = speaker::STATE_STARTING; }

esp_err_t ResamplerSpeaker::start_() {
  this->target_stream_info_ = audio::AudioStreamInfo(
      this->target_bits_per_sample_, this->audio_stream_info_.get_channels(), this->target_sample_rate_);

  this->output_speaker_->set_audio_stream_info(this->target_stream_info_);
  this->output_speaker_->start();

  if (this->requires_resampling_()) {
    // Start the resampler task to handle converting sample rates
    return this->start_task_();
  }

  return ESP_OK;
}

esp_err_t ResamplerSpeaker::start_task_() {
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
    this->task_handle_ = xTaskCreateStatic(resample_task, "sample", TASK_STACK_SIZE, (void *) this,
                                           RESAMPLER_TASK_PRIORITY, this->task_stack_buffer_, &this->task_stack_);
  }

  if (this->task_handle_ == nullptr) {
    return ESP_ERR_INVALID_STATE;
  }

  return ESP_OK;
}

void ResamplerSpeaker::stop() { this->state_ = speaker::STATE_STOPPING; }

void ResamplerSpeaker::stop_() {
  if (this->task_handle_ != nullptr) {
    xEventGroupSetBits(this->event_group_, ResamplingEventGroupBits::COMMAND_STOP);
  }
  this->output_speaker_->stop();
}

esp_err_t ResamplerSpeaker::delete_task_() {
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

void ResamplerSpeaker::finish() { this->output_speaker_->finish(); }

bool ResamplerSpeaker::has_buffered_data() const {
  bool has_ring_buffer_data = false;
  if (this->requires_resampling_() && (this->ring_buffer_.use_count() > 0)) {
    has_ring_buffer_data = (this->ring_buffer_.lock()->available() > 0);
  }
  return (has_ring_buffer_data || this->output_speaker_->has_buffered_data());
}

void ResamplerSpeaker::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
  this->output_speaker_->set_mute_state(mute_state);
}

void ResamplerSpeaker::set_volume(float volume) {
  this->volume_ = volume;
  this->output_speaker_->set_volume(volume);
}

bool ResamplerSpeaker::requires_resampling_() const {
  return (this->audio_stream_info_.get_sample_rate() != this->target_sample_rate_) ||
         (this->audio_stream_info_.get_bits_per_sample() != this->target_bits_per_sample_);
}

void ResamplerSpeaker::resample_task(void *params) {
  ResamplerSpeaker *this_resampler = (ResamplerSpeaker *) params;

  this_resampler->task_created_ = true;
  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::STATE_STARTING);

  std::unique_ptr<audio::AudioResampler> resampler =
      make_unique<audio::AudioResampler>(this_resampler->audio_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS),
                                         this_resampler->target_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS));

  esp_err_t err = resampler->start(this_resampler->audio_stream_info_, this_resampler->target_stream_info_,
                                   this_resampler->taps_, this_resampler->filters_);

  if (err == ESP_OK) {
    std::shared_ptr<RingBuffer> temp_ring_buffer =
        RingBuffer::create(this_resampler->audio_stream_info_.ms_to_bytes(this_resampler->buffer_duration_ms_));

    if (temp_ring_buffer.use_count() == 0) {
      err = ESP_ERR_NO_MEM;
    } else {
      this_resampler->ring_buffer_ = temp_ring_buffer;
      resampler->add_source(this_resampler->ring_buffer_);

      this_resampler->output_speaker_->set_audio_stream_info(this_resampler->target_stream_info_);
      resampler->add_sink(this_resampler->output_speaker_);
    }
  }

  if (err == ESP_OK) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::STATE_RUNNING);
  } else if (err == ESP_ERR_NO_MEM) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NO_MEM);
  } else if (err == ESP_ERR_NOT_SUPPORTED) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED);
  }

  while (err == ESP_OK) {
    uint32_t event_bits = xEventGroupGetBits(this_resampler->event_group_);

    if (event_bits & ResamplingEventGroupBits::COMMAND_STOP) {
      break;
    }

    // Stop gracefully if the decoder is done
    int32_t ms_differential = 0;
    audio::AudioResamplerState resampler_state = resampler->resample(false, &ms_differential);

    if (resampler_state == audio::AudioResamplerState::FINISHED) {
      break;
    } else if (resampler_state == audio::AudioResamplerState::FAILED) {
      xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
      break;
    }
  }

  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::STATE_STOPPING);
  resampler.reset();
  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::STATE_STOPPED);
  this_resampler->task_created_ = false;
  vTaskDelete(nullptr);
}

}  // namespace resampler
}  // namespace esphome

#endif
