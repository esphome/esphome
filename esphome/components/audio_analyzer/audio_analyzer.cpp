#include "audio_analyzer.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

// More samples due than this means loop() was blocked: skip to the newest frame instead of catching up
static constexpr uint32_t MAX_BACKLOG_SAMPLES = 2 * FFT_SIZE;
// The overflow warning clears after this long without an overflow
static constexpr uint32_t OVERFLOW_CLEAR_MS = 1000;
// Release steps applied in one loop pass at most, after a long stall
static constexpr uint32_t MAX_RELEASE_STEPS = 32;

void AudioAnalyzer::dump_analysis_config_() {
  const SpectrumAnalysis &a = this->analysis_;
  ESP_LOGCONFIG(TAG,
                "  Analysis rate: %.0f Hz (decimation %u)\n"
                "  Bands: %u log-spaced, %.0f-%.0f Hz\n"
                "  RMS range: %.0f..%.0f dBFS\n"
                "  Band range: %.0f..%.0f dBFS\n"
                "  Buffer: %" PRIu32 " ms",
                a.get_analysis_rate(), this->tap_.get_decimation(), NUM_BANDS, a.get_lower_edge(), a.get_upper_edge(),
                a.get_rms_min_db(), a.get_rms_max_db(), a.get_band_min_db(), a.get_band_max_db(), this->buffer_ms_());
}

uint32_t AudioAnalyzer::buffered_samples_() {
  if (this->tap_.drain_if_restarting()) {
    this->analysis_.reset_frame();
  }
  return this->tap_.get_fifo().available();
}

void AudioAnalyzer::analyze_(uint32_t due, bool silent) {
  SampleFifo &fifo = this->tap_.get_fifo();
  if (due > MAX_BACKLOG_SAMPLES) {
    // Being current matters more than analyzing every sample
    fifo.skip(due - FFT_SIZE);
    this->analysis_.reset_frame();
    due = FFT_SIZE;
  }
  fifo.consume(due, [this, silent](int16_t sample) {
    this->push_sample_(silent ? 0.0f : static_cast<float>(sample) * (1.0f / 32768.0f));
  });
}

bool AudioAnalyzer::update_overflow_warning_(uint32_t now) {
  if (this->tap_.take_overflow()) {
    const bool raised = !this->status_has_warning();
    this->status_set_warning(LOG_STR("Analysis buffer full"));
    this->last_overflow_ms_ = now;
    return raised;
  }
  if (this->status_has_warning() && now - this->last_overflow_ms_ > OVERFLOW_CLEAR_MS) {
    this->status_clear_warning();
  }
  return false;
}

bool AudioAnalyzer::push_sample_(float sample) {
  if (!this->analysis_.push(sample)) {
    return false;
  }
  this->last_update_ms_ = App.get_loop_component_start_time();
  this->active_ = true;
  this->frame_callback_.call();
  return true;
}

void AudioAnalyzer::release_if_idle_(uint32_t now) {
  const uint32_t frame_ms = this->analysis_.get_frame_ms();
  const uint32_t elapsed = now - this->last_update_ms_;
  // One frame period of grace: the next frame may just be late
  if (elapsed < 2 * frame_ms) {
    return;
  }
  this->active_ = false;
  // One step per frame period of silence, the first one frame after the last frame
  uint32_t steps = elapsed / frame_ms - 1;
  if (steps > MAX_RELEASE_STEPS) {
    steps = MAX_RELEASE_STEPS;
    this->last_update_ms_ = now - frame_ms * (MAX_RELEASE_STEPS + 1);
  }
  for (; steps > 0 && !this->analysis_.is_released(); --steps) {
    this->analysis_.release_step();
    this->last_update_ms_ += frame_ms;
  }
}

}  // namespace esphome::audio_analyzer
