#include "usb_audio_speaker.h"

#ifdef USE_USB_AUDIO

#include <algorithm>
#include <cinttypes>

#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "sdkconfig.h"

namespace esphome::usb_audio {

ESPHOME_LOG_TAG(TAG, "usb_audio.spk");

static constexpr size_t MAX_TIMEOUT_RETRIES = 4;
// Zeros are silence for the signed PCM the driver streams; 240 is a whole number of frames for every format.
static constexpr uint8_t SILENCE[240] = {};

void USBAudioSpeaker::setup() {
  const auto &info = this->usb_stream_info_;
  this->write_chunk_ = info.frames_to_bytes(std::max<uint32_t>(info.bytes_to_frames(this->buffer_size_ / 4), 1));
  // The driver sends 1 ms packets, grouped into transfers it keeps in flight.
  this->frame_bytes_ = info.frames_to_bytes(1);
  this->packet_bytes_ = info.frames_to_bytes(info.get_sample_rate() / 1000);
  this->packet_frac_ = info.get_sample_rate() % 1000;
  this->transfer_bytes_ = info.frames_to_bytes((info.get_sample_rate() + 999) / 1000 * CONFIG_UAC_NUM_PACKETS_PER_URB);
  this->transfer_us_ = CONFIG_UAC_NUM_PACKETS_PER_URB * 1000;
  this->in_flight_us_ = CONFIG_UAC_NUM_ISOC_URBS * this->transfer_us_;
  // About 10 ms per report, like the I2S speaker's DMA buffers.
  this->report_frames_ = std::max<uint32_t>(info.get_sample_rate() / 100, 1);
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
                this->usb_stream_info_.get_sample_rate(), this->usb_stream_info_.get_bits_per_sample(),
                this->usb_stream_info_.get_channels(), this->buffer_size_, this->write_timeout_ms_);
}

void USBAudioSpeaker::loop() {
  this->deliver_reports_();
  const uint8_t commands = this->commands_.exchange(0);
  if (commands & COMMAND_STOP) {
    this->stop_();
  }
  if ((commands & COMMAND_START) && this->state_ == speaker::STATE_STOPPED) {
    if (this->writes_in_flight_ != 0) {
      // A write from before the stop may still count its bytes; reset once it has, unless a stop() came since.
      uint8_t pending = this->commands_;
      while (!(pending & COMMAND_STOP)) {
        if (this->commands_.compare_exchange_weak(pending, pending | COMMAND_START)) {
          this->enable_loop_soon_any_context();
          break;
        }
      }
    } else {
      // Suspending flushed the driver's ring buffer, and resuming restarts its packet sizing.
      portENTER_CRITICAL(&this->taken_lock_);
      this->ring_bytes_ = 0;
      this->ring_padding_ = 0;
      // Resuming resets the driver's accumulator, then prepares every transfer, which advances it by their packets.
      this->packet_frac_accum_ = CONFIG_UAC_NUM_ISOC_URBS * CONFIG_UAC_NUM_PACKETS_PER_URB * this->packet_frac_ % 1000;
      this->unreported_frames_ = 0;
      this->report_frames_pending_ = 0;
      this->last_finish_us_ = 0;
      // Idle windows start now, not at the previous stream's last transfer.
      const uint32_t now_us = micros();
      this->last_taken_us_ = now_us;
      this->last_event_us_ = now_us;
      portEXIT_CRITICAL(&this->taken_lock_);
      this->state_ = speaker::STATE_STARTING;
      // Answers through on_stream_state() now or once the device connects.
      this->parent_->resume(Endpoint::SPEAKER);
    }
  }

  if (commands & COMMAND_FINISH) {
    if (this->state_ == speaker::STATE_RUNNING) {
      this->begin_finishing_();
    } else if (this->state_ == speaker::STATE_STARTING) {
      // The stream is still starting; on_stream_state() finishes it once it runs.
      this->finish_after_start_ = true;
    }
  }
  if (this->finishing_ && this->pad_remaining_ > 0) {
    this->pad_with_silence_();
  }
  const bool settling = this->settle_stale_count_();
  // has_buffered_data_() turns false once the driver has taken the last whole transfer and it has had time to play.
  if (this->finishing_ && this->pad_remaining_ == 0 && !this->has_buffered_data_()) {
    this->stop_();
  }
  if (!this->finishing_ && !this->flush_tail_() && !settling) {
    this->disable_loop();
  }
}

// Called with taken_lock_ held.
void USBAudioSpeaker::queue_report_(int64_t finish_us) {
  this->report_frames_pending_ += this->unreported_frames_;
  this->report_finish_us_ = finish_us;
  this->unreported_frames_ = 0;
}

// Called with taken_lock_ held. The accumulator mirrors the driver's packet sizing at fractional sample rates.
uint32_t USBAudioSpeaker::next_transfer_bytes_(uint32_t &accum) const {
  uint32_t size = 0;
  for (int i = 0; i < CONFIG_UAC_NUM_PACKETS_PER_URB; i++) {
    accum += this->packet_frac_;
    size += this->packet_bytes_;
    if (accum >= 1000) {
      size += this->frame_bytes_;
      accum -= 1000;
    }
  }
  return size;
}

// Less than the next transfer of the caller's audio: the driver parks its transfers until the ring holds a whole one.
uint32_t USBAudioSpeaker::audio_tail_bytes_() {
  // No transfer is larger than transfer_bytes_, so a full transfer in the ring rules a tail out without the lock.
  const uint32_t ring = this->ring_bytes_;
  if (this->state_ != speaker::STATE_RUNNING || ring >= this->transfer_bytes_ || ring <= this->ring_padding_) {
    return 0;
  }
  portENTER_CRITICAL(&this->taken_lock_);
  uint32_t accum = this->packet_frac_accum_;
  const uint32_t next = this->next_transfer_bytes_(accum);
  portEXIT_CRITICAL(&this->taken_lock_);
  return ring < next ? next - ring : 0;
}

// A take on the driver's task of a write still being counted looks like a park. Once no write or transfer event has
// come for the in-flight time, the driver holds less than a transfer, so replay takes until that is counted.
// True while waiting for the bus to idle.
bool USBAudioSpeaker::settle_stale_count_() {
  if (this->state_ != speaker::STATE_RUNNING || this->ring_bytes_ == 0) {
    return false;
  }
  auto busy = [this]() { return this->writes_in_flight_ != 0 || this->within_in_flight_(this->last_event_us_); };
  if (busy()) {
    return true;
  }
  portENTER_CRITICAL(&this->taken_lock_);
  // Again under the lock, so a take inside a write that just started has already set the event time.
  if (busy()) {
    portEXIT_CRITICAL(&this->taken_lock_);
    return true;
  }
  const int64_t now_us = esp_timer_get_time();
  while (true) {
    uint32_t accum = this->packet_frac_accum_;
    const uint32_t size = this->next_transfer_bytes_(accum);
    const uint32_t padding_now = this->ring_padding_;
    const uint32_t ring = this->ring_bytes_;
    if (ring < size) {
      break;
    }
    this->take_(ring, padding_now, size, accum, now_us);
  }
  if (this->unreported_frames_ > 0) {
    this->queue_report_(this->last_finish_us_);
  }
  portEXIT_CRITICAL(&this->taken_lock_);
  this->deliver_reports_();
  return false;
}

// Pads an audio tail to a whole transfer once the bus has gone idle, so the driver takes it and the frames get
// reported; a caller that stops mid-transfer (the mixer at the end of a stream) would otherwise wait for them forever.
// Returns true while a tail is still pending, so loop() keeps running.
bool USBAudioSpeaker::flush_tail_() {
  if (this->audio_tail_bytes_() == 0) {
    return false;
  }
  if (this->within_in_flight_(this->last_taken_us_)) {
    return true;
  }
  // Padding must sit at the end of the ring, so no caller write may interleave with it: the flag is raised before the
  // in-flight count is read and play_internal_() reads it after raising the count, so one side always sees the other.
  if (!this->flushing_) {
    this->flushing_ = true;
    if (this->writes_in_flight_ > 0) {
      this->flushing_ = false;
      return true;
    }
    this->pad_remaining_ = this->audio_tail_bytes_();
  }
  // A busy driver can cut the padding short; the hold stays until the rest is written on a later pass.
  this->pad_with_silence_();
  if (this->pad_remaining_ > 0) {
    return true;
  }
  // The ring now holds exactly the next transfer (or nothing, if the driver took it meanwhile): no tail is left.
  this->flushing_ = false;
  return false;
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

void USBAudioSpeaker::begin_finishing_() {
  if (this->finishing_) {
    return;
  }
  this->finishing_ = true;
  // finish() pads the last transfer itself, so a tail flush that was cut short hands over to it.
  this->flushing_ = false;
  this->pad_remaining_ = this->transfer_bytes_;
  // loop() pads the last transfer and stops once the audio has played.
  this->enable_loop();
}

void USBAudioSpeaker::stop_() {
  this->finishing_ = false;
  this->flushing_ = false;
  this->finish_after_start_ = false;
  this->pad_remaining_ = 0;
  if (this->state_ == speaker::STATE_STOPPED) {
    return;
  }
  this->parent_->suspend(Endpoint::SPEAKER);
  this->state_ = speaker::STATE_STOPPED;
}

void USBAudioSpeaker::on_stream_state(StreamEvent event) {
  if (event == StreamEvent::RUNNING) {
    if (this->state_ == speaker::STATE_STARTING) {
      this->state_ = speaker::STATE_RUNNING;
      if (this->finish_after_start_) {
        this->finish_after_start_ = false;
        this->begin_finishing_();
      }
    }
    return;
  }
  // Playback stops on a lost device as well as a failed start; the next start() opens it again.
  if (event == StreamEvent::LOST && this->state_ == speaker::STATE_RUNNING) {
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
  if (this->audio_stream_info_ != this->usb_stream_info_) {
    // Playing it through the configured stream would change the speed and channel layout.
    if (!this->format_mismatch_.exchange(true)) {
      const auto &info = this->audio_stream_info_;
      ESP_LOGE(TAG,
               "Audio format (%" PRIu32 " Hz, %u bits, %u channels) does not match the USB stream; use a resampler "
               "speaker",
               info.get_sample_rate(), info.get_bits_per_sample(), info.get_channels());
    }
    if (budget_ms > 0) {
      vTaskDelay(pdMS_TO_TICKS(budget_ms));
    }
    return 0;
  }
  this->format_mismatch_ = false;

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
    const size_t chunk =
        this->flushing_ ? 0 : std::min<size_t>({length - written, this->write_chunk_, this->ring_room_()});
    if (chunk == 0) {
      // Back-pressure: wait for the driver to take a transfer, or for loop() to finish padding a tail.
      if (budget_ms == 0) {
        break;
      }
      vTaskDelay(1);
      now = millis();
      continue;
    }
    const uint32_t timeout_ms = budget_ms > 0 ? budget_ms - elapsed : 0;
    const esp_err_t err = this->write_(data + written, chunk, timeout_ms, false);
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
  // For settle_stale_count_() and the tail flush.
  if (this->ring_bytes_ != 0) {
    this->enable_loop_soon_any_context();
  }
  return written;
}

size_t USBAudioSpeaker::ring_room_() const {
  // Whole frames up to buffer_size_. ring_bytes_ is never below the real fill, so a write of this much does not wait.
  const uint32_t ring = this->ring_bytes_;
  const uint32_t room = ring < this->buffer_size_ ? this->buffer_size_ - ring : 0;
  return room - room % this->frame_bytes_;
}

esp_err_t USBAudioSpeaker::write_(const uint8_t *data, size_t length, uint32_t timeout_ms, bool padding) {
  // Counted once stored, so the count never falls below the real fill: by a take inside the write, else on return.
  TaskHandle_t none = nullptr;
  const bool tracked = this->writer_task_.compare_exchange_strong(none, xTaskGetCurrentTaskHandle());
  if (tracked) {
    this->pending_bytes_ = length;
    this->pending_padding_ = padding;
  }
  const esp_err_t err = this->parent_->write_speaker(data, length, timeout_ms);
  if (tracked) {
    // 0 if a take inside the write counted them.
    length = this->pending_bytes_.exchange(0);
    this->writer_task_ = nullptr;
  }
  // A failed write stored nothing, since the driver only writes whole chunks.
  if (err == ESP_OK) {
    this->count_(length, padding);
  }
  return err;
}

void USBAudioSpeaker::count_(uint32_t length, bool padding) {
  this->ring_bytes_.fetch_add(length);
  if (padding) {
    this->ring_padding_.fetch_add(length);
  }
}

void USBAudioSpeaker::pad_with_silence_() {
  if (this->state_ != speaker::STATE_RUNNING) {
    this->pad_remaining_ = 0;
    return;
  }
  // Counted before the hub's handle is read, so the hub never closes the device under this call.
  this->writes_in_flight_.fetch_add(1);
  const size_t piece = sizeof(SILENCE) - sizeof(SILENCE) % this->usb_stream_info_.frames_to_bytes(1);
  while (this->pad_remaining_ > 0) {
    const size_t chunk = std::min<size_t>({this->pad_remaining_, piece, this->ring_room_()});
    if (chunk == 0 || this->write_(SILENCE, chunk, 0, true) != ESP_OK) {
      // A full buffer; the rest is written on a later loop().
      break;
    }
    this->pad_remaining_ -= chunk;
  }
  this->writes_in_flight_.fetch_sub(1);
}

void USBAudioSpeaker::on_transfer_taken() {
  bool report = false;
  const int64_t now_us = esp_timer_get_time();
  this->last_event_us_ = static_cast<uint32_t>(now_us);
  portENTER_CRITICAL(&this->taken_lock_);
  // Only a take on the writer's task comes after the driver stored the write.
  const TaskHandle_t writer = this->writer_task_;
  if (writer != nullptr && writer == xTaskGetCurrentTaskHandle()) {
    this->count_(this->pending_bytes_.exchange(0), this->pending_padding_);
  }
  // Mirror the driver: the next transfer is CONFIG_UAC_NUM_PACKETS_PER_URB packets, and the driver takes it only when
  // the ring buffer holds all of it; otherwise it parks the transfer until the next write.
  uint32_t accum = this->packet_frac_accum_;
  const uint32_t size = this->next_transfer_bytes_(accum);
  // Padding first, since count_() adds the ring first.
  const uint32_t padding_now = this->ring_padding_;
  const uint32_t ring = this->ring_bytes_;
  // A tail left behind (less than the next transfer of the caller's audio) is flushed from loop() once the bus idles.
  bool tail = ring < size && ring > padding_now;
  if (ring >= size) {
    const uint32_t left = ring - size;
    uint32_t next_accum = accum;
    tail = left < this->transfer_bytes_ && left > padding_now && left < this->next_transfer_bytes_(next_accum);
    report = this->take_(ring, padding_now, size, accum, now_us);
  }
  portEXIT_CRITICAL(&this->taken_lock_);
  if (report || tail) {
    // The listeners can block, so loop() calls them; the frames and their finish time travel together, so the delay
    // does not change what they compute.
    this->enable_loop_soon_any_context();
  }
}

// Called with taken_lock_ held. Returns true when frames were queued for loop() to report.
bool USBAudioSpeaker::take_(uint32_t ring, uint32_t padding_now, uint32_t size, uint32_t accum, int64_t now_us) {
  this->packet_frac_accum_ = accum;
  this->ring_bytes_.fetch_sub(size);
  // Padding sits at the end of the ring and is not the caller's audio, so it is not reported.
  const uint32_t padding = std::min(padding_now, ring);
  const uint32_t audio = std::min(size, ring - padding);
  if (audio < size) {
    this->ring_padding_.fetch_sub(std::min(size - audio, padding));
  }
  this->last_taken_us_ = static_cast<uint32_t>(now_us);
  // While streaming, a transfer finishes one transfer after the previous one, however late this runs, but no later
  // than the transfers in flight allow; on an idle bus it plays right away.
  const int64_t finish_us = this->last_finish_us_ < now_us
                                ? now_us + this->transfer_us_
                                : std::min(this->last_finish_us_ + this->transfer_us_, now_us + this->in_flight_us_);
  this->last_finish_us_ = finish_us;
  this->unreported_frames_ += this->usb_stream_info_.bytes_to_frames(audio);
  // Batched like the I2S speaker's DMA buffers: about every 10 ms, and once the caller's audio runs out.
  if (this->unreported_frames_ > 0 && (this->unreported_frames_ >= this->report_frames_ || audio == ring - padding)) {
    this->queue_report_(finish_us);
    return true;
  }
  return false;
}

void USBAudioSpeaker::deliver_reports_() {
  portENTER_CRITICAL(&this->taken_lock_);
  const uint32_t frames = this->report_frames_pending_;
  const int64_t finish_us = this->report_finish_us_;
  this->report_frames_pending_ = 0;
  portEXIT_CRITICAL(&this->taken_lock_);
  if (frames > 0) {
    this->audio_output_callback_(frames, finish_us);
  }
}

// since_us is read before the clock, so an event landing in between cannot make the difference negative.
bool USBAudioSpeaker::within_in_flight_(uint32_t since_us) const { return micros() - since_us < this->in_flight_us_; }

bool USBAudioSpeaker::has_buffered_data_() const {
  // The driver still holds a whole transfer to take, or one it took has not finished playing. Less than a transfer
  // stays in the ring buffer until the next write.
  return this->state_ == speaker::STATE_RUNNING &&
         (this->ring_bytes_ >= this->transfer_bytes_ || this->within_in_flight_(this->last_taken_us_));
}

bool USBAudioSpeaker::has_buffered_data() const { return this->has_buffered_data_(); }

// The hub's worker sends volume and mute to the device.
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
