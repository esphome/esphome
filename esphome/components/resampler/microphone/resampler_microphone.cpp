#include "resampler_microphone.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::resampler {

static const UBaseType_t MAX_LISTENERS = 16;
static const UBaseType_t RESAMPLER_TASK_PRIORITY = 5;
static const uint32_t TASK_STACK_SIZE = 3072;
static const uint32_t TRANSFER_BUFFER_DURATION_MS = 16;
static const char *const TAG = "resampler_microphone";

enum ResamplingEventGroupBits : uint32_t {
  COMMAND_STOP = (1 << 0),
  TASK_STARTING = (1 << 10),
  TASK_RUNNING = (1 << 11),
  TASK_STOPPING = (1 << 12),
  TASK_STOPPED = (1 << 13),
  WARNING_FULL_RING_BUFFER = (1 << 17),
  ERR_ESP_NO_MEM = (1 << 19),
  ERR_ESP_NOT_SUPPORTED = (1 << 20),
  ERR_ESP_FAIL = (1 << 21),
  ALL_BITS = 0x00FFFFFF,
};

void ResamplerMicrophone::setup() {
  this->event_group_ = xEventGroupCreate();
  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create event group");
    this->mark_failed();
    return;
  }

  this->active_listeners_semaphore_ = xSemaphoreCreateCounting(MAX_LISTENERS, MAX_LISTENERS);
  if (this->active_listeners_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Creating semaphore failed");
    this->mark_failed();
    return;
  }

  this->microphone_source_->add_data_callback([this](const std::vector<uint8_t> &data) {
    if (this->state_ == microphone::STATE_STOPPED) {
      return;
    }
    if (this->requires_resampling_()) {
      std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
      if (temp_ring_buffer == nullptr) {
        return;
      }
      if (temp_ring_buffer->free() < data.size()) {
        xEventGroupSetBits(this->event_group_, ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER);
        temp_ring_buffer->reset();
      }
      temp_ring_buffer->write(data.data(), data.size());
    } else if (this->data_callbacks_.size() > 0) {
      this->data_callbacks_.call(data);
    }
  });

  this->configure_stream_settings_();
}

void ResamplerMicrophone::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if (event_group_bits & ResamplingEventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Starting");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_STARTING);
  }

  if (event_group_bits & ResamplingEventGroupBits::TASK_RUNNING) {
    ESP_LOGD(TAG, "Started");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_RUNNING);
    this->state_ = microphone::STATE_RUNNING;
    this->status_clear_error();
  }

  if (event_group_bits & ResamplingEventGroupBits::TASK_STOPPING) {
    ESP_LOGD(TAG, "Stopping");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::TASK_STOPPING);
  }

  if (event_group_bits & ResamplingEventGroupBits::TASK_STOPPED) {
    ESP_LOGD(TAG, "Stopped");
    this->task_.deallocate();
    this->ring_buffer_.reset();
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ALL_BITS);
    this->status_clear_error();
    this->state_ = microphone::STATE_STOPPED;
  }

  if (event_group_bits & ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER) {
    ESP_LOGW(TAG, "Ring buffer full, resetting it");
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::WARNING_FULL_RING_BUFFER);
  }

  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_NO_MEM) {
    ESP_LOGE(TAG, "Not enough memory");
    this->status_set_error(LOG_STR("Not enough memory"));
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_NO_MEM);
  }
  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED) {
    ESP_LOGE(TAG, "Unsupported stream");
    this->status_set_error(LOG_STR("Unsupported stream"));
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED);
  }
  if (event_group_bits & ResamplingEventGroupBits::ERR_ESP_FAIL) {
    ESP_LOGE(TAG, "Resampler failure");
    this->status_set_error(LOG_STR("Resampler failure"));
    xEventGroupClearBits(this->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
  }

  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) < MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_STOPPED)) {
    this->state_ = microphone::STATE_STARTING;
  }

  if ((uxSemaphoreGetCount(this->active_listeners_semaphore_) == MAX_LISTENERS) &&
      (this->state_ == microphone::STATE_RUNNING)) {
    this->state_ = microphone::STATE_STOPPING;
  }

  switch (this->state_) {
    case microphone::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }
      this->configure_stream_settings_();
      if (this->requires_resampling_()) {
        if (!this->task_.is_created()) {
          if (this->start_task_() != ESP_OK) {
            ESP_LOGE(TAG, "Task failed to start, retrying in 1 second");
            this->status_momentary_error("task_fail", 1000);
          } else {
            this->microphone_source_->start();
          }
        }
      } else {
        this->microphone_source_->start();
        this->state_ = microphone::STATE_RUNNING;
      }
      break;
    case microphone::STATE_RUNNING:
      break;
    case microphone::STATE_STOPPING:
      this->microphone_source_->stop();
      if (this->requires_resampling_()) {
        xEventGroupSetBits(this->event_group_, ResamplingEventGroupBits::COMMAND_STOP);
      } else {
        this->state_ = microphone::STATE_STOPPED;
      }
      break;
    case microphone::STATE_STOPPED:
      break;
  }
}

void ResamplerMicrophone::start() {
  if (this->is_failed())
    return;
  xSemaphoreTake(this->active_listeners_semaphore_, 0);
}

void ResamplerMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED || this->is_failed())
    return;
  xSemaphoreGive(this->active_listeners_semaphore_);
}

size_t ResamplerMicrophone::audio_sink_write(uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  (void) ticks_to_wait;
  if (length == 0 || this->data_callbacks_.size() == 0) {
    return length;
  }
  std::vector<uint8_t> output(data, data + length);
  this->data_callbacks_.call(output);
  return length;
}

esp_err_t ResamplerMicrophone::start_task_() {
  if (this->task_.create(resample_task, "resample_mic", TASK_STACK_SIZE, (void *) this, RESAMPLER_TASK_PRIORITY,
                         this->task_stack_in_psram_)) {
    return ESP_OK;
  }
  return ESP_ERR_NO_MEM;
}

void ResamplerMicrophone::configure_stream_settings_() {
  audio::AudioStreamInfo source_stream_info = this->microphone_source_->get_audio_stream_info();
  this->audio_stream_info_ = audio::AudioStreamInfo(source_stream_info.get_bits_per_sample(),
                                                    source_stream_info.get_channels(), this->target_sample_rate_);
}

bool ResamplerMicrophone::requires_resampling_() const {
  return this->microphone_source_->get_audio_stream_info().get_sample_rate() != this->target_sample_rate_;
}

void ResamplerMicrophone::resample_task(void *params) {
  auto *this_resampler = static_cast<ResamplerMicrophone *>(params);
  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STARTING);

  audio::AudioStreamInfo source_stream_info = this_resampler->microphone_source_->get_audio_stream_info();
  std::unique_ptr<audio::AudioResampler> resampler =
      make_unique<audio::AudioResampler>(source_stream_info.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS),
                                         this_resampler->audio_stream_info_.ms_to_bytes(TRANSFER_BUFFER_DURATION_MS));

  esp_err_t err = resampler->start(source_stream_info, this_resampler->audio_stream_info_, this_resampler->taps_,
                                   this_resampler->filters_);
  if (err == ESP_OK) {
    std::unique_ptr<ring_buffer::RingBuffer> ring_buffer =
        ring_buffer::RingBuffer::create(source_stream_info.ms_to_bytes(this_resampler->buffer_duration_ms_));
    if (ring_buffer == nullptr) {
      err = ESP_ERR_NO_MEM;
    } else {
      std::shared_ptr<ring_buffer::RingBuffer> shared_ring_buffer(std::move(ring_buffer));
      this_resampler->ring_buffer_ = shared_ring_buffer;
      err = resampler->add_source(this_resampler->ring_buffer_);
      if (err == ESP_OK) {
        err = resampler->add_sink(this_resampler);
      }
    }
  }

  if (err == ESP_OK) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_RUNNING);
  } else if (err == ESP_ERR_NO_MEM) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NO_MEM);
  } else if (err == ESP_ERR_NOT_SUPPORTED) {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_NOT_SUPPORTED);
  } else {
    xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
  }

  while (err == ESP_OK) {
    if (xEventGroupGetBits(this_resampler->event_group_) & ResamplingEventGroupBits::COMMAND_STOP) {
      break;
    }

    int32_t ms_differential = 0;
    audio::AudioResamplerState state = resampler->resample(false, &ms_differential);
    if (state == audio::AudioResamplerState::FAILED) {
      xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::ERR_ESP_FAIL);
      break;
    }
  }

  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STOPPING);
  resampler.reset();
  xEventGroupSetBits(this_resampler->event_group_, ResamplingEventGroupBits::TASK_STOPPED);

  vTaskSuspend(nullptr);
}

}  // namespace esphome::resampler

#endif  // USE_ESP32
