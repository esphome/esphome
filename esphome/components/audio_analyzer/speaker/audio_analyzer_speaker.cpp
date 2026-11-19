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

static_assert(static_cast<uint8_t>(PassThroughState::PASS_THROUGH_STATE_STOPPED) == speaker::STATE_STOPPED &&
                  static_cast<uint8_t>(PassThroughState::PASS_THROUGH_STATE_STARTING) == speaker::STATE_STARTING &&
                  static_cast<uint8_t>(PassThroughState::PASS_THROUGH_STATE_RUNNING) == speaker::STATE_RUNNING &&
                  static_cast<uint8_t>(PassThroughState::PASS_THROUGH_STATE_STOPPING) == speaker::STATE_STOPPING,
              "PassThroughState mirrors speaker::State");

static inline PassThroughState to_pass_through(speaker::State state) { return static_cast<PassThroughState>(state); }
static inline speaker::State to_speaker(PassThroughState state) { return static_cast<speaker::State>(state); }

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
  // Band plan for the highest analysis rate until a stream arrives, so dump_config() shows it
  this->configure_analysis_(static_cast<float>(MAX_ANALYSIS_RATE_HZ));

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

// start(), stop() and finish() can run on any task. Each wakes loop(), which follows the output's state.
void AudioAnalyzerSpeaker::start() {
  if (starts_new_stream(to_pass_through(this->state_))) {
    // New stream: the output holds nothing, and the samples left from the previous stream are dropped
    this->frames_in_output_.store(0, std::memory_order_release);
    this->new_stream_.store(true, std::memory_order_release);
    this->sync_state_.store(SyncState::SYNC_STATE_RESYNC, std::memory_order_release);
    // May run on a producer task, so the cached loop timestamp is not usable here
    this->state_start_ms_.store(millis(), std::memory_order_release);
    this->state_ = speaker::STATE_STARTING;
  }
  this->sync_stream_info_();
  this->output_speaker_->start();
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::stop() {
  this->state_ = to_speaker(state_after_stop(to_pass_through(this->state_)));
  this->output_speaker_->stop();
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::finish() {
  // Stays running until the output has played its buffered audio
  this->output_speaker_->finish();
  this->enable_loop_soon_any_context();
}

// THREAD CONTEXT: the task that writes audio to this speaker.
size_t AudioAnalyzerSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  if (starts_new_stream(to_pass_through(this->state_))) {
    // Producers such as mixer drive play() without calling start()
    this->start();
  }
  if (this->new_stream_.exchange(false, std::memory_order_acq_rel)) {
    this->frame_assembler_.reset();
  }
  this->sync_stream_info_();
  const size_t bytes_written = this->output_speaker_->play(data, length, ticks_to_wait);
  if (bytes_written > 0) {
    // A write can end mid-frame; the producer continues from there on its next call
    this->frame_assembler_.add(data, bytes_written, this->audio_stream_info_.frames_to_bytes(1),
                               [this](const uint8_t *frames, uint32_t count) { this->forward_frames_(frames, count); });
  }
  return bytes_written;
}

void AudioAnalyzerSpeaker::forward_frames_(const uint8_t *data, uint32_t frames) {
  // Counted before the copy, so loop() never sees samples of frames that are not counted as in the output yet
  this->frames_in_output_.fetch_add(frames, std::memory_order_release);
  if (this->ring_buffer_ != nullptr) {
    this->copy_frames_(data, frames);
  }
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
    chunk[count++] = static_cast<int16_t>(std::clamp(sample * 32768.0f, -32768.0f, 32767.0f));
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
  const PassThroughState current = to_pass_through(this->state_);
  bool start_timed_out = false;
  if (current == PassThroughState::PASS_THROUGH_STATE_STARTING) {
    // Signed: start() may have stamped a time after this loop pass began
    const int32_t elapsed = static_cast<int32_t>(App.get_loop_component_start_time() -
                                                 this->state_start_ms_.load(std::memory_order_acquire));
    start_timed_out = elapsed > static_cast<int32_t>(STATE_TRANSITION_TIMEOUT_MS);
  }
  const PassThroughState next =
      follow_output(current, this->output_speaker_->is_running(), this->output_speaker_->is_stopped(), start_timed_out);
  if (next == current) {
    return;
  }
  if (start_timed_out && next == PassThroughState::PASS_THROUGH_STATE_STOPPING) {
    ESP_LOGW(TAG, "Output speaker did not start");
    this->output_speaker_->stop();
  }
  // start() or stop() on another task may have moved the state since it was read; theirs wins
  if (this->state_ == to_speaker(current)) {
    this->state_ = to_speaker(next);
  }
}

void AudioAnalyzerSpeaker::loop() {
  this->update_state_();

  SyncState sync_state = SyncState::SYNC_STATE_DRAINED;
  if (this->ring_buffer_ != nullptr) {
    bool analyzed = false;
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
      analyzed = this->analyze_heard_();
    }
    this->report_overflow_(this->overflow_.exchange(false, std::memory_order_relaxed), analyzed);
  }

  this->release_if_idle_();

  if (this->state_ == speaker::STATE_STOPPED && sync_state != SyncState::SYNC_STATE_RESYNC && !this->active_ &&
      this->analysis_.is_released()) {
    // start(), stop(), finish() and play() wake it up
    this->disable_loop();
  }
}

void AudioAnalyzerSpeaker::report_overflow_(bool overflowed, bool analyzed) {
  if (overflowed) {
    if (!this->status_has_warning()) {
      const uint32_t sample_rate = this->copy_sample_rate_.load(std::memory_order_relaxed);
      const uint32_t in_output = this->frames_in_output_.load(std::memory_order_relaxed);
      ESP_LOGW(TAG,
               "Analysis buffer full: the output holds about %" PRIu32 " ms of audio, buffer_duration is %" PRIu32
               " ms (or the main loop stalled)",
               sample_rate > 0 ? static_cast<uint32_t>(static_cast<uint64_t>(in_output) * 1000 / sample_rate) : 0,
               this->buffer_duration_ms_);
    }
    this->status_set_warning();
  } else if (analyzed && this->status_has_warning()) {
    // Frames are analyzed again after the restart from the newest audio
    this->status_clear_warning();
  }
}

bool AudioAnalyzerSpeaker::analyze_heard_() {
  // Buffered samples first: the producer counts frames before copying them, so every sample read here is counted
  const uint32_t buffered = this->ring_buffer_->available() / sizeof(int16_t);
  const uint32_t in_output = this->frames_in_output_.load(std::memory_order_acquire);
  uint32_t due = samples_heard(buffered, this->decimation_, in_output);
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
  const bool silent =
      this->output_speaker_->get_mute_state() || this->output_speaker_->get_volume() < speaker::SILENT_VOLUME_THRESHOLD;
  bool analyzed = false;
  while (due > 0) {
    const size_t want = std::min<uint32_t>(due, CHUNK_SAMPLES);
    const size_t got = this->ring_buffer_->read(chunk, want * sizeof(int16_t), 0) / sizeof(int16_t);
    for (size_t i = 0; i < got; ++i) {
      analyzed |= this->push_sample_(silent ? 0.0f : static_cast<float>(chunk[i]) * (1.0f / 32768.0f));
    }
    due -= got;
    if (got < want) {
      break;
    }
  }
  return analyzed;
}

}  // namespace esphome::audio_analyzer

#endif
