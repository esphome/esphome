#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include <espectre_sdk.h>
#include <cmath>
#include <string>
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif
#ifdef USE_SWITCH
#include "esphome/components/switch/switch.h"
#endif

namespace esphome::espectre {

class ESPectreComponent final : public Component, public ::espectre::IRuntimeListener {
#ifdef USE_NUMBER
  SUB_NUMBER(threshold)
  SUB_NUMBER(motion_on_hits)
  SUB_NUMBER(motion_off_hits)
#endif
#ifdef USE_SELECT
  SUB_SELECT(traffic_mode)
  SUB_SELECT(detection_algorithm)
  SUB_SELECT(csi_sample_spacing)
#endif
#ifdef USE_SWITCH
  SUB_SWITCH(sensing)
#endif

 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;
  // Register the SDK's Wi-Fi event handlers after network setup, before station startup.
  float get_setup_priority() const override { return setup_priority::WIFI + 1.0f; }

  void set_detection_algorithm(::espectre::DetectionAlgorithm value) {
    this->runtime_.config().detection_algorithm = value;
  }
  void set_csi_capture_profile(::espectre::CsiCapturePolicy value) {
    this->runtime_.config().csi_capture_policy = value;
  }
  void set_traffic_generator_mode(::espectre::TrafficGeneratorMode value) {
    this->runtime_.config().traffic_generator_mode = value;
  }
  void set_traffic_generator_target_ip(const std::string &value) {
    this->runtime_.config().traffic_generator_target_ip = value;
  }
  void set_traffic_generator_target_mac(const std::string &value) {
    this->runtime_.config().traffic_generator_target_mac = value;
  }
  void set_csi_sample_spacing(::espectre::TemporalCsiSpacing value) {
    this->runtime_.config().csi_sample_spacing = value;
  }
  void set_csi_traffic_multicast_group(const std::string &value) {
    this->runtime_.config().csi_traffic_multicast_group = value;
  }
  void set_motion_on_hits(uint8_t value) { this->runtime_.config().motion_on_hits = value; }
  void set_motion_off_hits(uint8_t value) { this->runtime_.config().motion_off_hits = value; }
  void set_window_size_ms(uint32_t value) { this->runtime_.config().window_size_ms = value; }
  void set_csi_target_pps(uint32_t value) { this->runtime_.config().csi_target_pps = value; }
  void set_evaluation_interval_ms(uint32_t value) { this->runtime_.config().evaluation_interval_ms = value; }
  void set_lowpass_enabled(bool value) { this->runtime_.config().lowpass_enabled = value; }
  void set_lowpass_cutoff(float value) { this->runtime_.config().lowpass_cutoff = value; }
  void set_hampel_enabled(bool value) { this->runtime_.config().hampel_enabled = value; }
  void set_hampel_window(uint8_t value) { this->runtime_.config().hampel_window = value; }
  void set_hampel_threshold(float value) { this->runtime_.config().hampel_threshold = value; }
  void set_wifi_band_policy(::espectre::WifiBandPolicy value) { this->runtime_.config().wifi_band_policy = value; }
  // Queue controls so entity automations cannot re-enter the SDK from a listener callback.
  void recalibrate() { this->recalibrate_pending_ = true; }
#ifdef USE_NUMBER
  void request_threshold(float value) { this->pending_threshold_ = value; }
  void request_motion_on_hits(float value) { this->pending_motion_on_hits_ = static_cast<uint8_t>(std::lround(value)); }
  void request_motion_off_hits(float value) {
    this->pending_motion_off_hits_ = static_cast<uint8_t>(std::lround(value));
  }
#endif
#ifdef USE_SELECT
  void request_traffic_generator_mode(::espectre::TrafficGeneratorMode mode) { this->pending_traffic_mode_ = mode; }
  /// Request a mode by its traffic_generator_mode YAML name, as the select offers it.
  void request_traffic_generator_mode(const char *name);
  /// Request an algorithm by its detection_algorithm YAML name, as the select offers it.
  void request_detection_algorithm(const char *name);
  /// Request a spacing rule by its csi_sample_spacing YAML name, as the select offers it.
  void request_csi_sample_spacing(const char *name);
#endif
#ifdef USE_SWITCH
  void request_sensing(bool enabled) { this->pending_sensing_ = enabled; }
#endif

#ifdef USE_BINARY_SENSOR
  SUB_BINARY_SENSOR(motion)
  SUB_BINARY_SENSOR(calibrating)
#endif
#ifdef USE_SENSOR
  SUB_SENSOR(movement)
#endif
  /// Latest one-second runtime diagnostics sample, or nullptr before the runtime starts.
  const ::espectre::RuntimeDiagnosticsSample *diagnostics_sample() const { return this->runtime_.diagnostics_sample(); }

 protected:
  // Called once per SDK detector evaluation (every evaluation_interval), which bounds the movement publish rate.
  void on_live_telemetry(const ::espectre::RuntimeSnapshot &snapshot) override { this->movement_pending_ = true; }
  void on_calibration_finished(const ::espectre::RuntimeSnapshot &snapshot, bool success) override;
  void on_runtime_fault(const char *message) override;
  void start_runtime_();
  void schedule_restart_();
  void invalidate_sensing_();
  void stop_();
#ifdef USE_SELECT
  void apply_pending_traffic_mode_();
  void publish_traffic_mode_();
#endif
  // Threshold, motion hits, detection algorithm, sample spacing and sensing.
  void restore_controls_();
  void apply_pending_controls_();
  void publish_controls_();

  static constexpr uint32_t RESTART_DELAY_MS = 30000;

  ::espectre::RuntimeFrontendController runtime_;
  uint32_t restart_requested_ms_{0};
#ifdef USE_NUMBER
  ESPPreferenceObject motion_on_hits_pref_;
  ESPPreferenceObject motion_off_hits_pref_;
  float pending_threshold_{NAN};
  // Zero means no request; the runtime accepts 1 to 20.
  uint8_t pending_motion_on_hits_{0};
  uint8_t pending_motion_off_hits_{0};
#endif
#ifdef USE_SELECT
  ESPPreferenceObject traffic_mode_pref_;
  ESPPreferenceObject detection_algorithm_pref_;
  ESPPreferenceObject csi_sample_spacing_pref_;
  optional<::espectre::TrafficGeneratorMode> pending_traffic_mode_;
  optional<::espectre::DetectionAlgorithm> pending_detection_algorithm_;
  optional<::espectre::TemporalCsiSpacing> pending_csi_sample_spacing_;
#endif
#ifdef USE_SWITCH
  optional<bool> pending_sensing_;
#endif
  bool recalibrate_pending_{false};
  bool movement_pending_{false};
  bool ready_{false};
  bool motion_state_{false};
  bool calibrating_state_{false};
  bool calibrating_published_{false};
  bool runtime_fault_{false};
  bool running_{false};
  bool restart_pending_{false};
  bool calibrated_{false};
  bool roaming_suppressed_{false};
};

}  // namespace esphome::espectre

#endif  // USE_ESPECTRE
