#include "espectre.h"

#ifdef USE_ESPECTRE

#include <cinttypes>
#include <cmath>
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_ROAMING_SUPPRESSION)
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::espectre {

ESPHOME_LOG_TAG(TAG, "espectre");

static const char *csi_capture_profile_name(::espectre::CsiCapturePolicy profile) {
  switch (profile) {
    case ::espectre::CsiCapturePolicy::LLTF:
      return LOG_STR_LITERAL("lltf");
    case ::espectre::CsiCapturePolicy::HT_VHT:
      return LOG_STR_LITERAL("ht_vht");
    default:
      return LOG_STR_LITERAL("auto");
  }
}

#ifdef USE_SELECT
void ESPectreComponent::request_traffic_generator_mode(const char *name) {
  this->pending_traffic_mode_ = ::espectre::parse_traffic_generator_mode(name);
}

void ESPectreComponent::request_detection_algorithm(const char *name) {
  this->pending_detection_algorithm_ = ::espectre::parse_detection_algorithm(name);
}

void ESPectreComponent::request_csi_sample_spacing(const char *name) {
  this->pending_csi_sample_spacing_ = ::espectre::parse_csi_sample_spacing(name);
}
#endif

static int log_level(::espectre::LogLevel level) {
  switch (level) {
    case ::espectre::LogLevel::ERROR:
      return ESPHOME_LOG_LEVEL_ERROR;
    case ::espectre::LogLevel::WARNING:
      return ESPHOME_LOG_LEVEL_WARN;
    case ::espectre::LogLevel::INFO:
      return ESPHOME_LOG_LEVEL_INFO;
    case ::espectre::LogLevel::DEBUG:
      return ESPHOME_LOG_LEVEL_DEBUG;
    default:
      return ESPHOME_LOG_LEVEL_VERBOSE;
  }
}

void ESPectreComponent::setup() {
  // ESPHome owns scan results, including scans requested by the SDK's CSI recovery.
  this->runtime_.config().wifi_scan_results_managed_externally = true;
  this->restore_controls_();
  this->start_runtime_();
}

void ESPectreComponent::start_runtime_() {
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_ROAMING_SUPPRESSION)
  // A sensor stays in one place, and each roaming scan takes the radio off-channel for
  // seconds, emptying the CSI window. Losing the access point still reconnects normally.
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->request_roaming_suppression();
    this->roaming_suppressed_ = true;
  }
#endif
  ::espectre::set_log_sink({
      .context = nullptr,
      .enabled = [](void *, ::espectre::LogLevel level, const char *) { return log_level(level) <= ESPHOME_LOG_LEVEL; },
      .write = [](void *, ::espectre::LogLevel level, const char *tag, int line, const char *format,
                  va_list args) { esp_log_vprintf_(log_level(level), tag, line, format, args); },
  });
  if (!this->runtime_.setup(this)) {
    ESP_LOGE(TAG, "Runtime setup failed");
    this->stop_();
    this->schedule_restart_();
    return;
  }
  this->running_ = true;
  this->status_clear_error();
#ifdef USE_SELECT
  this->publish_traffic_mode_();
#endif
  this->publish_controls_();
}

void ESPectreComponent::schedule_restart_() {
  // A fault can be transient, such as a Wi-Fi stall, so retry instead of failing for good.
  this->runtime_fault_ = false;
  this->status_set_error(LOG_STR("Runtime stopped"));
  ESP_LOGW(TAG, "Restarting the runtime in %" PRIu32 " s", RESTART_DELAY_MS / 1000);
  this->restart_pending_ = true;
  this->restart_requested_ms_ = App.get_loop_component_start_time();
}

#ifdef USE_SELECT
// Selections are saved as SDK enum values, so renumbering an enum would restore the wrong option.
static_assert(static_cast<uint8_t>(::espectre::TrafficGeneratorMode::PING) == 0 &&
                  static_cast<uint8_t>(::espectre::TrafficGeneratorMode::DNS) == 1 &&
                  static_cast<uint8_t>(::espectre::TrafficGeneratorMode::DNS_TCP) == 2 &&
                  static_cast<uint8_t>(::espectre::TrafficGeneratorMode::WIFI_RAW) == 3 &&
                  static_cast<uint8_t>(::espectre::TrafficGeneratorMode::EXTERNAL_HOST) == 4,
              "Saved traffic generator modes depend on these values");
static_assert(static_cast<uint8_t>(::espectre::DetectionAlgorithm::LIGHTWEIGHT) == 0 &&
                  static_cast<uint8_t>(::espectre::DetectionAlgorithm::HIGH_ACCURACY) == 1,
              "Saved detection algorithms depend on these values");
static_assert(static_cast<uint8_t>(::espectre::TemporalCsiSpacing::HALF_SLOT) == 0 &&
                  static_cast<uint8_t>(::espectre::TemporalCsiSpacing::DISTINCT) == 1,
              "Saved CSI sample spacings depend on these values");

template<typename T, typename F>
static void restore_select_choice(select::Select *select, ESPPreferenceObject &pref, T &value, F accept) {
  if (select == nullptr)
    return;
  pref = select->make_entity_preference<uint8_t>(static_cast<uint32_t>(value) + 1);
  uint8_t saved;
  if (pref.load(&saved) && accept(static_cast<T>(saved)))
    value = static_cast<T>(saved);
}

// Returns false when no choice was queued; otherwise the caller republishes the active option.
template<typename T>
static bool apply_select_choice(::espectre::RuntimeFrontendController &runtime, optional<T> &pending,
                                ESPPreferenceObject &pref, bool (::espectre::RuntimeFrontendController::*set)(T),
                                const char *(*name)(T)) {
  if (!pending.has_value())
    return false;
  const T value = *pending;
  pending.reset();
  if ((runtime.*set)(value)) {
    const auto saved = static_cast<uint8_t>(value);
    pref.save(&saved);
  } else {
    ESP_LOGW(TAG, "Option %s was rejected", name(value));
  }
  return true;
}

void ESPectreComponent::apply_pending_traffic_mode_() {
  if (apply_select_choice(this->runtime_, this->pending_traffic_mode_, this->traffic_mode_pref_,
                          &::espectre::RuntimeFrontendController::set_traffic_generator_mode,
                          ::espectre::traffic_generator_mode_name))
    this->publish_traffic_mode_();
}

void ESPectreComponent::publish_traffic_mode_() {
  if (this->traffic_mode_select_ == nullptr)
    return;
  this->traffic_mode_select_->publish_state(
      ::espectre::traffic_generator_mode_name(this->runtime_.config().traffic_generator_mode));
}
#endif

#ifdef USE_NUMBER
static void restore_motion_hits(number::Number *number, ESPPreferenceObject &pref, uint8_t &hits) {
  if (number == nullptr)
    return;
  pref = number->make_entity_preference<uint8_t>(hits);
  uint8_t saved;
  if (pref.load(&saved) && saved >= ::espectre::RUNTIME_MOTION_HITS_MIN && saved <= ::espectre::RUNTIME_MOTION_HITS_MAX)
    hits = saved;
}
#endif

void ESPectreComponent::restore_controls_() {
  // Saved values are keyed by the YAML value, so a YAML change discards what the entities saved.
  auto &config = this->runtime_.config();
#ifdef USE_NUMBER
  restore_motion_hits(this->motion_on_hits_number_, this->motion_on_hits_pref_, config.motion_on_hits);
  restore_motion_hits(this->motion_off_hits_number_, this->motion_off_hits_pref_, config.motion_off_hits);
#endif
#ifdef USE_SELECT
  restore_select_choice(this->traffic_mode_select_, this->traffic_mode_pref_, config.traffic_generator_mode,
                        [this](::espectre::TrafficGeneratorMode mode) {
                          return ::espectre::runtime_traffic_generator_mode_valid(mode) &&
                                 this->traffic_mode_select_->has_option(::espectre::traffic_generator_mode_name(mode));
                        });
  // Without this the SDK rejects every detector change once the runtime runs.
  if (this->detection_algorithm_select_ != nullptr)
    config.runtime_detector_selection_enabled = true;
  restore_select_choice(this->detection_algorithm_select_, this->detection_algorithm_pref_, config.detection_algorithm,
                        ::espectre::runtime_detection_algorithm_valid);
  restore_select_choice(this->csi_sample_spacing_select_, this->csi_sample_spacing_pref_, config.csi_sample_spacing,
                        ::espectre::runtime_csi_sample_spacing_valid);
#endif
#ifdef USE_SWITCH
  // Applied before the first runtime starts, so a disabled sensor never starts sensing.
  if (this->sensing_switch_ != nullptr)
    this->runtime_.set_services_armed(this->sensing_switch_->get_initial_state_with_restore_mode().value_or(true));
#endif
}

void ESPectreComponent::apply_pending_controls_() {
#ifdef USE_NUMBER
  if (!std::isnan(this->pending_threshold_)) {
    const float threshold = this->pending_threshold_;
    this->pending_threshold_ = NAN;
    if (!this->runtime_.set_threshold(threshold)) {
      ESP_LOGW(TAG, "Threshold %.2f was rejected", threshold);
    }
    // Republished even when rejected, so the entity drops the value it was asked for.
    PUBLISH_STATE(this->threshold_number_, this->runtime_.snapshot().threshold);
  }
  if (this->pending_motion_on_hits_ != 0 || this->pending_motion_off_hits_ != 0) {
    const auto &config = this->runtime_.config();
    const uint8_t on_hits = this->pending_motion_on_hits_ != 0 ? this->pending_motion_on_hits_ : config.motion_on_hits;
    const uint8_t off_hits =
        this->pending_motion_off_hits_ != 0 ? this->pending_motion_off_hits_ : config.motion_off_hits;
    this->pending_motion_on_hits_ = 0;
    this->pending_motion_off_hits_ = 0;
    if (this->runtime_.set_motion_hits(on_hits, off_hits)) {
      this->motion_on_hits_pref_.save(&on_hits);
      this->motion_off_hits_pref_.save(&off_hits);
    } else {
      ESP_LOGW(TAG, "Motion on/off hits %u/%u were rejected", on_hits, off_hits);
    }
    PUBLISH_STATE(this->motion_on_hits_number_, config.motion_on_hits);
    PUBLISH_STATE(this->motion_off_hits_number_, config.motion_off_hits);
  }
#endif
#ifdef USE_SELECT
  // Only the changed entity is republished, because numbers and selects run on_value on every publish.
  // loop() republishes the threshold that a detector change replaces.
  if (apply_select_choice(this->runtime_, this->pending_detection_algorithm_, this->detection_algorithm_pref_,
                          &::espectre::RuntimeFrontendController::set_detection_algorithm,
                          ::espectre::detection_algorithm_name)) {
    PUBLISH_STATE(this->detection_algorithm_select_,
                  ::espectre::detection_algorithm_name(this->runtime_.config().detection_algorithm));
  }
  if (apply_select_choice(this->runtime_, this->pending_csi_sample_spacing_, this->csi_sample_spacing_pref_,
                          &::espectre::RuntimeFrontendController::set_csi_sample_spacing,
                          ::espectre::csi_sample_spacing_name)) {
    PUBLISH_STATE(this->csi_sample_spacing_select_,
                  ::espectre::csi_sample_spacing_name(this->runtime_.config().csi_sample_spacing));
  }
#endif
#ifdef USE_SWITCH
  if (this->pending_sensing_.has_value()) {
    this->runtime_.set_services_armed(*this->pending_sensing_);
    this->pending_sensing_.reset();
    PUBLISH_STATE(this->sensing_switch_, this->runtime_.services_armed());
  }
#endif
}

void ESPectreComponent::publish_controls_() {
#ifdef USE_NUMBER
  PUBLISH_STATE(this->threshold_number_, this->runtime_.snapshot().threshold);
  PUBLISH_STATE(this->motion_on_hits_number_, this->runtime_.config().motion_on_hits);
  PUBLISH_STATE(this->motion_off_hits_number_, this->runtime_.config().motion_off_hits);
#endif
#ifdef USE_SELECT
  PUBLISH_STATE(this->detection_algorithm_select_,
                ::espectre::detection_algorithm_name(this->runtime_.config().detection_algorithm));
  PUBLISH_STATE(this->csi_sample_spacing_select_,
                ::espectre::csi_sample_spacing_name(this->runtime_.config().csi_sample_spacing));
#endif
#ifdef USE_SWITCH
  PUBLISH_STATE(this->sensing_switch_, this->runtime_.services_armed());
#endif
}

void ESPectreComponent::loop() {
  if (this->runtime_fault_) {
    this->stop_();
    this->schedule_restart_();
    return;
  }
  if (!this->running_) {
    // Without a backend this only reaps a traffic worker that outlived the last runtime.
    this->runtime_.loop();
    if (this->restart_pending_ &&
        App.get_loop_component_start_time() - this->restart_requested_ms_ >= RESTART_DELAY_MS) {
      this->restart_pending_ = false;
      this->start_runtime_();
    }
    return;
  }
  if (this->recalibrate_pending_) {
    this->recalibrate_pending_ = false;
    if (!this->runtime_.trigger_recalibration()) {
      ESP_LOGW(TAG, "Recalibration is not available");
    }
  }
#ifdef USE_SELECT
  this->apply_pending_traffic_mode_();
#endif
  this->apply_pending_controls_();
  this->runtime_.loop();
  if (this->runtime_fault_)
    return;

  // Read once after the SDK finishes dispatching callbacks, including readiness changes.
  const auto &snapshot = this->runtime_.snapshot();
#ifdef USE_NUMBER
  // Calibration and a detector change both replace the threshold.
  if (this->threshold_number_ != nullptr && this->threshold_number_->state != snapshot.threshold)
    this->threshold_number_->publish_state(snapshot.threshold);
#endif
#ifdef USE_BINARY_SENSOR
  if (this->calibrating_binary_sensor_ != nullptr &&
      (!this->calibrating_published_ || this->calibrating_state_ != snapshot.calibrating)) {
    this->calibrating_state_ = snapshot.calibrating;
    this->calibrating_published_ = true;
    this->calibrating_binary_sensor_->publish_state(snapshot.calibrating);
  }
#endif
  if (!snapshot.ready_to_publish) {
    if (this->ready_)
      this->invalidate_sensing_();
    this->movement_pending_ = false;
    return;
  }

#ifdef USE_BINARY_SENSOR
  const bool motion = snapshot.motion_state == ::espectre::MotionState::MOTION;
  if (this->motion_binary_sensor_ != nullptr && (!this->ready_ || this->motion_state_ != motion)) {
    this->motion_state_ = motion;
    this->motion_binary_sensor_->publish_state(motion);
  }
#endif
#ifdef USE_SENSOR
  if (this->movement_sensor_ != nullptr && (this->movement_pending_ || !this->ready_))
    this->movement_sensor_->publish_state(snapshot.movement_metric);
#endif
  this->movement_pending_ = false;
  this->ready_ = true;
}

void ESPectreComponent::invalidate_sensing_() {
#ifdef USE_BINARY_SENSOR
  if (this->motion_binary_sensor_ != nullptr)
    this->motion_binary_sensor_->invalidate_state();
#endif
#ifdef USE_SENSOR
  PUBLISH_STATE(this->movement_sensor_, NAN);
#endif
  this->ready_ = false;
}

void ESPectreComponent::on_calibration_finished(const ::espectre::RuntimeSnapshot &snapshot, bool success) {
  if (success) {
    this->calibrated_ = true;
    this->status_clear_warning();
    ESP_LOGI(TAG, "Calibration complete");
    return;
  }
  // After a successful calibration, a failed one keeps that calibrated threshold.
  if (!this->calibrated_)
    this->status_set_warning(LOG_STR("Calibration failed"));
  ESP_LOGW(TAG, "Calibration failed; retaining the previous threshold");
}

void ESPectreComponent::on_runtime_fault(const char *message) {
  ESP_LOGE(TAG, "Runtime fault: %s", message);
  this->runtime_fault_ = true;
}

void ESPectreComponent::stop_() {
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_ROAMING_SUPPRESSION)
  if (this->roaming_suppressed_ && wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->release_roaming_suppression();
  }
#endif
  this->roaming_suppressed_ = false;
  this->running_ = false;
  // The next runtime reports its own calibration, so forget this one's.
  this->calibrated_ = false;
  this->calibrating_published_ = false;
  this->runtime_.shutdown();
  ::espectre::clear_log_sink();
  this->invalidate_sensing_();
#ifdef USE_BINARY_SENSOR
  if (this->calibrating_binary_sensor_ != nullptr)
    this->calibrating_binary_sensor_->invalidate_state();
#endif
}

void ESPectreComponent::on_shutdown() {
  this->restart_pending_ = false;
  this->stop_();
}

void ESPectreComponent::dump_config() {
  const auto &config = this->runtime_.config();
  ESP_LOGCONFIG(TAG,
                "ESPectre:\n"
                "  Detection algorithm: %s\n"
                "  CSI capture profile: %s\n"
                "  Traffic generator mode: %s\n"
                "  Traffic generator target MAC: %s\n"
                "  CSI sample spacing: %s\n"
                "  Motion on/off hits: %u/%u\n"
                "  Window size: %" PRIu32 " ms\n"
                "  CSI target rate: %" PRIu32 " pps\n"
                "  Evaluation interval: %" PRIu32 " ms\n"
                "  Low-pass filter: %s (%.1f Hz)\n"
                "  Hampel filter: %s (window %u, threshold %.1f)",
                ::espectre::detection_algorithm_name(config.detection_algorithm),
                csi_capture_profile_name(config.csi_capture_policy),
                ::espectre::traffic_generator_mode_name(config.traffic_generator_mode),
                config.traffic_generator_target_mac.c_str(),
                ::espectre::csi_sample_spacing_name(config.csi_sample_spacing), config.motion_on_hits,
                config.motion_off_hits, config.window_size_ms, config.csi_target_pps, config.evaluation_interval_ms,
                ONOFF(config.lowpass_enabled), config.lowpass_cutoff, ONOFF(config.hampel_enabled),
                config.hampel_window, config.hampel_threshold);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Motion", this->motion_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Calibrating", this->calibrating_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Movement score", this->movement_sensor_);
#endif
#ifdef USE_NUMBER
  LOG_NUMBER("  ", "Threshold", this->threshold_number_);
  LOG_NUMBER("  ", "Motion on hits", this->motion_on_hits_number_);
  LOG_NUMBER("  ", "Motion off hits", this->motion_off_hits_number_);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Traffic generator mode", this->traffic_mode_select_);
  LOG_SELECT("  ", "Detection algorithm", this->detection_algorithm_select_);
  LOG_SELECT("  ", "CSI sample spacing", this->csi_sample_spacing_select_);
#endif
#ifdef USE_SWITCH
  LOG_SWITCH("  ", "Sensing", this->sensing_switch_);
#endif
}

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
