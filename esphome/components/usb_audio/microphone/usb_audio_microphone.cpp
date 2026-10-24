#include "usb_audio_microphone.h"

#ifdef USE_USB_AUDIO

#include "esphome/core/log.h"

#include <algorithm>
#include <cinttypes>

extern "C" {
#include "esp_err.h"
}

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio.mic");

void USBAudioMicrophone::setup() {
  this->frame_.reserve(this->audio_stream_info_.ms_to_bytes(2 * READ_TIMEOUT_MS));
  if (!this->mic_task_.create(&USBAudioMicrophone::mic_task, "usb_mic", MIC_TASK_STACK_SIZE, this, MIC_TASK_PRIORITY,
                              false)) {
    this->mark_failed(LOG_STR("Failed to create microphone task"));
    return;
  }
  // start() and stop() wake the loop.
  this->disable_loop();
}

void USBAudioMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USB Microphone:\n"
                "  Sample rate: %" PRIu32 " Hz\n"
                "  Bits per sample: %u\n"
                "  Channels: %u\n"
                "  Buffer size: %" PRIu32,
                this->audio_stream_info_.get_sample_rate(), this->audio_stream_info_.get_bits_per_sample(),
                this->audio_stream_info_.get_channels(), this->buffer_size_);
}

void USBAudioMicrophone::loop() {
  const uint8_t commands = this->commands_.exchange(0);
  if (commands & COMMAND_STOP) {
    // Also when a failed start left the microphone STOPPED, so the next session's retries start from the minimum.
    this->cancel_timeout(RETRY_START_ID);
    this->retry_delay_ms_ = RETRY_MIN_MS;
  }
  if ((commands & COMMAND_STOP) &&
      (this->state_ == microphone::STATE_STARTING || this->state_ == microphone::STATE_RUNNING)) {
    this->parent_->suspend(Endpoint::MICROPHONE);
    // The task notices within one read timeout; the stop finishes below once it is idle.
    this->running_ = false;
    this->state_ = microphone::STATE_STOPPING;
  }
  if (this->state_ == microphone::STATE_STOPPING && this->task_idle_) {
    this->state_ = microphone::STATE_STOPPED;
  }
  // A retry can race the last stop(), so only start while someone is still listening.
  if ((commands & COMMAND_START) && this->listeners_ > 0) {
    if (this->state_ == microphone::STATE_STOPPED) {
      this->state_ = microphone::STATE_STARTING;
      // Answers through on_stream_state() now or once the device connects.
      this->parent_->resume(Endpoint::MICROPHONE);
    } else if (this->state_ == microphone::STATE_STOPPING) {
      // Retry once the task is idle.
      this->commands_.fetch_or(COMMAND_START);
    }
  }
  if (this->state_ != microphone::STATE_STOPPING) {
    this->disable_loop();
  }
}

void USBAudioMicrophone::request_(uint8_t set, uint8_t clear) {
  this->commands_.fetch_and(static_cast<uint8_t>(~clear));
  this->commands_.fetch_or(set);
  this->enable_loop_soon_any_context();
}

// Every MicrophoneSource forwards its own start() and stop(), so capture runs while any of them is listening, like
// i2s_audio's listener count.
void USBAudioMicrophone::start() {
  if (this->is_failed()) {
    return;
  }
  uint8_t listeners = this->listeners_.load();
  do {
    if (listeners >= MAX_LISTENERS) {
      return;
    }
  } while (!this->listeners_.compare_exchange_weak(listeners, listeners + 1));
  if (listeners == 0) {
    this->request_(COMMAND_START, COMMAND_STOP);
  }
}

void USBAudioMicrophone::stop() {
  uint8_t listeners = this->listeners_.load();
  do {
    if (listeners == 0) {
      return;
    }
  } while (!this->listeners_.compare_exchange_weak(listeners, listeners - 1));
  if (listeners == 1) {
    this->request_(COMMAND_STOP, COMMAND_START);
  }
}

void USBAudioMicrophone::on_stream_state(StreamEvent event) {
  switch (event) {
    case StreamEvent::RUNNING:
      if (this->state_ == microphone::STATE_STARTING) {
        this->task_idle_ = false;
        this->running_ = true;
        xTaskNotifyGive(this->mic_task_.get_handle());
        this->state_ = microphone::STATE_RUNNING;
        this->retry_delay_ms_ = RETRY_MIN_MS;
      }
      break;
    case StreamEvent::LOST:
      // The hub restarts the stream on reconnect, since the microphone still wants it, so keep waiting in STARTING;
      // this also covers a disconnect while the stream was still starting.
      if (this->state_ == microphone::STATE_RUNNING) {
        ESP_LOGW(TAG, "USB device disconnected, waiting for it to return");
        this->running_ = false;
        this->state_ = microphone::STATE_STARTING;
      }
      break;
    case StreamEvent::FAILED:
      // Only a start or resume fails, so the task is idle.
      if (this->state_ == microphone::STATE_STARTING) {
        this->state_ = microphone::STATE_STOPPED;
        // A MicrophoneSource calls start() once and then waits for data, and start() only asks for the stream when
        // the first listener arrives, so retry here for the listeners still held.
        if (this->listeners_ > 0) {
          this->set_timeout(RETRY_START_ID, this->retry_delay_ms_, [this]() {
            if (this->listeners_ > 0) {
              this->request_(COMMAND_START, 0);
            }
          });
          this->retry_delay_ms_ = std::min(this->retry_delay_ms_ * 2, RETRY_MAX_MS);
        }
      }
      break;
  }
}

void USBAudioMicrophone::mic_task(void *param) { static_cast<USBAudioMicrophone *>(param)->mic_task_loop_(); }

void USBAudioMicrophone::mic_task_loop_() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (this->running_) {
      size_t bytes_read = 0;
      this->frame_.resize(this->frame_.capacity());
      esp_err_t err =
          this->parent_->read_microphone(this->frame_.data(), this->frame_.size(), &bytes_read, READ_TIMEOUT_MS);
      if (!this->running_) {
        break;
      }

      if (err == ESP_OK && bytes_read > 0) {
        this->frame_.resize(bytes_read);
        this->data_callbacks_.call(this->frame_);
      } else if (err == ESP_ERR_TIMEOUT) {
        // The driver already waited READ_TIMEOUT_MS for data; read again.
      } else if (err == ESP_ERR_INVALID_STATE) {
        // The stream is starting or suspended.
        vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Read error: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      }
    }

    this->task_idle_ = true;
  }
}

}  // namespace esphome::usb_audio

#endif
