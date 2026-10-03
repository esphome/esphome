#pragma once

// Lets the tests skip themselves when clang-tidy builds them against the real SDK.
#define ESPECTRE_SDK_TEST_DOUBLE

// SDK test double: hardware behavior belongs to the SDK; these tests exercise the ESPHome adapter.
#include <cstdarg>
#include <cstdint>
#include <functional>
#include <string>

namespace espectre {

enum class DetectionAlgorithm { LIGHTWEIGHT, HIGH_ACCURACY };
enum class CsiCapturePolicy { AUTO, LLTF, HT_VHT };
enum class TrafficGeneratorMode { PING, DNS, DNS_TCP, WIFI_RAW, EXTERNAL };
enum class WifiBandPolicy { BAND_2G, BAND_5G, AUTO };
enum class MotionState { IDLE, MOTION };
enum class LogLevel { ERROR, WARNING, INFO, DEBUG, VERBOSE };

constexpr float runtime_default_threshold(DetectionAlgorithm algorithm) {
  return algorithm == DetectionAlgorithm::HIGH_ACCURACY ? 0.5f : 0.66218545f;
}

struct RuntimeConfig {
  DetectionAlgorithm detection_algorithm{DetectionAlgorithm::LIGHTWEIGHT};
  float threshold{runtime_default_threshold(DetectionAlgorithm::LIGHTWEIGHT)};
  CsiCapturePolicy csi_capture_policy{CsiCapturePolicy::AUTO};
  TrafficGeneratorMode traffic_generator_mode{TrafficGeneratorMode::PING};
  std::string traffic_generator_target_ip;
  std::string csi_traffic_multicast_group{"239.255.0.1"};
  uint8_t motion_on_hits{4};
  uint8_t motion_off_hits{3};
  WifiBandPolicy wifi_band_policy{WifiBandPolicy::AUTO};
  bool wifi_scan_results_managed_externally{false};
  bool persist_runtime_overrides{true};
};
struct RuntimeDiagnosticsSample {
  float generator_pps{0.0f};
  float traffic_tx_pps{0.0f};
  float traffic_rx_pps{0.0f};
  float csi_accepted_pps{0.0f};
  float csi_occupancy_ratio{0.0f};
};
struct RuntimeSnapshot {
  bool ready_to_publish{false};
  bool calibrating{false};
  MotionState motion_state{MotionState::IDLE};
  float movement_metric{0.0f};
};
class IRuntimeListener {
 public:
  virtual ~IRuntimeListener() = default;
  virtual void on_live_telemetry(float movement, float threshold) {}
  virtual void on_calibration_finished(const RuntimeSnapshot &snapshot, bool success) {}
  virtual void on_runtime_fault(const char *message) {}
};
struct LogSink {
  void *context;
  bool (*enabled)(void *, LogLevel, const char *);
  void (*write)(void *, LogLevel, const char *, int, const char *, va_list);
};
inline bool set_log_sink(const LogSink &sink) { return true; }
inline void clear_log_sink() {}

class RuntimeFrontendController {
 public:
  RuntimeFrontendController() { instance = this; }
  RuntimeConfig &config() { return config_; }
  const RuntimeSnapshot &snapshot() const { return snapshot_; }
  const RuntimeDiagnosticsSample *diagnostics_sample() const { return &diagnostics_sample_; }
  bool setup(IRuntimeListener *listener) {
    setup_calls++;
    this->listener = listener;
    this->config_at_setup_ = this->config_;
    return setup_result;
  }
  void loop() {
    if (loop_hook)
      loop_hook();
  }
  bool trigger_recalibration() {
    recalibration_calls++;
    return true;
  }
  void shutdown() { shutdown_called = true; }
  bool set_traffic_generator_mode(TrafficGeneratorMode mode) {
    config_.traffic_generator_mode = mode;
    return true;
  }

  inline static RuntimeFrontendController *instance{nullptr};
  RuntimeConfig config_;
  RuntimeConfig config_at_setup_;
  RuntimeSnapshot snapshot_;
  RuntimeDiagnosticsSample diagnostics_sample_;
  IRuntimeListener *listener{nullptr};
  std::function<void()> loop_hook;
  bool setup_result{true};
  bool shutdown_called{false};
  unsigned recalibration_calls{0};
  unsigned setup_calls{0};
};

}  // namespace espectre
