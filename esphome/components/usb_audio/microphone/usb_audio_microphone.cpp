#include "usb_audio_microphone.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/core/log.h"

extern "C" {
#include "esp_err.h"
}

namespace esphome {
namespace usb_audio {

static const char *const TAG_MIC = "usb_audio.mic";

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
  ESP_LOGCONFIG(TAG_MIC, "  Channels: %u (mono only)", this->channels_);
  ESP_LOGCONFIG(TAG_MIC, "  Buffer size: %u", static_cast<unsigned int>(this->read_buffer_.size()));
}

void USBAudioMicrophone::start() {
  if (this->state_ == microphone::STATE_RUNNING) {
    return;
  }

  if (!this->parent_->ensure_started()) {
    ESP_LOGE(TAG_MIC, "USB host not started");
    this->status_set_warning();
    return;
  }

  this->parent_->resume_microphone();

  if (this->task_handle_ == nullptr) {
    this->running_ = true;
    BaseType_t created = xTaskCreate(&USBAudioMicrophone::mic_task_, "usb_mic", MIC_TASK_STACK_SIZE, this,
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

void USBAudioMicrophone::mic_task_(void *param) {
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
      // Expected when there is no new data within the timeout window
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

  LockGuard lock(this->queue_mutex_);
  if (this->pending_frames_.size() >= MAX_QUEUED_FRAMES) {
    this->pending_frames_.pop_front();
  }
  this->pending_frames_.push_back(std::move(frame));
}

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
