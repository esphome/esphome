#include "usb_audio_microphone.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO) && \
    (defined(USE_ESP32_VARIANT_ESP32P4) || defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3))

#include "esphome/core/log.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

extern "C" {
#include "esp_err.h"
}

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio.mic");

void USBAudioMicrophone::setup() {
  this->frame_.reserve(USB_AUDIO_MICROPHONE_BUFFER_SIZE);
  this->audio_stream_info_ = audio::AudioStreamInfo(this->bits_per_sample_, this->channels_, this->sample_rate_);
  if (!this->mic_task_.create(&USBAudioMicrophone::mic_task, "usb_mic", MIC_TASK_STACK_SIZE, this, MIC_TASK_PRIORITY,
                              false)) {
    ESP_LOGE(TAG, "Failed to create microphone task");
    this->mark_failed();
  }
}

void USBAudioMicrophone::dump_config() {
  // Channel indices are 0 or 1 ("0, 1" at most).
  char channel_list[8] = "all";
  if (this->enabled_channel_mask_ != 0) {
    size_t pos = 0;
    for (uint8_t i = 0; i < this->channels_; i++) {
      if ((this->enabled_channel_mask_ & (1U << i)) == 0)
        continue;
      pos += snprintf(channel_list + pos, sizeof(channel_list) - pos, pos == 0 ? "%u" : ", %u", i);
    }
  }
  ESP_LOGCONFIG(TAG,
                "USB Microphone:\n"
                "  Sample rate: %" PRIu32 " Hz\n"
                "  Bits per sample: %u\n"
                "  Source channels: %u\n"
                "  Enabled channels: %s\n"
                "  Buffer size: %u",
                this->sample_rate_, this->bits_per_sample_, this->channels_, channel_list,
                USB_AUDIO_MICROPHONE_BUFFER_SIZE);
}

void USBAudioMicrophone::start() {
  if (this->state_ == microphone::STATE_RUNNING || !this->mic_task_.is_created()) {
    return;
  }

  if (!this->parent_->ensure_started(USBAudioComponent::Endpoint::MICROPHONE)) {
    ESP_LOGE(TAG, "USB host not started");
    this->status_set_warning();
    return;
  }

  this->parent_->resume_microphone();

  this->task_idle_ = false;
  this->running_ = true;
  xTaskNotifyGive(this->mic_task_.get_handle());
  this->state_ = microphone::STATE_RUNNING;
  this->status_clear_warning();
}

void USBAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED) {
    return;
  }

  this->running_ = false;
  // Bounded by one read timeout.
  while (!this->task_idle_) {
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  this->parent_->suspend_microphone();
  this->state_ = microphone::STATE_STOPPED;
}

void USBAudioMicrophone::mic_task(void *param) { static_cast<USBAudioMicrophone *>(param)->mic_task_loop_(); }

void USBAudioMicrophone::mic_task_loop_() {
  while (true) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (this->running_) {
      size_t bytes_read = 0;
      this->frame_.resize(USB_AUDIO_MICROPHONE_BUFFER_SIZE);
      esp_err_t err =
          this->parent_->read_microphone(this->frame_.data(), this->frame_.size(), &bytes_read, READ_TIMEOUT_MS);
      if (!this->running_) {
        break;
      }

      if (err == ESP_OK && bytes_read > 0) {
        this->frame_.resize(bytes_read);
        this->apply_channel_selection_(this->frame_.data(), bytes_read);
        this->data_callbacks_.call(this->frame_);
      } else if (err == ESP_ERR_TIMEOUT || err == ESP_ERR_INVALID_STATE) {
        // No data yet, or the stream is starting or suspended.
        vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      } else if (err != ESP_OK) {
        ESP_LOGW(TAG, "Read error: %s", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      }
    }

    this->task_idle_ = true;
  }
}

void USBAudioMicrophone::apply_channel_selection_(uint8_t *data, size_t length) {
  const uint8_t all_channels = (1U << this->channels_) - 1;
  const uint8_t mask = this->enabled_channel_mask_ & all_channels;
  if (mask == 0 || mask == all_channels) {
    return;
  }

  const size_t bytes_per_sample = this->bits_per_sample_ / 8U;
  const size_t frame_stride = this->channels_ * bytes_per_sample;
  if (frame_stride == 0 || (length % frame_stride) != 0) {
    if (!this->logged_invalid_frame_) {
      ESP_LOGW(TAG, "Unable to apply channel selection: unexpected frame size (%u bytes)",
               static_cast<unsigned int>(length));
      this->logged_invalid_frame_ = true;
    }
    return;
  }

  // Copy the first enabled channel over every disabled one.
  const size_t primary_offset = __builtin_ctz(mask) * bytes_per_sample;
  for (uint8_t *frame = data; frame < data + length; frame += frame_stride) {
    for (uint8_t channel = 0; channel < this->channels_; channel++) {
      if ((mask & (1U << channel)) == 0) {
        std::memcpy(frame + channel * bytes_per_sample, frame + primary_offset, bytes_per_sample);
      }
    }
  }
}

}  // namespace esphome::usb_audio

#endif
