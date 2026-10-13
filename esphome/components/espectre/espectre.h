#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESPECTRE

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/optional.h"
#include <espectre_sdk.h>
#include <string>
#ifdef USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif
#ifdef USE_SELECT
#include "esphome/components/select/select.h"
#include "esphome/core/preferences.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome::espectre {

class ESPectreComponent final : public Component, public ::espectre::IRuntimeListener {
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
  void set_csi_traffic_multicast_group(const std::string &value) {
    this->runtime_.config().csi_traffic_multicast_group = value;
  }
  void set_motion_on_hits(uint8_t value) { this->runtime_.config().motion_on_hits = value; }
  void set_motion_off_hits(uint8_t value) { this->runtime_.config().motion_off_hits = value; }
  void set_wifi_band_policy(::espectre::WifiBandPolicy value) { this->runtime_.config().wifi_band_policy = value; }
  // Queue controls so entity automations cannot re-enter the SDK from a listener callback.
  void recalibrate() { this->recalibrate_pending_ = true; }
#ifdef USE_SELECT
  void set_traffic_mode_select(select::Select *value) { this->traffic_mode_select_ = value; }
  void request_traffic_generator_mode(::espectre::TrafficGeneratorMode mode) { this->pending_traffic_mode_ = mode; }
  /// Request a mode by its traffic_generator_mode YAML name; unknown names are ignored.
  void request_traffic_generator_mode(const char *name);
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
  // Called once per SDK detector evaluation (every 250 ms), which bounds the movement publish rate.
  void on_live_telemetry(const ::espectre::RuntimeSnapshot &snapshot) override { this->movement_pending_ = true; }
  void on_calibration_finished(const ::espectre::RuntimeSnapshot &snapshot, bool success) override;
  void on_runtime_fault(const char *message) override;
  void start_runtime_();
  void schedule_restart_();
  void invalidate_sensing_();
  void stop_();
#ifdef USE_SELECT
  void restore_traffic_mode_();
  void apply_pending_traffic_mode_();
  void publish_traffic_mode_();
#endif

  static constexpr uint32_t RESTART_DELAY_MS = 30000;

  ::espectre::RuntimeFrontendController runtime_;
  uint32_t restart_requested_ms_{0};
#ifdef USE_SELECT
  select::Select *traffic_mode_select_{nullptr};
  ESPPreferenceObject traffic_mode_pref_;
  optional<::espectre::TrafficGeneratorMode> pending_traffic_mode_;
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
