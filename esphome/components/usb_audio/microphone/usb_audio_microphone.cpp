#include "usb_audio_microphone.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "../usb_audio.h"

#include "esphome/core/log.h"

#include <cstring>
#include <string>

extern "C" {
#include "esp_err.h"
}

namespace esphome {
namespace usb_audio {

static const char *const TAG_MIC = "usb_audio.mic";

uint8_t USBAudioMicrophone::get_effective_channel_count_() const { return this->channels_; }

void USBAudioMicrophone::setup() {
  uint32_t buffer_size = this->parent_->get_microphone_buffer_size();
  if (buffer_size == 0) {
    buffer_size = USB_AUDIO_DEFAULT_BUFFER_SIZE;
  }
  this->read_buffer_.resize(buffer_size);

  this->audio_stream_info_ = audio::AudioStreamInfo(this->bits_per_sample_, this->channels_, this->sample_rate_);
}

void USBAudioMicrophone::dump_config() {
  ESP_LOGCONFIG(TAG_MIC, "USB Microphone:");
  ESP_LOGCONFIG(TAG_MIC, "  Sample rate: %u Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG_MIC, "  Bits per sample: %u", this->bits_per_sample_);
  ESP_LOGCONFIG(TAG_MIC, "  Source channels: %u", this->channels_);
  uint8_t effective_channels = this->get_effective_channel_count_();
  ESP_LOGCONFIG(TAG_MIC, "  Forwarded channels: %u", effective_channels);
  if (!this->enabled_channels_.empty()) {
    std::string channel_list;
    for (size_t i = 0; i < this->enabled_channels_.size(); ++i) {
      if (i != 0) {
        channel_list.append(", ");
      }
      channel_list.append(std::to_string(this->enabled_channels_[i]));
    }
    ESP_LOGCONFIG(TAG_MIC, "  Enabled channel indices: [%s]", channel_list.c_str());
  }
  ESP_LOGCONFIG(TAG_MIC, "  Buffer size: %u", static_cast<unsigned int>(this->read_buffer_.size()));
}

void USBAudioMicrophone::start() {
  if (this->state_ == microphone::STATE_RUNNING) {
    return;
  }

  if (!this->parent_->ensure_started(USBAudioComponent::Endpoint::MICROPHONE)) {
    ESP_LOGE(TAG_MIC, "USB host not started");
    this->status_set_warning();
    return;
  }

  this->parent_->resume_microphone();

  if (this->task_handle_ == nullptr) {
    this->running_ = true;
    BaseType_t created = xTaskCreate(&USBAudioMicrophone::mic_task, "usb_mic", MIC_TASK_STACK_SIZE, this,
                                     MIC_TASK_PRIORITY, &this->task_handle_);
    if (created != pdPASS) {
      ESP_LOGE(TAG_MIC, "Failed to create microphone task");
      this->running_ = false;
      this->task_handle_ = nullptr;
      this->status_set_warning();
      return;
    }
  }
  this->state_ = microphone::STATE_RUNNING;
  this->status_clear_warning();
}

void USBAudioMicrophone::stop() {
  if (this->state_ == microphone::STATE_STOPPED) {
    return;
  }

  this->running_ = false;
  if (this->task_handle_ != nullptr) {
    // Wait for the task to finish
    while (this->task_handle_ != nullptr) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }

  this->parent_->suspend_microphone();

  {
    LockGuard lock(this->queue_mutex_);
    this->pending_frames_.clear();
  }

  this->state_ = microphone::STATE_STOPPED;
}

void USBAudioMicrophone::loop() {
  std::deque<std::vector<uint8_t>> frames;
  {
    LockGuard lock(this->queue_mutex_);
    if (!this->pending_frames_.empty()) {
      frames.swap(this->pending_frames_);
    }
  }

  for (auto &frame : frames) {
    this->data_callbacks_.call(frame);
  }
}

void USBAudioMicrophone::mic_task(void *param) {
  auto *self = static_cast<USBAudioMicrophone *>(param);
  if (self == nullptr) {
    vTaskDelete(nullptr);
    return;
  }
  self->mic_task_loop_();
  vTaskDelete(nullptr);
}

void USBAudioMicrophone::mic_task_loop_() {
  while (this->running_) {
    size_t bytes_read = 0;
    esp_err_t err = this->parent_->read_microphone(this->read_buffer_.data(), this->read_buffer_.size(), &bytes_read,
                                                   READ_TIMEOUT_MS);

    if (!this->running_) {
      break;
    }

    if (err == ESP_OK && bytes_read > 0) {
      this->enqueue_frame_(this->read_buffer_.data(), bytes_read);
    } else if (err == ESP_ERR_TIMEOUT) {
      // No data available right now; yield to avoid starving lower priority tasks.
      vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      continue;
    } else if (err == ESP_ERR_INVALID_STATE) {
      // Stream not active yet (e.g. device starting or suspended); yield before retrying.
      vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
      continue;
    } else if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
      ESP_LOGW(TAG_MIC, "Read error: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(READ_TIMEOUT_MS));
    }
  }

  this->task_handle_ = nullptr;
}

void USBAudioMicrophone::enqueue_frame_(const uint8_t *data, size_t length) {
  std::vector<uint8_t> frame(data, data + length);

  if (!this->enabled_channels_.empty()) {
    const size_t bytes_per_sample = static_cast<size_t>(this->bits_per_sample_) / 8U;
    const size_t channel_stride = static_cast<size_t>(this->channels_) * bytes_per_sample;

    if (bytes_per_sample == 0 || channel_stride == 0 || (length % channel_stride) != 0) {
      if (!this->logged_invalid_frame_) {
        ESP_LOGW(TAG_MIC, "Unable to apply channel selection: unexpected frame size (%u bytes)",
                 static_cast<unsigned int>(length));
        this->logged_invalid_frame_ = true;
      }
    } else {
      std::vector<bool> enabled_map(this->channels_, false);
      for (uint8_t enabled_channel : this->enabled_channels_) {
        if (enabled_channel < this->channels_) {
          enabled_map[enabled_channel] = true;
        } else if (!this->logged_invalid_channel_) {
          ESP_LOGW(TAG_MIC, "Enabled channel index %u exceeds available source channels", enabled_channel);
          this->logged_invalid_channel_ = true;
        }
      }

      size_t primary_channel = 0;
      bool primary_found = false;
      for (size_t channel_index = 0; channel_index < enabled_map.size(); ++channel_index) {
        if (enabled_map[channel_index]) {
          primary_channel = channel_index;
          primary_found = true;
          break;
        }
      }

      if (primary_found) {
        const size_t sample_count = length / channel_stride;
        for (size_t sample_index = 0; sample_index < sample_count; ++sample_index) {
          uint8_t *sample_base = frame.data() + sample_index * channel_stride;
          const uint8_t *primary_source = sample_base + primary_channel * bytes_per_sample;

          for (size_t channel_index = 0; channel_index < this->channels_; ++channel_index) {
            if (!enabled_map[channel_index]) {
              std::memcpy(sample_base + channel_index * bytes_per_sample, primary_source, bytes_per_sample);
            }
          }
        }
      }
    }
  }

  LockGuard lock(this->queue_mutex_);
  if (this->pending_frames_.size() >= MAX_QUEUED_FRAMES) {
    this->pending_frames_.pop_front();
  }
  this->pending_frames_.push_back(std::move(frame));
}

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
