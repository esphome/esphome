#include "usb_audio_speaker.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/components/usb_audio/usb_audio.h"

#include "esphome/core/log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace esphome {
namespace usb_audio {

static const char *const TAG_SPK = "usb_audio.spk";

void USBAudioSpeaker::setup() {
  this->audio_stream_info_ = audio::AudioStreamInfo(this->bits_per_sample_, this->channels_, this->sample_rate_);
}

void USBAudioSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG_SPK, "USB Speaker:");
  ESP_LOGCONFIG(TAG_SPK, "  Sample rate: %u Hz", this->sample_rate_);
  ESP_LOGCONFIG(TAG_SPK, "  Bits per sample: %u", this->bits_per_sample_);
  ESP_LOGCONFIG(TAG_SPK, "  Channels: %u", this->channels_);
  ESP_LOGCONFIG(TAG_SPK, "  Write timeout: %u ms", this->write_timeout_ms_);
  if (this->channels_ > 2) {
    ESP_LOGW(TAG_SPK, "USB speaker only supports mono or stereo playback; additional channels will be ignored");
  }
}

void USBAudioSpeaker::loop() {
  if (this->finish_requested_) {
    if (!this->has_buffered_data() || millis() > this->finish_deadline_ms_) {
      this->stop();
      this->finish_requested_ = false;
    }
  }
}

void USBAudioSpeaker::start() {
  if (this->state_ == speaker::STATE_RUNNING) {
    return;
  }
  if (!this->ensure_started_()) {
    ESP_LOGE(TAG_SPK, "USB host not started");
    this->status_set_warning();
    return;
  }
  this->parent_->resume_speaker();
  this->state_ = speaker::STATE_RUNNING;
  this->status_clear_warning();
}

void USBAudioSpeaker::stop() {
  if (this->state_ == speaker::STATE_STOPPED) {
    return;
  }
  this->parent_->suspend_speaker();
  this->state_ = speaker::STATE_STOPPED;
  this->finish_requested_ = false;
}

void USBAudioSpeaker::finish() {
  if (!this->is_running()) {
    this->stop();
    return;
  }
  this->finish_requested_ = true;
  this->finish_deadline_ms_ = millis() + (this->write_timeout_ms_ * 3);
}

size_t USBAudioSpeaker::play(const uint8_t *data, size_t length) {
  if (!this->is_running()) {
    this->start();
  }
  if (!this->is_running()) {
    return 0;
  }

  esp_err_t err = this->parent_->write_speaker(data, length, this->write_timeout_ms_);
  if (err == ESP_OK) {
    this->last_write_ms_ = millis();
    return length;
  }
  if (err == ESP_ERR_TIMEOUT) {
    // Yield briefly to give the USB host task time to drain its buffers.
    vTaskDelay(pdMS_TO_TICKS(this->write_timeout_ms_));
  } else {
    ESP_LOGW(TAG_SPK, "Write error: %s", esp_err_to_name(err));
    vTaskDelay(pdMS_TO_TICKS(this->write_timeout_ms_));
  }
  return 0;
}

bool USBAudioSpeaker::has_buffered_data() const {
  if (!this->is_running()) {
    return false;
  }
  const uint32_t now = millis();
  return (now - this->last_write_ms_) < (this->write_timeout_ms_ * 2);
}

void USBAudioSpeaker::set_volume(float volume) {
  speaker::Speaker::set_volume(volume);
  if (this->parent_ != nullptr) {
    this->parent_->set_speaker_volume_level(volume);
  }
}

void USBAudioSpeaker::set_mute_state(bool mute_state) {
  speaker::Speaker::set_mute_state(mute_state);
  if (this->parent_ != nullptr) {
    this->parent_->set_speaker_mute_state(mute_state);
  }
}

bool USBAudioSpeaker::ensure_started_() { return this->parent_->ensure_started(USBAudioComponent::Endpoint::SPEAKER); }

}  // namespace usb_audio
}  // namespace esphome

#endif  // defined(USE_ESP32) && defined(USE_USB_AUDIO)
