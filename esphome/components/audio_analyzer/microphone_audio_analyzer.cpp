#include "microphone_audio_analyzer.h"

#if defined(USE_ESP32) && defined(USE_MICROPHONE)

#include "esphome/core/log.h"

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

// Same as sound_level
static constexpr uint32_t RING_BUFFER_DURATION_MS = 120;
static constexpr uint32_t MAX_FILL_DURATION_MS = 30;
// Enough fills to drain a full ring buffer, even when the wrap splits one
static constexpr uint8_t MAX_FILLS_PER_LOOP = RING_BUFFER_DURATION_MS / MAX_FILL_DURATION_MS + 2;

void MicrophoneAudioAnalyzer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio Analyzer:\n"
                "  Passive: %s",
                YESNO(this->microphone_source_->is_passive()));
  this->dump_analysis_config_();
}

void MicrophoneAudioAnalyzer::setup() {
  // The schema limits the source to 16-bit mono; only the sample rate comes from the microphone
  const audio::AudioStreamInfo stream_info = this->microphone_source_->get_audio_stream_info();
  const size_t bytes_per_frame = stream_info.frames_to_bytes(1);
  const size_t ring_buffer_size =
      (stream_info.ms_to_bytes(RING_BUFFER_DURATION_MS) / bytes_per_frame) * bytes_per_frame;
  this->ring_buffer_ = ring_buffer::RingBuffer::create(ring_buffer_size);
  if (this->ring_buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to allocate the ring buffer"));
    return;
  }
  this->audio_source_ = audio::RingBufferAudioSource::create(
      this->ring_buffer_, stream_info.ms_to_bytes(MAX_FILL_DURATION_MS), static_cast<uint8_t>(bytes_per_frame));
  if (this->audio_source_ == nullptr) {
    this->mark_failed(LOG_STR("Failed to allocate the audio source"));
    return;
  }

  this->decimator_.configure(stream_info.get_sample_rate());
  this->configure_analysis_(this->decimator_.get_output_rate());

  // THREAD CONTEXT: the microphone task. The ring buffer outlives the callback; write() overwrites the oldest audio.
  ring_buffer::RingBuffer *ring_buffer = this->ring_buffer_.get();
  this->microphone_source_->add_data_callback([this, ring_buffer](const microphone::AudioCapture &capture) {
    if (ring_buffer->free() < capture.data.size()) {
      this->overrun_.store(true, std::memory_order_relaxed);
    }
    ring_buffer->write((void *) capture.data.data(), capture.data.size());
  });

  if (!this->microphone_source_->is_passive()) {
    // Automatically start the microphone if not in passive mode
    this->microphone_source_->start();
  }
}

void MicrophoneAudioAnalyzer::loop() {
  // Passive, or stopped with stop(): not an error, the values release to silence
  if (!this->microphone_source_->is_running()) {
    this->running_ = false;
    this->release_if_idle_();
    return;
  }

  const audio::AudioStreamInfo stream_info = this->microphone_source_->get_audio_stream_info();
  if (!this->running_) {
    // Drop what is left from the previous run and the partial frame
    this->running_ = true;
    this->audio_source_->clear_buffered_data();
    this->overrun_.store(false, std::memory_order_relaxed);
    this->analysis_.reset_frame();
    this->overrun_logged_ = false;
    if (this->decimator_.get_input_rate() != stream_info.get_sample_rate()) {
      this->decimator_.configure(stream_info.get_sample_rate());
      this->configure_analysis_(this->decimator_.get_output_rate());
    }
  }

  // Expose chunks of the ring buffer's internal storage until it is drained - don't block the main loop
  const uint8_t bytes_per_sample = stream_info.get_bits_per_sample() / 8;
  const size_t bytes_per_frame = stream_info.frames_to_bytes(1);
  for (uint8_t i = 0; i < MAX_FILLS_PER_LOOP; ++i) {
    this->audio_source_->fill(0);
    const uint32_t frames = stream_info.bytes_to_frames(this->audio_source_->available());
    if (frames == 0) {
      break;
    }
    const uint8_t *data = this->audio_source_->data();
    for (uint32_t f = 0; f < frames; ++f) {
      float sample;
      if (this->decimator_.push(read_mono_sample(data + f * bytes_per_frame, bytes_per_sample, 1), sample)) {
        this->push_sample_(sample);
      }
    }
    this->audio_source_->consume(frames * bytes_per_frame);
  }

  if (this->overrun_.exchange(false, std::memory_order_relaxed) && !this->overrun_logged_) {
    this->overrun_logged_ = true;
    ESP_LOGW(TAG, "The main loop fell behind the microphone; the oldest audio was dropped");
  }
  this->release_if_idle_();
}

void MicrophoneAudioAnalyzer::start() {
  if (this->microphone_source_->is_passive()) {
    ESP_LOGW(TAG, "Can't start the microphone in passive mode");
    return;
  }
  this->microphone_source_->start();
}

void MicrophoneAudioAnalyzer::stop() {
  if (this->microphone_source_->is_passive()) {
    ESP_LOGW(TAG, "Can't stop the microphone in passive mode");
    return;
  }
  this->microphone_source_->stop();
}

}  // namespace esphome::audio_analyzer

#endif
