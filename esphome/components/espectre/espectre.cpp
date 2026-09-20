#include "espectre.h"

#ifdef USE_ESPECTRE

#include <cinttypes>
#include <cmath>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_ROAMING_SUPPRESSION)
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::espectre {

static const char *const TAG = "espectre";

struct TrafficModeOption {
  const char *name;
  ::espectre::TrafficGeneratorMode mode;
};
// Option names match the traffic_generator_mode YAML values.
static constexpr TrafficModeOption TRAFFIC_MODE_OPTIONS[] = {
    {"ping", ::espectre::TrafficGeneratorMode::PING},         {"dns", ::espectre::TrafficGeneratorMode::DNS},
    {"dns_tcp", ::espectre::TrafficGeneratorMode::DNS_TCP},   {"wifi_raw", ::espectre::TrafficGeneratorMode::WIFI_RAW},
    {"external", ::espectre::TrafficGeneratorMode::EXTERNAL},
};

static const char *traffic_mode_name(::espectre::TrafficGeneratorMode mode) {
  for (const auto &option : TRAFFIC_MODE_OPTIONS) {
    if (option.mode == mode)
      return option.name;
  }
  return nullptr;
}

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
  for (const auto &option : TRAFFIC_MODE_OPTIONS) {
    if (strcmp(option.name, name) == 0) {
      this->pending_traffic_mode_ = option.mode;
      return;
    }
  }
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
  // YAML is the only source of truth; do not restore or save controls in NVS.
  this->runtime_.config().persist_runtime_overrides = false;
#ifdef USE_SELECT
  this->restore_traffic_mode_();
#endif
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
void ESPectreComponent::restore_traffic_mode_() {
  if (this->traffic_mode_select_ == nullptr)
    return;
  // Keyed by the YAML mode, so changing it in YAML discards a mode saved from the select.
  const auto yaml_mode = this->runtime_.config().traffic_generator_mode;
  this->traffic_mode_pref_ =
      this->traffic_mode_select_->make_entity_preference<uint8_t>(static_cast<uint32_t>(yaml_mode) + 1);
  uint8_t saved;
  if (!this->traffic_mode_pref_.load(&saved))
    return;
  const auto mode = static_cast<::espectre::TrafficGeneratorMode>(saved);
  const char *name = traffic_mode_name(mode);
  if (name != nullptr && this->traffic_mode_select_->has_option(name))
    this->runtime_.config().traffic_generator_mode = mode;
}

void ESPectreComponent::apply_pending_traffic_mode_() {
  if (!this->pending_traffic_mode_.has_value())
    return;
  const auto mode = *this->pending_traffic_mode_;
  this->pending_traffic_mode_.reset();
  if (this->runtime_.set_traffic_generator_mode(mode)) {
    const auto saved = static_cast<uint8_t>(mode);
    this->traffic_mode_pref_.save(&saved);
  } else {
    ESP_LOGW(TAG, "Traffic generator mode %s was rejected", traffic_mode_name(mode));
  }
  this->publish_traffic_mode_();
}

void ESPectreComponent::publish_traffic_mode_() {
  if (this->traffic_mode_select_ == nullptr)
    return;
  const char *name = traffic_mode_name(this->runtime_.config().traffic_generator_mode);
  if (name != nullptr)
    this->traffic_mode_select_->publish_state(name);
}
#endif

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
  this->runtime_.loop();
  if (this->runtime_fault_)
    return;

  // Read once after the SDK finishes dispatching callbacks, including readiness changes.
  const auto &snapshot = this->runtime_.snapshot();
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
  if (this->movement_sensor_ != nullptr)
    this->movement_sensor_->publish_state(NAN);
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
  // The next runtime starts from the default threshold and publishes its calibration again.
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
                "  Motion on/off hits: %u/%u",
                config.detection_algorithm == ::espectre::DetectionAlgorithm::LIGHTWEIGHT
                    ? LOG_STR_LITERAL("lightweight")
                    : LOG_STR_LITERAL("high_accuracy"),
                csi_capture_profile_name(config.csi_capture_policy), traffic_mode_name(config.traffic_generator_mode),
                config.motion_on_hits, config.motion_off_hits);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Motion", this->motion_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Calibrating", this->calibrating_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Movement score", this->movement_sensor_);
#endif
#ifdef USE_SELECT
  LOG_SELECT("  ", "Traffic generator mode", this->traffic_mode_select_);
#endif
}

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
