#include "microphone_audio_analyzer.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

void MicrophoneAnalyzerBase::run_(bool microphone_running) {
  const uint32_t now = App.get_loop_component_start_time();
  if (!microphone_running) {
    // Passive, or stopped with stop_analysis(): not an error, the values release to silence
    this->running_ = false;
    this->status_clear_warning();
    this->release_if_idle_(now);
    if (this->is_idle_()) {
      // The data callback wakes it up
      this->disable_loop();
    }
    return;
  }

  if (!this->running_) {
    // A new run: drop the audio left from the previous one
    this->running_ = true;
    this->tap_.restart();
  }
  this->analyze_(this->buffered_samples_(), false);
  if (this->update_overflow_warning_(now)) {
    ESP_LOGW(TAG, "The main loop fell behind the microphone by more than %" PRIu32 " ms; the analysis skipped ahead",
             this->buffer_ms_());
  }
  this->release_if_idle_(now);
}

bool MicrophoneAnalyzerBase::allows_control_(bool passive) const {
  if (passive) {
    ESP_LOGW(TAG, "Can't start or stop the microphone in passive mode");
  }
  return !passive;
}

#ifdef USE_MICROPHONE
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
  // THREAD CONTEXT: the microphone task
  this->microphone_source_->add_data_callback(
      [this](const microphone::AudioCapture &capture) { this->on_audio_(capture.data.data(), capture.data.size()); });

  if (!this->microphone_source_->is_passive()) {
    // Automatically start the microphone if not in passive mode
    this->microphone_source_->start();
  }
}
#endif  // USE_MICROPHONE

}  // namespace esphome::audio_analyzer
