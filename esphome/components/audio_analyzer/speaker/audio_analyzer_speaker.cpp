#include "audio_analyzer_speaker.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#include <algorithm>
#include <cmath>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer.speaker");

// Samples copied or analyzed per stack chunk
static constexpr size_t CHUNK_SAMPLES = 128;
// When more is due (the loop was blocked), skip to the newest frame instead of catching up
static constexpr uint32_t MAX_BACKLOG_SAMPLES = 2 * FFT_SIZE;
// Maximum time to wait for the output to report running after start()
static constexpr uint32_t STATE_TRANSITION_TIMEOUT_MS = 5000;

static inline void atomic_subtract_clamped(std::atomic<uint32_t> &var, uint32_t amount) {
  uint32_t current = var.load(std::memory_order_acquire);
  while (current > 0 && !var.compare_exchange_weak(current, current - std::min(amount, current),
                                                   std::memory_order_release, std::memory_order_acquire)) {
  }
}

void AudioAnalyzerSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio Analyzer Speaker:\n"
                "  Buffer duration: %" PRIu32 " ms",
                this->buffer_duration_ms_);
  this->dump_analysis_config_();
}

void AudioAnalyzerSpeaker::setup() {
  // Sized for the highest analysis rate, so any stream format fits
  const size_t size = static_cast<size_t>(this->buffer_duration_ms_) * MAX_ANALYSIS_RATE_HZ / 1000 * sizeof(int16_t);
  this->ring_buffer_ = ring_buffer::RingBuffer::create(size);
  if (this->ring_buffer_ == nullptr) {
    // Keep forwarding audio: a failed analysis must not stop playback
    ESP_LOGE(TAG, "Not enough memory for a %u byte analysis buffer", static_cast<unsigned>(size));
    this->status_set_error(LOG_STR("Not enough memory for the analysis buffer"));
  }

  // THREAD CONTEXT: the output speaker's task. Counts the frames that were played, then forwards the notification
  // unchanged to whatever listens to this speaker (for example a media player tracking playback).
  this->output_speaker_->add_audio_output_callback([this](uint32_t new_frames, int64_t write_timestamp) {
    atomic_subtract_clamped(this->frames_in_output_, new_frames);
    this->audio_output_callback_(new_frames, write_timestamp);
  });
}

void AudioAnalyzerSpeaker::sync_stream_info_() {
  if (this->output_speaker_->get_audio_stream_info() != this->audio_stream_info_) {
    this->output_speaker_->set_audio_stream_info(this->audio_stream_info_);
  }
}

void AudioAnalyzerSpeaker::start() {
  if (this->state_ == speaker::STATE_STOPPED) {
    // New stream: the output holds nothing, and the samples left from the previous stream are dropped
    this->frames_in_output_.store(0, std::memory_order_release);
    this->sync_state_.store(SyncState::SYNC_STATE_RESYNC, std::memory_order_release);
    this->state_ = speaker::STATE_STARTING;
    // May run on a producer task, so the cached loop timestamp is not usable here
    this->state_start_ms_ = millis();
  }
  this->sync_stream_info_();
  this->output_speaker_->start();
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::stop() {
  this->state_ = speaker::STATE_STOPPING;
  this->output_speaker_->stop();
}

void AudioAnalyzerSpeaker::finish() {
  this->state_ = speaker::STATE_STOPPING;
  this->output_speaker_->finish();
}

// THREAD CONTEXT: the task that writes audio to this speaker.
size_t AudioAnalyzerSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (this->state_ == speaker::STATE_STOPPED) {
    // Producers such as mixer drive play() without calling start()
    this->start();
  }
  this->sync_stream_info_();
  const size_t bytes_written = this->output_speaker_->play(data, length, ticks_to_wait);
  if (bytes_written > 0) {
    const uint32_t frames = this->audio_stream_info_.bytes_to_frames(bytes_written);
    // Counted before the copy, so loop() never sees samples of frames that are not counted as in the output yet
    this->frames_in_output_.fetch_add(frames, std::memory_order_release);
    if (this->ring_buffer_ != nullptr) {
      this->copy_frames_(data, frames);
    }
  }
  return bytes_written;
}

void AudioAnalyzerSpeaker::copy_frames_(const uint8_t *data, uint32_t frames) {
  SyncState sync_state = this->sync_state_.load(std::memory_order_acquire);
  if (sync_state == SyncState::SYNC_STATE_DRAINED) {
    this->copy_stream_info_ = this->audio_stream_info_;
    this->decimator_.configure(this->copy_stream_info_.get_sample_rate());
    this->copy_sample_rate_.store(this->copy_stream_info_.get_sample_rate(), std::memory_order_relaxed);
    this->sync_state_.store(SyncState::SYNC_STATE_COPYING, std::memory_order_release);
    sync_state = SyncState::SYNC_STATE_COPYING;
  }
  if (sync_state != SyncState::SYNC_STATE_COPYING) {
    return;  // Waiting for loop() to empty the buffer
  }
  if (this->copy_stream_info_ != this->audio_stream_info_) {
    // Format changed mid-stream: restart the copy with the new format
    this->sync_state_.store(SyncState::SYNC_STATE_RESYNC, std::memory_order_release);
    return;
  }

  const uint8_t bytes_per_sample = this->copy_stream_info_.get_bits_per_sample() / 8;
  const uint8_t channels = this->copy_stream_info_.get_channels();
  const size_t bytes_per_frame = this->copy_stream_info_.frames_to_bytes(1);
  int16_t chunk[CHUNK_SAMPLES];
  size_t count = 0;
  for (uint32_t f = 0; f < frames; ++f) {
    float sample;
    if (!this->decimator_.push(read_mono_sample(data + f * bytes_per_frame, bytes_per_sample, channels), sample)) {
      continue;
    }
    chunk[count++] = static_cast<int16_t>(
        std::clamp<int32_t>(static_cast<int32_t>(lroundf(sample * 32768.0f)), INT16_MIN, INT16_MAX));
    if (count == CHUNK_SAMPLES) {
      if (!this->write_samples_(chunk, count)) {
        return;
      }
      count = 0;
    }
  }
  if (count > 0) {
    this->write_samples_(chunk, count);
  }
}

bool AudioAnalyzerSpeaker::write_samples_(const int16_t *samples, size_t count) {
  const size_t bytes = count * sizeof(int16_t);
  if (this->ring_buffer_->write_without_replacement(samples, bytes, 0, false) == bytes) {
    return true;
  }
  // Full: the output holds more audio than buffer_duration, or loop() was blocked. Start over from the newest audio.
  this->overflow_.store(true, std::memory_order_relaxed);
  this->sync_state_.store(SyncState::SYNC_STATE_RESYNC, std::memory_order_release);
  return false;
}

void AudioAnalyzerSpeaker::update_state_() {
  switch (this->state_) {
    case speaker::STATE_STARTING:
      if (this->output_speaker_->is_running()) {
        this->state_ = speaker::STATE_RUNNING;
      } else if ((App.get_loop_component_start_time() - this->state_start_ms_) > STATE_TRANSITION_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Output speaker did not start");
        this->state_ = speaker::STATE_STOPPED;
      }
      break;
    case speaker::STATE_RUNNING:
    case speaker::STATE_STOPPING:
      if (this->output_speaker_->is_stopped()) {
        this->state_ = speaker::STATE_STOPPED;
      }
      break;
    case speaker::STATE_STOPPED:
      if (this->output_speaker_->is_running()) {
        this->state_ = speaker::STATE_RUNNING;
      }
      break;
  }
}

void AudioAnalyzerSpeaker::loop() {
  this->update_state_();

  SyncState sync_state = SyncState::SYNC_STATE_DRAINED;
  if (this->ring_buffer_ != nullptr) {
    sync_state = this->sync_state_.load(std::memory_order_acquire);
    if (sync_state == SyncState::SYNC_STATE_RESYNC) {
      // The producer doesn't write until it sees SYNC_STATE_DRAINED
      this->ring_buffer_->reset();
      this->analysis_.reset_frame();
      this->awaiting_copy_ = true;
      SyncState expected = SyncState::SYNC_STATE_RESYNC;
      this->sync_state_.compare_exchange_strong(expected, SyncState::SYNC_STATE_DRAINED, std::memory_order_acq_rel);
    } else if (sync_state == SyncState::SYNC_STATE_COPYING) {
      if (this->awaiting_copy_) {
        this->awaiting_copy_ = false;
        const uint32_t sample_rate = this->copy_sample_rate_.load(std::memory_order_relaxed);
        this->decimation_ = decimation_for(sample_rate);
        if (sample_rate != this->analysis_sample_rate_) {
          this->analysis_sample_rate_ = sample_rate;
          this->configure_analysis_(static_cast<float>(sample_rate) / this->decimation_);
        }
      }
      this->analyze_heard_();
    }
    if (this->overflow_.exchange(false, std::memory_order_relaxed) && !this->overflow_logged_) {
      this->overflow_logged_ = true;
      ESP_LOGW(TAG, "Analysis buffer full: buffer_duration is shorter than the output's latency, or the loop stalled");
    }
  }

  this->release_if_idle_();

  if (this->state_ == speaker::STATE_STOPPED && sync_state != SyncState::SYNC_STATE_RESYNC && !this->active_ &&
      this->analysis_.is_released()) {
    // start() and play() wake it up
    this->disable_loop();
  }
}

void AudioAnalyzerSpeaker::analyze_heard_() {
  uint32_t due = samples_heard(this->ring_buffer_->available() / sizeof(int16_t), this->decimation_,
                               this->frames_in_output_.load(std::memory_order_acquire));
  int16_t chunk[CHUNK_SAMPLES];
  if (due > MAX_BACKLOG_SAMPLES) {
    // Being current matters more than analyzing every sample: keep the newest frame's worth
    uint32_t skip = due - FFT_SIZE;
    while (skip > 0) {
      const size_t want = std::min<uint32_t>(skip, CHUNK_SAMPLES);
      const size_t got = this->ring_buffer_->read(chunk, want * sizeof(int16_t), 0) / sizeof(int16_t);
      skip -= got;
      if (got < want) {
        break;
      }
    }
    this->analysis_.reset_frame();
    due = FFT_SIZE;
  }

  // Muted or at zero volume nothing is heard; otherwise the analysis is taken before the volume
  const bool muted = this->is_silent_();
  while (due > 0) {
    const size_t want = std::min<uint32_t>(due, CHUNK_SAMPLES);
    const size_t got = this->ring_buffer_->read(chunk, want * sizeof(int16_t), 0) / sizeof(int16_t);
    for (size_t i = 0; i < got; ++i) {
      this->push_sample_(muted ? 0.0f : static_cast<float>(chunk[i]) * (1.0f / 32768.0f));
    }
    due -= got;
    if (got < want) {
      break;
    }
  }
}

}  // namespace esphome::audio_analyzer

#endif
