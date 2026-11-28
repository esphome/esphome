#include "microphone_audio_analyzer.h"

#ifdef USE_MICROPHONE

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

void MicrophoneAudioAnalyzer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Audio Analyzer:\n"
                "  Passive: %s",
                YESNO(this->microphone_source_->is_passive()));
  this->dump_analysis_config_();
}

void MicrophoneAudioAnalyzer::setup() {
  if (!this->tap_.allocate(this->buffer_samples_)) {
    this->mark_failed(LOG_STR("Not enough memory for the analysis buffer"));
    return;
  }
  this->microphone_source_->add_data_callback(
      [this](const microphone::AudioCapture &capture) { this->on_capture_(capture); });

  if (!this->microphone_source_->is_passive()) {
    this->microphone_source_->start();
  }
}

void MicrophoneAudioAnalyzer::on_capture_(const microphone::AudioCapture &capture) {
  if (capture.stream_info != this->stream_info_) {
    // The bands are worked out for the microphone's configured rate; validation only allows a fixed-rate microphone
    this->unexpected_format_.store(true, std::memory_order_relaxed);
  } else {
    this->tap_.write(capture.data.data(), capture.data.size() / sizeof(int16_t), sizeof(int16_t), 1);
  }
  this->enable_loop_soon_any_context();
}

void MicrophoneAudioAnalyzer::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  if (!this->microphone_source_->is_running()) {
    // Passive, or stopped with stop_analysis(): not an error, the values release to silence
    this->running_ = false;
    this->status_clear_warning();
    this->release_if_idle_(now);
    if (this->is_idle_()) {
      this->disable_loop();
    }
    return;
  }

  if (!this->running_) {
    // A new run: drop the audio left from the previous one
    this->running_ = true;
    this->warned_unexpected_format_ = false;
    this->tap_.restart();
  }
  this->analyze_(this->buffered_samples_(), false);
  if (this->update_overflow_warning_(now)) {
    ESP_LOGW(TAG, "The main loop fell behind the microphone by more than %" PRIu32 " ms; the analysis skipped ahead",
             this->buffer_ms_());
  }
  if (this->unexpected_format_.exchange(false, std::memory_order_relaxed) && !this->warned_unexpected_format_) {
    this->warned_unexpected_format_ = true;
    ESP_LOGW(TAG, "Not analyzing microphone audio that isn't 16-bit mono at %" PRIu32 " Hz",
             this->stream_info_.get_sample_rate());
  }
  this->release_if_idle_(now);
}

bool MicrophoneAudioAnalyzer::allows_control_() const {
  const bool passive = this->microphone_source_->is_passive();
  if (passive) {
    ESP_LOGW(TAG, "Can't start or stop the microphone in passive mode");
  }
  return !passive;
}

void MicrophoneAudioAnalyzer::start_analysis() {
  if (this->allows_control_()) {
    this->microphone_source_->start();
  }
}

void MicrophoneAudioAnalyzer::stop_analysis() {
  if (this->allows_control_()) {
    this->microphone_source_->stop();
  }
}

}  // namespace esphome::audio_analyzer

#endif  // USE_MICROPHONE
