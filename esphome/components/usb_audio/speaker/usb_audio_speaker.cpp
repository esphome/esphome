#include "usb_audio_speaker.h"

#ifdef USE_USB_AUDIO

#include <algorithm>
#include <cinttypes>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_timer.h"

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio.spk");

static constexpr size_t MAX_TIMEOUT_RETRIES = 4;

void USBAudioSpeaker::setup() {
  const auto &info = this->audio_stream_info_;
  this->write_chunk_ = info.frames_to_bytes(std::max<uint32_t>(info.bytes_to_frames(this->buffer_size_ / 4), 1));
  // start(), stop() and finish() wake the loop.
  this->disable_loop();
}

void USBAudioSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "USB Speaker:\n"
                "  Sample rate: %" PRIu32 " Hz\n"
                "  Bits per sample: %u\n"
                "  Channels: %u\n"
                "  Buffer size: %" PRIu32 "\n"
                "  Write timeout: %" PRIu32 " ms",
                this->audio_stream_info_.get_sample_rate(), this->audio_stream_info_.get_bits_per_sample(),
                this->audio_stream_info_.get_channels(), this->buffer_size_, this->write_timeout_ms_);
}

void USBAudioSpeaker::loop() {
  const uint8_t commands = this->commands_.exchange(0);
  if (commands & COMMAND_STOP) {
    this->stop_();
  }
  if ((commands & COMMAND_START) && this->state_ == speaker::STATE_STOPPED) {
    this->state_ = speaker::STATE_STARTING;
    // Answers through on_stream_state() now or once the device connects.
    this->parent_->resume(Endpoint::SPEAKER);
  }

  if (commands & COMMAND_FINISH) {
    this->finishing_ = true;
  }
  // has_buffered_data_() turns false once the queued audio has had time to play out.
  if (this->finishing_ && !this->has_buffered_data_(App.get_loop_component_start_time())) {
    this->stop_();
  }
  if (!this->finishing_) {
    this->disable_loop();
  }
}

void USBAudioSpeaker::request_(uint8_t set, uint8_t clear) {
  this->commands_.fetch_and(static_cast<uint8_t>(~clear));
  this->commands_.fetch_or(set);
  this->enable_loop_soon_any_context();
}

void USBAudioSpeaker::start() {
  if (this->state_ == speaker::STATE_STARTING || this->state_ == speaker::STATE_RUNNING) {
    return;
  }
  this->request_(COMMAND_START, COMMAND_STOP);
}

void USBAudioSpeaker::stop() { this->request_(COMMAND_STOP, COMMAND_START | COMMAND_FINISH); }

void USBAudioSpeaker::finish() { this->request_(COMMAND_FINISH, 0); }

void USBAudioSpeaker::stop_() {
  this->finishing_ = false;
  if (this->state_ == speaker::STATE_STOPPED) {
    return;
  }
  this->parent_->suspend(Endpoint::SPEAKER);
  this->state_ = speaker::STATE_STOPPED;
}

void USBAudioSpeaker::on_stream_state(bool running) {
  if (running) {
    if (this->state_ == speaker::STATE_STARTING) {
      this->state_ = speaker::STATE_RUNNING;
    }
    return;
  }
  if (this->state_ == speaker::STATE_RUNNING) {
    ESP_LOGW(TAG, "USB device disconnected, stopping playback");
  }
  this->stop_();
}

size_t USBAudioSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  // 0 ticks is one non-blocking pass; portMAX_DELAY is capped by the configured write timeout.
  const uint32_t budget_ms = ticks_to_wait == portMAX_DELAY ? this->write_timeout_ms_ : pdTICKS_TO_MS(ticks_to_wait);
  return this->play_internal_(data, length, budget_ms);
}

// Callers such as voice_assistant and rtttl call this from loop(), so it never blocks.
size_t USBAudioSpeaker::play(const uint8_t *data, size_t length) { return this->play(data, length, 0); }

size_t USBAudioSpeaker::play_internal_(const uint8_t *data, size_t length, uint32_t budget_ms) {
  if (this->state_ != speaker::STATE_RUNNING) {
    this->start();
    // Wait out the budget so a pipeline task retrying play() does not spin.
    if (budget_ms > 0) {
      vTaskDelay(pdMS_TO_TICKS(budget_ms));
    }
    return 0;
  }

  // Counted before the hub's handle is read, so the hub never closes the device under this call.
  this->writes_in_flight_.fetch_add(1);
  if (this->parent_->speaker_controls_pending()) {
    this->parent_->apply_pending_speaker_controls();
  }

  // Runs on the caller's task, so the cached loop time does not apply.
  const uint32_t start_ms = millis();
  uint32_t now = start_ms;
  size_t written = 0;
  size_t timeout_retries = 0;
  while (written < length) {
    const uint32_t elapsed = now - start_ms;
    if (budget_ms > 0 && elapsed >= budget_ms) {
      break;
    }
    const size_t chunk = std::min<size_t>(length - written, this->write_chunk_);
    const uint32_t timeout_ms = budget_ms > 0 ? budget_ms - elapsed : 0;
    const esp_err_t err = this->parent_->write_speaker(data + written, chunk, timeout_ms);
    now = millis();
    if (err == ESP_OK) {
      written += chunk;
      timeout_retries = 0;
    } else if (err == ESP_ERR_INVALID_STATE) {
      // The device went away.
      break;
    } else if (err != ESP_ERR_TIMEOUT) {
      ESP_LOGW(TAG, "Write error: %s", esp_err_to_name(err));
      break;
    } else if (budget_ms == 0 || ++timeout_retries >= MAX_TIMEOUT_RETRIES) {
      // A full driver buffer is back-pressure; the caller retries.
      break;
    }
  }
  this->writes_in_flight_.fetch_sub(1);

  if (written > 0) {
    // Whatever was queued at the last write has partly played out since; the driver holds at most buffer_size_.
    const auto &info = this->audio_stream_info_;
    const uint32_t queued = this->queued_bytes_;
    const uint32_t elapsed_ms = now - this->last_write_ms_;
    const uint32_t remaining = elapsed_ms < info.bytes_to_ms(queued) ? queued - info.ms_to_bytes(elapsed_ms) : 0;
    this->queued_bytes_ = std::min<uint32_t>(remaining + written, this->buffer_size_);
    this->last_write_ms_ = now;
    this->audio_output_callback_(this->audio_stream_info_.bytes_to_frames(written), esp_timer_get_time());
  }
  return written;
}

bool USBAudioSpeaker::has_buffered_data_(uint32_t now) const {
  // Signed, since the cached loop time can be a little older than a write made on another task.
  const auto elapsed_ms = static_cast<int32_t>(now - this->last_write_ms_);
  return this->state_ == speaker::STATE_RUNNING &&
         elapsed_ms < static_cast<int32_t>(this->audio_stream_info_.bytes_to_ms(this->queued_bytes_));
}

bool USBAudioSpeaker::has_buffered_data() const { return this->has_buffered_data_(millis()); }

void USBAudioSpeaker::set_volume(float volume) {
  speaker::Speaker::set_volume(volume);
  this->parent_->set_speaker_volume_level(volume);
}

void USBAudioSpeaker::set_mute_state(bool mute_state) {
  speaker::Speaker::set_mute_state(mute_state);
  this->parent_->set_speaker_mute_state(mute_state);
}

}  // namespace esphome::usb_audio

#endif
