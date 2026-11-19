#include "audio_analyzer_speaker.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <algorithm>

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer.speaker");

static constexpr uint32_t MAX_FILL_DURATION_MS = 30;
static constexpr uint8_t MAX_FILLS_PER_LOOP = 2;

void AudioAnalyzerSpeaker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio Analyzer Speaker:\n"
                "  Buffer duration: %" PRIu32 " ms",
                this->buffer_duration_ms_);
  this->dump_analysis_config_();
}

void AudioAnalyzerSpeaker::setup() {
  if (!this->allocate_fft_()) {
    // Keep forwarding audio: a failed analysis must not stop playback
    this->status_set_error(LOG_STR("Failed to allocate FFT buffers"));
  }

  // THREAD CONTEXT: the output speaker's task. Counts the frames that were played, then forwards the notification
  // unchanged to whatever listens to this speaker (for example a media player tracking playback).
  this->output_speaker_->add_audio_output_callback([this](uint32_t new_frames, int64_t write_timestamp) {
    this->played_frames_.fetch_add(new_frames, std::memory_order_relaxed);
    this->audio_output_callback_(new_frames, write_timestamp);
  });
}

void AudioAnalyzerSpeaker::sync_stream_info_() {
  if (this->output_speaker_->get_audio_stream_info() != this->audio_stream_info_) {
    this->output_speaker_->set_audio_stream_info(this->audio_stream_info_);
  }
}

void AudioAnalyzerSpeaker::start() {
  this->sync_stream_info_();
  this->output_speaker_->start();
}

// THREAD CONTEXT: the task that writes audio to this speaker.
size_t AudioAnalyzerSpeaker::play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
  this->sync_stream_info_();
  const size_t bytes_written = this->output_speaker_->play(data, length, ticks_to_wait);

  if (bytes_written > 0) {
    // Copy only what the output accepted, without blocking. Overwrites the oldest data if loop() fell behind.
    std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
    if (temp_ring_buffer != nullptr) {
      temp_ring_buffer->write(data, bytes_written);
    }
  }
  return bytes_written;
}

void AudioAnalyzerSpeaker::loop() {
  // Mirror the output speaker's state, which callers read through ``is_stopped()`` and ``is_running()``.
  if (this->output_speaker_->is_running()) {
    this->state_ = speaker::STATE_RUNNING;
  } else if (this->output_speaker_->is_stopped()) {
    this->state_ = speaker::STATE_STOPPED;
  } else if (this->state_ == speaker::STATE_STOPPED || this->state_ == speaker::STATE_STARTING) {
    this->state_ = speaker::STATE_STARTING;
  } else {
    this->state_ = speaker::STATE_STOPPING;
  }

  if (this->fft_re_ == nullptr) {
    return;  // FFT allocation failed: pass-through only
  }

  if (this->state_ == speaker::STATE_STOPPED) {
    if (this->audio_source_ != nullptr) {
      this->stop_();
    }
    this->decay_if_idle_();
    return;
  }

  if (!this->start_()) {
    this->decay_if_idle_();
    return;
  }
  this->configure_stream_(this->allocated_stream_info_);

  // Audio played before the ring buffer existed was never copied: drop that credit so the analysis does not run
  // ahead of the sound.
  std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = this->ring_buffer_.lock();
  const uint32_t buffered_frames = this->allocated_stream_info_.bytes_to_frames(
      this->audio_source_->available() + (temp_ring_buffer != nullptr ? temp_ring_buffer->available() : 0));
  uint32_t credit = this->played_frames_.load(std::memory_order_relaxed);
  if (credit > buffered_frames) {
    this->played_frames_.fetch_sub(credit - buffered_frames, std::memory_order_relaxed);
    credit = buffered_frames;
  }

  for (uint8_t i = 0; (i < MAX_FILLS_PER_LOOP) && (credit > 0); ++i) {
    this->audio_source_->fill(0);
    const uint32_t frames =
        std::min(this->allocated_stream_info_.bytes_to_frames(this->audio_source_->available()), credit);
    if (frames == 0) {
      break;
    }
    this->process_frames_(this->audio_source_->data(), frames);
    this->audio_source_->consume(this->allocated_stream_info_.frames_to_bytes(frames));
    this->played_frames_.fetch_sub(frames, std::memory_order_relaxed);
    credit -= frames;
  }

  this->decay_if_idle_();
}

bool AudioAnalyzerSpeaker::start_() {
  const audio::AudioStreamInfo stream_info = this->audio_stream_info_;
  if ((this->audio_source_ != nullptr) && (stream_info == this->allocated_stream_info_)) {
    return true;
  }
  this->stop_();

  const size_t bytes_per_frame = stream_info.frames_to_bytes(1);
  if ((bytes_per_frame == 0) || (bytes_per_frame > audio::RingBufferAudioSource::MAX_ALIGNMENT_BYTES)) {
    return false;  // Frame too large to analyze; audio still passes through
  }

  const size_t ring_buffer_size =
      (stream_info.ms_to_bytes(this->buffer_duration_ms_) / bytes_per_frame) * bytes_per_frame;
  std::shared_ptr<ring_buffer::RingBuffer> temp_ring_buffer = ring_buffer::RingBuffer::create(ring_buffer_size);
  if (temp_ring_buffer == nullptr) {
    this->status_momentary_warning(15000);
    return false;
  }
  this->audio_source_ = audio::RingBufferAudioSource::create(
      temp_ring_buffer, stream_info.ms_to_bytes(MAX_FILL_DURATION_MS), static_cast<uint8_t>(bytes_per_frame));
  if (this->audio_source_ == nullptr) {
    this->status_momentary_warning(15000);
    return false;
  }
  this->allocated_stream_info_ = stream_info;
  this->played_frames_.store(0, std::memory_order_relaxed);
  this->reset_frame_();
  this->ring_buffer_ = temp_ring_buffer;
  return true;
}

void AudioAnalyzerSpeaker::stop_() {
  this->audio_source_.reset();  // The ring buffer is freed once play() releases it too
  this->played_frames_.store(0, std::memory_order_relaxed);
  this->reset_frame_();
}

}  // namespace esphome::audio_analyzer

#endif
