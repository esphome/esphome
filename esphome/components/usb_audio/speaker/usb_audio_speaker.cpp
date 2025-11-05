#include "usb_audio_speaker.h"

#if defined(USE_ESP32) && defined(USE_USB_AUDIO)

#include "esphome/components/usb_audio/usb_audio.h"

#include <algorithm>

#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_timer.h"
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

namespace {
constexpr size_t MIN_WRITE_CHUNK = 512;
constexpr size_t CHUNK_GROW_STEP = 512;
constexpr size_t MAX_TIMEOUT_RETRIES = 4;
constexpr size_t SUCCESS_STREAK_FOR_GROWTH = 16;
constexpr size_t HARD_CHUNK_CAP = 4096;
constexpr uint32_t MAX_WORK_TIME_MS = 20;
}  // namespace

size_t USBAudioSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  uint32_t time_budget_ms = this->write_timeout_ms_;
  if (ticks_to_wait != portMAX_DELAY) {
    uint32_t ticks_ms = static_cast<uint32_t>(ticks_to_wait) * portTICK_PERIOD_MS;
    if (ticks_ms > 0) {
      time_budget_ms = std::min(time_budget_ms, ticks_ms);
    }
  }
  return this->play_internal_(data, length, time_budget_ms);
}

size_t USBAudioSpeaker::play(const uint8_t *data, size_t length) {
  return this->play_internal_(data, length, this->write_timeout_ms_);
}

size_t USBAudioSpeaker::play_internal_(const uint8_t *data, size_t length, uint32_t time_budget_ms) {
  if (!this->is_running()) {
    this->start();
  }
  if (!this->is_running() || data == nullptr || length == 0) {
    return 0;
  }

  const uint32_t work_budget_ms =
      (time_budget_ms > 0) ? std::min<uint32_t>(time_budget_ms, MAX_WORK_TIME_MS) : MAX_WORK_TIME_MS;
  const uint32_t start_ms = millis();

  size_t total_written = 0;
  size_t remaining = length;
  const uint8_t *current = data;

  size_t max_chunk = this->parent_->get_speaker_buffer_size();
  if (max_chunk == 0) {
    max_chunk = HARD_CHUNK_CAP;
  }
  max_chunk = std::max(std::min(max_chunk, HARD_CHUNK_CAP), MIN_WRITE_CHUNK);

  const size_t bytes_per_frame =
      std::max<size_t>(1, static_cast<size_t>(this->channels_) * std::max<size_t>(1, this->bits_per_sample_ / 8));
  auto align_to_frame = [bytes_per_frame](size_t value) -> size_t {
    if (bytes_per_frame <= 1) {
      return value;
    }
    return (value / bytes_per_frame) * bytes_per_frame;
  };

  size_t timeout_retry_count = 0;

  while (remaining > 0) {
    uint32_t elapsed_ms = millis() - start_ms;
    if (elapsed_ms >= work_budget_ms) {
      break;
    }

    size_t current_upper_bound = this->chunk_upper_bound_;
    if (current_upper_bound == 0) {
      current_upper_bound = std::min(std::max(MIN_WRITE_CHUNK, max_chunk), HARD_CHUNK_CAP);
      size_t aligned_bound = align_to_frame(current_upper_bound);
      if (aligned_bound > 0) {
        current_upper_bound = aligned_bound;
      }
      this->chunk_upper_bound_ = current_upper_bound;
    }

    size_t chunk_cap = remaining;
    chunk_cap = std::min(chunk_cap, current_upper_bound);
    chunk_cap = std::min(chunk_cap, max_chunk);
    chunk_cap = std::min(chunk_cap, HARD_CHUNK_CAP);

    if (chunk_cap == 0) {
      ESP_LOGW(TAG_SPK, "Chunk cap resolved to zero; aborting playback write");
      break;
    }

    size_t chunk = this->preferred_chunk_size_;
    if (chunk == 0 || chunk > chunk_cap) {
      chunk = chunk_cap;
    }

    size_t aligned_chunk = align_to_frame(chunk);
    if (aligned_chunk > 0) {
      chunk = aligned_chunk;
    } else {
      aligned_chunk = align_to_frame(chunk_cap);
      if (aligned_chunk > 0) {
        chunk = aligned_chunk;
      }
    }

    if (chunk > chunk_cap) {
      chunk = chunk_cap;
    }
    if (chunk > remaining) {
      chunk = remaining;
    }
    if (chunk == 0) {
      ESP_LOGW(TAG_SPK, "Chunk resolved to zero after alignment; aborting playback write");
      break;
    }

    uint32_t remaining_budget_ms = work_budget_ms - elapsed_ms;
    if (remaining_budget_ms == 0) {
      remaining_budget_ms = 1;
    }
    uint32_t call_timeout_ms = std::min<uint32_t>(this->write_timeout_ms_, remaining_budget_ms);
    if (call_timeout_ms == 0) {
      call_timeout_ms = 1;
    }

    ESP_LOGV(TAG_SPK, "Attempting USB write chunk=%u remaining=%u timeout_ms=%u", (unsigned) chunk,
             (unsigned) remaining, (unsigned) call_timeout_ms);
    esp_err_t err = this->parent_->write_speaker(current, chunk, call_timeout_ms);

    if (err == ESP_OK) {
      this->last_write_ms_ = millis();
      total_written += chunk;
      current += chunk;
      remaining -= chunk;
      this->chunk_success_streak_++;
      timeout_retry_count = 0;
      ESP_LOGV(TAG_SPK, "Chunk write ok chunk=%u total_written=%u", (unsigned) chunk, (unsigned) total_written);

      const size_t frames_written = chunk / bytes_per_frame;
      if (frames_written > 0) {
        const int64_t timestamp_us = esp_timer_get_time();
        this->audio_output_callback_(static_cast<uint32_t>(frames_written), timestamp_us);
      }

      if (this->preferred_chunk_size_ < chunk_cap) {
        size_t grown = this->preferred_chunk_size_ + CHUNK_GROW_STEP;
        if (grown < MIN_WRITE_CHUNK) {
          grown = MIN_WRITE_CHUNK;
        }
        if (grown > chunk_cap) {
          grown = chunk_cap;
        }
        size_t aligned = align_to_frame(grown);
        if (aligned > 0) {
          grown = aligned;
        }
        this->preferred_chunk_size_ = grown;
      }

      if (this->chunk_success_streak_ >= SUCCESS_STREAK_FOR_GROWTH && this->chunk_upper_bound_ < chunk_cap) {
        size_t new_bound = this->chunk_upper_bound_ + CHUNK_GROW_STEP;
        new_bound = std::min(new_bound, chunk_cap);
        size_t aligned = align_to_frame(new_bound);
        if (aligned > 0) {
          new_bound = aligned;
        }
        if (new_bound > this->chunk_upper_bound_) {
          ESP_LOGD(TAG_SPK, "Increasing chunk upper bound to %u", (unsigned) new_bound);
          this->chunk_upper_bound_ = new_bound;
        }
        this->chunk_success_streak_ = 0;
      }

      continue;
    }

    this->chunk_success_streak_ = 0;

    if (err == ESP_ERR_TIMEOUT) {
      ESP_LOGW(TAG_SPK, "Chunk write timeout chunk=%u", (unsigned) chunk);

      size_t new_upper = (chunk > CHUNK_GROW_STEP) ? (chunk - CHUNK_GROW_STEP) : chunk;
      new_upper = std::max(MIN_WRITE_CHUNK, std::min(new_upper, chunk_cap));
      size_t aligned = align_to_frame(new_upper);
      if (aligned > 0) {
        new_upper = aligned;
      }
      if (new_upper < this->chunk_upper_bound_) {
        ESP_LOGD(TAG_SPK, "Reducing chunk upper bound to %u", (unsigned) new_upper);
        this->chunk_upper_bound_ = new_upper;
      }

      size_t reduced = this->preferred_chunk_size_ / 2;
      if (reduced < MIN_WRITE_CHUNK) {
        reduced = MIN_WRITE_CHUNK;
      }
      reduced = std::min(reduced, this->chunk_upper_bound_);
      aligned = align_to_frame(reduced);
      if (aligned > 0) {
        reduced = aligned;
      }
      this->preferred_chunk_size_ = reduced;

      timeout_retry_count++;
      vTaskDelay(pdMS_TO_TICKS(call_timeout_ms));
      if (timeout_retry_count < MAX_TIMEOUT_RETRIES) {
        continue;
      }
      ESP_LOGW(TAG_SPK, "Max timeout retries reached with chunk=%u", (unsigned) chunk);
    } else {
      ESP_LOGW(TAG_SPK, "Write error: %s", esp_err_to_name(err));
      vTaskDelay(pdMS_TO_TICKS(call_timeout_ms));
    }

    break;
  }

  return total_written;
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
