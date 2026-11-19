#include "audio_analyzer.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::audio_analyzer {

ESPHOME_LOG_TAG(TAG, "audio_analyzer");

void AudioAnalyzer::dump_analysis_config_() {
  const SpectrumAnalysis &a = this->analysis_;
  ESP_LOGCONFIG(TAG,
                "  Bands: %u log-spaced, %.0f-%.0f Hz configured, %.0f-%.0f Hz analyzed at %.0f Hz\n"
                "  RMS range: %.0f..%.0f dBFS\n"
                "  Band range: %.0f..%.0f dBFS\n"
                "  Attack/release: %" PRIu32 "/%" PRIu32 " ms",
                NUM_BANDS, a.get_min_frequency(), a.get_max_frequency(), a.get_lower_edge(), a.get_upper_edge(),
                a.get_analysis_rate(), a.get_rms_min_db(), a.get_rms_max_db(), a.get_band_min_db(), a.get_band_max_db(),
                a.get_attack(), a.get_release());
}

void AudioAnalyzer::configure_analysis_(float analysis_rate) {
  const SpectrumAnalysis &a = this->analysis_;
  if (!this->analysis_.configure(analysis_rate)) {
    // WARN once per boot: a stream that switches between rates would repeat it on every switch
    if (this->clamp_warned_) {
      ESP_LOGD(TAG, "Range clamped for %.0f Hz", analysis_rate);
    } else {
      this->clamp_warned_ = true;
      ESP_LOGW(TAG, "%.0f Hz analysis rate: bands cover %.0f-%.0f Hz instead of %.0f-%.0f Hz", analysis_rate,
               a.get_lower_edge(), a.get_upper_edge(), a.get_min_frequency(), a.get_max_frequency());
    }
  }
  ESP_LOGD(TAG, "Analyzing at %.0f Hz, %.1f Hz bins, %.1f ms frames", analysis_rate, a.get_bin_width(),
           1000.0f * a.get_frame_seconds());
}

void AudioAnalyzer::on_frame_() {
  this->last_frame_ms_ = App.get_loop_component_start_time();
  this->active_ = true;
  this->frame_callback_.call();
}

void AudioAnalyzer::release_if_idle_() {
  const float frame_seconds = this->analysis_.get_frame_seconds();
  const uint32_t frame_ms = static_cast<uint32_t>(1000.0f * frame_seconds);
  const uint32_t now = App.get_loop_component_start_time();
  if ((now - this->last_frame_ms_) < 2 * frame_ms) {
    return;
  }
  this->active_ = false;
  if (this->analysis_.is_released()) {
    return;
  }
  this->analysis_.release(frame_seconds);
  this->last_frame_ms_ = now - frame_ms;  // next release step one frame from now
}

}  // namespace esphome::audio_analyzer
