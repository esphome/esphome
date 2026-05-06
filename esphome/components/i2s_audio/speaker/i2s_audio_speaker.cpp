#include "i2s_audio_speaker.h"

#ifdef USE_ESP32

#include <driver/i2s_std.h>

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include "esp_timer.h"

// esp-audio-libs
#include <gain.h>

namespace esphome::i2s_audio {

static const char *const TAG = "i2s_audio.speaker";

// Software volume control maps the user-facing [0.0, 1.0] range to a Q31 scale factor.
// Volumes in (0.0, 1.0) map linearly to a dB reduction in [-49.0, 0.0] dB.
static constexpr float SOFTWARE_VOLUME_MIN_DB = -49.0f;

void I2SAudioSpeakerBase::setup() {
  this->event_group_ = xEventGroupCreate();

  if (this->event_group_ == nullptr) {
    ESP_LOGE(TAG, "Event group creation failed");
    this->mark_failed();
    return;
  }

  // Initialize volume control. When audio_dac is configured, this sets the DAC volume.
  // When no audio_dac is configured, this initializes software volume control.
  this->set_volume(this->volume_);
}

void I2SAudioSpeakerBase::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Speaker:\n"
                "  Pin: %d\n"
                "  Buffer duration: %" PRIu32,
                static_cast<int8_t>(this->dout_pin_), this->buffer_duration_ms_);
  if (this->timeout_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Timeout: %" PRIu32 " ms", this->timeout_.value());
  }
}

void I2SAudioSpeakerBase::loop() {
  uint32_t event_group_bits = xEventGroupGetBits(this->event_group_);

  if ((event_group_bits & SpeakerEventGroupBits::COMMAND_START) && (this->state_ == speaker::STATE_STOPPED)) {
    this->state_ = speaker::STATE_STARTING;
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
  }

  // Handle the task's state
  if (event_group_bits & SpeakerEventGroupBits::TASK_STARTING) {
    ESP_LOGD(TAG, "Starting");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STARTING);
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_RUNNING) {
    ESP_LOGV(TAG, "Started");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_RUNNING);
    this->state_ = speaker::STATE_RUNNING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPING) {
    ESP_LOGV(TAG, "Stopping");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::TASK_STOPPING);
    this->state_ = speaker::STATE_STOPPING;
  }
  if (event_group_bits & SpeakerEventGroupBits::TASK_STOPPED) {
    ESP_LOGD(TAG, "Stopped");

    vTaskDelete(this->speaker_task_handle_);
    this->speaker_task_handle_ = nullptr;

    this->stop_i2s_driver_();
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ALL_BITS);
    this->status_clear_error();

    this->on_task_stopped();

    this->state_ = speaker::STATE_STOPPED;
  }

  // Log any errors encountered by the task
  if (event_group_bits & SpeakerEventGroupBits::ERR_ESP_NO_MEM) {
    ESP_LOGE(TAG, "Not enough memory");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::ERR_ESP_NO_MEM);
  }

  // Warn if any playback timestamp events are dropped, which drastically reduces synced playback accuracy
  if (event_group_bits & SpeakerEventGroupBits::WARN_DROPPED_EVENT) {
    ESP_LOGW(TAG, "Event dropped, synchronized playback accuracy is reduced");
    xEventGroupClearBits(this->event_group_, SpeakerEventGroupBits::WARN_DROPPED_EVENT);
  }

  // Handle the speaker's state
  switch (this->state_) {
    case speaker::STATE_STARTING:
      if (this->status_has_error()) {
        break;
      }

      if (this->start_i2s_driver(this->audio_stream_info_) != ESP_OK) {
        ESP_LOGE(TAG, "Driver failed to start; retrying in 1 second");
        this->status_momentary_error("driver-failure", 1000);
        break;
      }

      if (this->speaker_task_handle_ == nullptr) {
        xTaskCreate(I2SAudioSpeakerBase::speaker_task, "speaker_task", TASK_STACK_SIZE, (void *) this, TASK_PRIORITY,
                    &this->speaker_task_handle_);

        if (this->speaker_task_handle_ == nullptr) {
          ESP_LOGE(TAG, "Task failed to start, retrying in 1 second");
          this->status_momentary_error("task-failure", 1000);
          this->stop_i2s_driver_();  // Stops the driver to return the lock; will be reloaded in next attempt
        }
      }
      break;
    case speaker::STATE_RUNNING:   // Intentional fallthrough
    case speaker::STATE_STOPPING:  // Intentional fallthrough
    case speaker::STATE_STOPPED:
      break;
  }
}

void I2SAudioSpeakerBase::set_volume(float volume) {
  this->volume_ = volume;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_ != nullptr) {
    if (volume > 0.0) {
      this->audio_dac_->set_mute_off();
    }
    this->audio_dac_->set_volume(volume);
  } else
#endif  // USE_AUDIO_DAC
  {
    // Fallback to software volume control by using a Q31 fixed point scaling factor.
    // At maximum volume (1.0), set to INT32_MAX to bypass volume processing entirely
    // and avoid any floating-point precision issues that could cause slight volume reduction.
    if (volume >= 1.0f) {
      this->q31_volume_factor_ = INT32_MAX;
    } else if (volume <= 0.0f) {
      this->q31_volume_factor_ = 0;
    } else {
      this->q31_volume_factor_ =
          esp_audio_libs::gain::db_to_q31(remap<float, float>(volume, 0.0f, 1.0f, SOFTWARE_VOLUME_MIN_DB, 0.0f));
    }
  }
}

void I2SAudioSpeakerBase::set_mute_state(bool mute_state) {
  this->mute_state_ = mute_state;
#ifdef USE_AUDIO_DAC
  if (this->audio_dac_) {
    if (mute_state) {
      this->audio_dac_->set_mute_on();
    } else {
      this->audio_dac_->set_mute_off();
    }
  } else
#endif  // USE_AUDIO_DAC
  {
    if (mute_state) {
      // Fallback to software volume control and scale by 0
      this->q31_volume_factor_ = 0;
    } else {
      // Revert to previous volume when unmuting
      this->set_volume(this->volume_);
    }
  }
}

size_t I2SAudioSpeakerBase::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setup failed; cannot play audio");
    return 0;
  }

  if (this->state_ != speaker::STATE_RUNNING && this->state_ != speaker::STATE_STARTING) {
    this->start();
  }

  if (this->state_ != speaker::STATE_RUNNING) {
    // Unable to write data to a running speaker, so delay the max amount of time so it can get ready
    vTaskDelay(ticks_to_wait);
    ticks_to_wait = 0;
  }

  size_t bytes_written = 0;
  if (this->state_ == speaker::STATE_RUNNING) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      // The weak_ptr locks successfully only while the speaker task owns the ring buffer, so it is safe to write
      bytes_written = temp_ring_buffer->write_without_replacement((void *) data, length, ticks_to_wait);
    }
  }

  return bytes_written;
}

bool I2SAudioSpeakerBase::has_buffered_data() const {
  if (this->audio_ring_buffer_.use_count() > 0) {
    std::shared_ptr<RingBuffer> temp_ring_buffer = this->audio_ring_buffer_.lock();
    return temp_ring_buffer->available() > 0;
  }
  return false;
}

void I2SAudioSpeakerBase::speaker_task(void *params) {
  I2SAudioSpeakerBase *this_speaker = (I2SAudioSpeakerBase *) params;
  this_speaker->run_speaker_task();
}

void I2SAudioSpeakerBase::start() {
  if (!this->is_ready() || this->is_failed() || this->status_has_error())
    return;
  if ((this->state_ == speaker::STATE_STARTING) || (this->state_ == speaker::STATE_RUNNING))
    return;

  // Mark STARTING immediately to avoid transient STOPPED observations before loop() processes COMMAND_START.
  this->state_ = speaker::STATE_STARTING;
  xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_START);
}

void I2SAudioSpeakerBase::stop() { this->stop_(false); }

void I2SAudioSpeakerBase::finish() { this->stop_(true); }

void I2SAudioSpeakerBase::stop_(bool wait_on_empty) {
  if (this->is_failed())
    return;
  if (this->state_ == speaker::STATE_STOPPED)
    return;

  if (wait_on_empty) {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP_GRACEFULLY);
  } else {
    xEventGroupSetBits(this->event_group_, SpeakerEventGroupBits::COMMAND_STOP);
  }
}

esp_err_t I2SAudioSpeakerBase::init_i2s_channel_(const i2s_chan_config_t &chan_cfg, const i2s_std_config_t &std_cfg,
                                                 size_t event_queue_size) {
  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, NULL);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2S channel allocation failed: %s", esp_err_to_name(err));
    this->parent_->unlock();
    return err;
  }

  err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize channel");
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    this->parent_->unlock();
    return err;
  }

  if (this->i2s_event_queue_ == nullptr) {
    this->i2s_event_queue_ = xQueueCreate(event_queue_size, sizeof(int64_t));
  } else {
    // Reset queue to clear any stale events from previous task
    xQueueReset(this->i2s_event_queue_);
  }

  return ESP_OK;
}

void I2SAudioSpeakerBase::stop_i2s_driver_() {
  if (this->tx_handle_ != nullptr) {
    i2s_channel_disable(this->tx_handle_);
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
  }
  this->parent_->unlock();
}

bool IRAM_ATTR I2SAudioSpeakerBase::i2s_on_sent_cb(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) {
  int64_t now = esp_timer_get_time();

  BaseType_t need_yield1 = pdFALSE;
  BaseType_t need_yield2 = pdFALSE;
  BaseType_t need_yield3 = pdFALSE;

  I2SAudioSpeakerBase *this_speaker = (I2SAudioSpeakerBase *) user_ctx;

  if (xQueueIsQueueFullFromISR(this_speaker->i2s_event_queue_)) {
    // Queue is full, so discard the oldest event and set the warning flag to inform the user
    int64_t dummy;
    xQueueReceiveFromISR(this_speaker->i2s_event_queue_, &dummy, &need_yield1);
    xEventGroupSetBitsFromISR(this_speaker->event_group_, SpeakerEventGroupBits::WARN_DROPPED_EVENT, &need_yield2);
  }

  xQueueSendToBackFromISR(this_speaker->i2s_event_queue_, &now, &need_yield3);

  return need_yield1 | need_yield2 | need_yield3;
}

void I2SAudioSpeakerBase::apply_software_volume_(uint8_t *data, size_t bytes_read) {
  if (this->q31_volume_factor_ == INT32_MAX) {
    return;  // Max volume, no processing needed
  }

  const size_t bytes_per_sample = this->current_stream_info_.samples_to_bytes(1);
  const uint32_t len = bytes_read / bytes_per_sample;

  esp_audio_libs::gain::apply(data, data, this->q31_volume_factor_, len, bytes_per_sample);
}

void I2SAudioSpeakerBase::swap_esp32_mono_samples_(uint8_t *data, size_t bytes_read) {
#ifdef USE_ESP32_VARIANT_ESP32
  // For ESP32 16-bit mono mode, adjacent samples need to be swapped.
  if (this->current_stream_info_.get_channels() == 1 && this->current_stream_info_.get_bits_per_sample() == 16) {
    int16_t *samples = reinterpret_cast<int16_t *>(data);
    size_t sample_count = bytes_read / sizeof(int16_t);
    for (size_t i = 0; i + 1 < sample_count; i += 2) {
      int16_t tmp = samples[i];
      samples[i] = samples[i + 1];
      samples[i + 1] = tmp;
    }
  }
#endif  // USE_ESP32_VARIANT_ESP32
}

}  // namespace esphome::i2s_audio

#endif  // USE_ESP32
