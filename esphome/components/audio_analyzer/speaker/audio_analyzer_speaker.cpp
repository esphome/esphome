#include "audio_analyzer_speaker.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer.speaker");

static constexpr uint8_t COMMAND_START = 1 << 0;
static constexpr uint8_t COMMAND_STOP = 1 << 1;
// A start ends without the output ever leaving STOPPED after this long (format refused, start ignored)
static constexpr uint32_t OUTPUT_START_TIMEOUT_MS = 1000;
// Running this long with audio in the output and no played-audio report means the output never reports it
static constexpr uint32_t OUTPUT_REPORT_TIMEOUT_MS = 2000;

static inline void atomic_subtract_clamped(std::atomic<uint32_t> &var, uint32_t amount) {
  uint32_t current = var.load(std::memory_order_acquire);
  while (current > 0 && !var.compare_exchange_weak(current, current - std::min(amount, current),
                                                   std::memory_order_release, std::memory_order_acquire)) {
  }
}

void AudioAnalyzerSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio Analyzer Speaker:\n"
                "  Sample rate: %" PRIu32 " Hz",
                this->sample_rate_);
  this->dump_analysis_config_();
}

void AudioAnalyzerSpeaker::setup() {
  if (!this->tap_.allocate(this->buffer_samples_)) {
    // Keep forwarding audio: a failed analysis must not stop playback
    this->status_set_error(LOG_STR("Not enough memory for the analysis buffer"));
  }

  // THREAD CONTEXT: the output speaker's task. Counts the frames that were played, then forwards the notification
  // unchanged to whatever listens to this speaker (for example a media player tracking playback).
  this->output_speaker_->add_audio_output_callback([this](uint32_t new_frames, int64_t write_timestamp) {
    this->output_reported_.store(true, std::memory_order_relaxed);
    atomic_subtract_clamped(this->frames_in_output_, new_frames);
    this->audio_output_callback_(new_frames, write_timestamp);
  });
}

bool AudioAnalyzerSpeaker::claim_new_stream_() {
  uint8_t commands = this->commands_.load(std::memory_order_acquire);
  do {
    if (commands & COMMAND_START) {
      return false;  // claimed already, loop() hasn't taken it yet
    }
    // A pending stop ends the current stream even though loop() hasn't moved the state yet. Reading state_ from
    // another task is what every producer does through is_running() / is_stopped(); only loop() writes it.
    const bool stopped = this->state_ == speaker::STATE_STOPPED || this->state_ == speaker::STATE_STOPPING;
    if (!(commands & COMMAND_STOP) && !stopped) {
      return false;  // continues the running stream
    }
  } while (!this->commands_.compare_exchange_weak(commands, commands | COMMAND_START, std::memory_order_acq_rel,
                                                  std::memory_order_acquire));
  return true;
}

// start() and stop() can run on any task. They forward to the output at once and post a command for loop().
void AudioAnalyzerSpeaker::start() {
  if (this->claim_new_stream_()) {
    // New stream: the output holds nothing, and the samples left from the previous stream are dropped
    this->frames_in_output_.store(0, std::memory_order_release);
    this->new_stream_.store(true, std::memory_order_release);
    this->tap_.restart();
  }
  if (this->output_speaker_->get_audio_stream_info() != this->audio_stream_info_) {
    this->output_speaker_->set_audio_stream_info(this->audio_stream_info_);
  }
  this->output_speaker_->start();
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::stop() {
  // Replaces a start loop() hasn't taken yet
  this->commands_.store(COMMAND_STOP, std::memory_order_release);
  this->output_speaker_->stop();
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::finish() {
  // Stays running until the output has played its buffered audio
  this->output_speaker_->finish();
  this->enable_loop_soon_any_context();
}

// THREAD CONTEXT: the task that writes audio to this speaker.
void AudioAnalyzerSpeaker::prepare_play_() {
  if (this->state_ == speaker::STATE_STOPPED || this->state_ == speaker::STATE_STOPPING) {
    // Producers such as mixer drive play() without calling start()
    this->start();
  } else if (this->output_speaker_->get_audio_stream_info() != this->audio_stream_info_) {
    this->output_speaker_->set_audio_stream_info(this->audio_stream_info_);
  }
  if (this->new_stream_.exchange(false, std::memory_order_acq_rel)) {
    this->frame_assembler_.reset();
  }
}

#ifdef USE_ESP32
size_t AudioAnalyzerSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  this->prepare_play_();
  const size_t bytes_written = this->output_speaker_->play(data, length, ticks_to_wait);
  this->forward_accepted_(data, bytes_written);
  return bytes_written;
}
#endif

size_t AudioAnalyzerSpeaker::play(const uint8_t *data, size_t length) {
  this->prepare_play_();
  const size_t bytes_written = this->output_speaker_->play(data, length);
  this->forward_accepted_(data, bytes_written);
  return bytes_written;
}

void AudioAnalyzerSpeaker::forward_accepted_(const uint8_t *data, size_t bytes_written) {
  const audio::AudioStreamInfo &info = this->audio_stream_info_;
  const bool copy = this->tap_.is_allocated() && this->analysis_enabled_.load(std::memory_order_relaxed);
  if (copy && info.get_sample_rate() != this->sample_rate_) {
    // The decimation and bands are worked out for sample_rate_; validation keeps in-tree producers to it
    this->unexpected_rate_.store(info.get_sample_rate(), std::memory_order_relaxed);
  }
  const bool analyze = copy && info.get_sample_rate() == this->sample_rate_;
  // A write can end mid-frame; the producer continues from there on its next call
  this->frame_assembler_.add(data, bytes_written, info.frames_to_bytes(1),
                             [this, &info, analyze](const uint8_t *frames, uint32_t count) {
                               // Counted before the copy, so loop() never sees samples of frames not counted yet
                               this->frames_in_output_.fetch_add(count, std::memory_order_release);
                               if (analyze) {
                                 this->tap_.write(frames, count, info.samples_to_bytes(1), info.get_channels());
                               }
                             });
}

void AudioAnalyzerSpeaker::start_analysis() {
  if (!this->analysis_enabled_.exchange(true, std::memory_order_relaxed)) {
    this->tap_.restart();
  }
}

void AudioAnalyzerSpeaker::stop_analysis() {
  this->analysis_enabled_.store(false, std::memory_order_relaxed);
  this->enable_loop_soon_any_context();
}

void AudioAnalyzerSpeaker::enter_state_(speaker::State state, uint32_t now) {
  this->state_ = state;
  this->state_since_ms_ = now;
  this->output_seen_starting_ = false;
  if (state == speaker::STATE_STARTING) {
    this->warned_unexpected_rate_ = false;
  }
  if (state == speaker::STATE_STOPPED) {
    // An overflow near the end of a stream must not stay raised while idle
    this->status_clear_warning();
  }
}

void AudioAnalyzerSpeaker::apply_commands_(uint32_t now) {
  const uint8_t commands = this->commands_.load(std::memory_order_acquire);
  if (commands == 0) {
    return;
  }
  if ((commands & COMMAND_STOP) && this->state_ != speaker::STATE_STOPPED) {
    this->enter_state_(speaker::STATE_STOPPING, now);
  }
  if ((commands & COMMAND_START) &&
      (this->state_ == speaker::STATE_STOPPED || this->state_ == speaker::STATE_STOPPING)) {
    this->enter_state_(speaker::STATE_STARTING, now);
  }
  // After the state: a producer that sees the start taken also sees STARTING. Commands posted meanwhile stay.
  this->commands_.fetch_and(static_cast<uint8_t>(~commands), std::memory_order_release);
}

void AudioAnalyzerSpeaker::follow_output_(uint32_t now) {
  switch (this->state_) {
    case speaker::STATE_STARTING:
      if (this->output_speaker_->is_running()) {
        this->enter_state_(speaker::STATE_RUNNING, now);
      } else if (!this->output_speaker_->is_stopped()) {
        this->output_seen_starting_ = true;
      } else if (this->output_seen_starting_) {
        // Started and already stopped again: a short stream, or the output gave up starting
        this->enter_state_(speaker::STATE_STOPPED, now);
      } else if (now - this->state_since_ms_ > OUTPUT_START_TIMEOUT_MS) {
        ESP_LOGD(TAG, "Output speaker did not start");
        this->enter_state_(speaker::STATE_STOPPED, now);
      }
      break;
    case speaker::STATE_RUNNING:
    case speaker::STATE_STOPPING:
      if (this->output_speaker_->is_stopped()) {
        this->enter_state_(speaker::STATE_STOPPED, now);
      }
      break;
    case speaker::STATE_STOPPED:
      break;
  }
}

void AudioAnalyzerSpeaker::check_output_reports_(uint32_t now) {
  if (this->state_ != speaker::STATE_RUNNING || this->warned_no_reports_ ||
      this->output_reported_.load(std::memory_order_relaxed)) {
    return;
  }
  if (now - this->state_since_ms_ > OUTPUT_REPORT_TIMEOUT_MS &&
      this->frames_in_output_.load(std::memory_order_relaxed) > 0) {
    this->warned_no_reports_ = true;
    ESP_LOGW(TAG, "The output speaker doesn't report the audio it played; nothing can be analyzed");
  }
}

void AudioAnalyzerSpeaker::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->apply_commands_(now);
  this->follow_output_(now);

  if (this->tap_.is_allocated()) {
    // Buffered samples first: the producer counts frames before copying them, so every sample read here is counted
    const uint32_t buffered = this->buffered_samples_();
    const uint32_t in_output = this->frames_in_output_.load(std::memory_order_acquire);
    this->analyze_(samples_heard(buffered, this->tap_.get_decimation(), in_output), this->output_silent_());
    if (this->update_overflow_warning_(now)) {
      ESP_LOGW(TAG,
               "Analysis buffer full: the output holds about %" PRIu32 " ms of audio, the buffer %" PRIu32
               " ms (or the main loop stalled)",
               static_cast<uint32_t>(static_cast<uint64_t>(in_output) * 1000 / this->sample_rate_), this->buffer_ms_());
    }
    const uint32_t rate = this->unexpected_rate_.exchange(0, std::memory_order_relaxed);
    if (rate != 0 && !this->warned_unexpected_rate_) {
      // Once per stream
      this->warned_unexpected_rate_ = true;
      ESP_LOGW(TAG, "Not analyzing a %" PRIu32 " Hz stream: sample_rate is %" PRIu32 " Hz", rate, this->sample_rate_);
    }
  }
  this->check_output_reports_(now);
  this->release_if_idle_(now);

  if (this->state_ == speaker::STATE_STOPPED && this->commands_.load(std::memory_order_relaxed) == 0 &&
      this->is_idle_()) {
    // start(), stop(), finish(), play() and stop_analysis() wake it up
    this->disable_loop();
  }
}

}  // namespace esphome::audio_analyzer
