#pragma once

// Lets the tests skip themselves when clang-tidy builds them against the real SDK.
#define ESPECTRE_SDK_TEST_DOUBLE

// SDK test double: hardware behavior belongs to the SDK; these tests exercise the ESPHome adapter.
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <functional>
#include <string>

namespace espectre {

enum class DetectionAlgorithm { LIGHTWEIGHT, HIGH_ACCURACY };
enum class CsiCapturePolicy { AUTO, LLTF, HT_VHT };
enum class TrafficGeneratorMode { PING, DNS, DNS_TCP, WIFI_RAW, EXTERNAL_HOST };
enum class TemporalCsiSpacing : uint8_t { HALF_SLOT, DISTINCT };
enum class WifiBandPolicy { BAND_2G, BAND_5G, AUTO };
enum class MotionState { IDLE, MOTION };
enum class LogLevel { ERROR, WARNING, INFO, DEBUG, VERBOSE };

constexpr uint8_t RUNTIME_MOTION_HITS_MIN = 1;
constexpr uint8_t RUNTIME_MOTION_HITS_MAX = 20;

constexpr bool runtime_detection_algorithm_valid(DetectionAlgorithm algorithm) {
  return algorithm == DetectionAlgorithm::LIGHTWEIGHT || algorithm == DetectionAlgorithm::HIGH_ACCURACY;
}
constexpr bool runtime_traffic_generator_mode_valid(TrafficGeneratorMode mode) {
  return mode == TrafficGeneratorMode::PING || mode == TrafficGeneratorMode::DNS ||
         mode == TrafficGeneratorMode::DNS_TCP || mode == TrafficGeneratorMode::WIFI_RAW ||
         mode == TrafficGeneratorMode::EXTERNAL_HOST;
}
constexpr bool runtime_csi_sample_spacing_valid(TemporalCsiSpacing spacing) {
  return spacing == TemporalCsiSpacing::HALF_SLOT || spacing == TemporalCsiSpacing::DISTINCT;
}

// Like the SDK, names never return nullptr and parsing falls back to the default.
inline const char *traffic_generator_mode_name(TrafficGeneratorMode mode) {
  switch (mode) {
    case TrafficGeneratorMode::DNS:
      return "dns";
    case TrafficGeneratorMode::DNS_TCP:
      return "dns_tcp";
    case TrafficGeneratorMode::WIFI_RAW:
      return "wifi_raw";
    case TrafficGeneratorMode::EXTERNAL_HOST:
      return "external";
    default:
      return "ping";
  }
}
inline TrafficGeneratorMode parse_traffic_generator_mode(const char *mode) {
  for (auto candidate : {TrafficGeneratorMode::DNS, TrafficGeneratorMode::DNS_TCP, TrafficGeneratorMode::WIFI_RAW,
                         TrafficGeneratorMode::EXTERNAL_HOST}) {
    if (mode != nullptr && std::strcmp(mode, traffic_generator_mode_name(candidate)) == 0)
      return candidate;
  }
  return TrafficGeneratorMode::PING;
}
inline const char *detection_algorithm_name(DetectionAlgorithm algorithm) {
  return algorithm == DetectionAlgorithm::HIGH_ACCURACY ? "high_accuracy" : "lightweight";
}
inline DetectionAlgorithm parse_detection_algorithm(const char *algorithm) {
  return algorithm != nullptr && std::strcmp(algorithm, "high_accuracy") == 0 ? DetectionAlgorithm::HIGH_ACCURACY
                                                                              : DetectionAlgorithm::LIGHTWEIGHT;
}
inline const char *csi_sample_spacing_name(TemporalCsiSpacing spacing) {
  return spacing == TemporalCsiSpacing::DISTINCT ? "distinct" : "half_slot";
}
inline TemporalCsiSpacing parse_csi_sample_spacing(const char *spacing) {
  return spacing != nullptr && std::strcmp(spacing, "distinct") == 0 ? TemporalCsiSpacing::DISTINCT
                                                                     : TemporalCsiSpacing::HALF_SLOT;
}

constexpr float runtime_default_threshold(DetectionAlgorithm algorithm) {
  return algorithm == DetectionAlgorithm::HIGH_ACCURACY ? 0.5f : 0.66218545f;
}

struct RuntimeConfig {
  DetectionAlgorithm detection_algorithm{DetectionAlgorithm::LIGHTWEIGHT};
  float threshold{runtime_default_threshold(DetectionAlgorithm::LIGHTWEIGHT)};
  bool runtime_detector_selection_enabled{false};
  CsiCapturePolicy csi_capture_policy{CsiCapturePolicy::AUTO};
  TrafficGeneratorMode traffic_generator_mode{TrafficGeneratorMode::PING};
  std::string traffic_generator_target_ip;
  std::string traffic_generator_target_mac{"auto"};
  TemporalCsiSpacing csi_sample_spacing{TemporalCsiSpacing::HALF_SLOT};
  std::string csi_traffic_multicast_group{"239.255.0.1"};
  uint8_t motion_on_hits{4};
  uint8_t motion_off_hits{3};
  uint32_t window_size_ms{1000};
  uint32_t csi_target_pps{100};
  uint32_t evaluation_interval_ms{250};
  bool lowpass_enabled{false};
  float lowpass_cutoff{11.0f};
  bool hampel_enabled{true};
  uint8_t hampel_window{7};
  float hampel_threshold{5.0f};
  WifiBandPolicy wifi_band_policy{WifiBandPolicy::AUTO};
  bool wifi_scan_results_managed_externally{false};
  bool persist_runtime_overrides{false};
};
struct RuntimeDiagnosticsSample {
  float generator_pps{0.0f};
  float traffic_tx_pps{0.0f};
  float traffic_rx_pps{0.0f};
  float csi_callback_pps{0.0f};
  float csi_accepted_pps{0.0f};
  float csi_missing_slots_pps{0.0f};
  float csi_excess_pps{0.0f};
  float csi_occupancy_ratio{0.0f};
};
struct RuntimeSnapshot {
  bool ready_to_publish{false};
  bool calibrating{false};
  MotionState motion_state{MotionState::IDLE};
  float movement_metric{0.0f};
  float threshold{runtime_default_threshold(DetectionAlgorithm::LIGHTWEIGHT)};
};
class IRuntimeListener {
 public:
  virtual ~IRuntimeListener() = default;
  virtual void on_live_telemetry(const RuntimeSnapshot &snapshot) {}
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
  bool set_threshold(float threshold) {
    if (threshold < 0.0f || threshold > 1.0f)
      return false;
    snapshot_.threshold = threshold;
    return true;
  }
  bool set_motion_hits(uint8_t motion_on_hits, uint8_t motion_off_hits) {
    if (motion_on_hits < RUNTIME_MOTION_HITS_MIN || motion_on_hits > RUNTIME_MOTION_HITS_MAX ||
        motion_off_hits < RUNTIME_MOTION_HITS_MIN || motion_off_hits > RUNTIME_MOTION_HITS_MAX)
      return false;
    config_.motion_on_hits = motion_on_hits;
    config_.motion_off_hits = motion_off_hits;
    return true;
  }
  bool set_detection_algorithm(DetectionAlgorithm algorithm) {
    // Like the SDK, the setting at setup decides whether the runtime can switch detectors.
    if (!config_at_setup_.runtime_detector_selection_enabled)
      return false;
    config_.detection_algorithm = algorithm;
    snapshot_.threshold = runtime_default_threshold(algorithm);
    return true;
  }
  bool set_csi_sample_spacing(TemporalCsiSpacing spacing) {
    config_.csi_sample_spacing = spacing;
    return true;
  }
  void set_services_armed(bool armed) { services_armed_ = armed; }
  bool services_armed() const { return services_armed_; }

  inline static RuntimeFrontendController *instance{nullptr};
  RuntimeConfig config_;
  RuntimeConfig config_at_setup_;
  RuntimeSnapshot snapshot_;
  RuntimeDiagnosticsSample diagnostics_sample_;
  bool services_armed_{true};
  IRuntimeListener *listener{nullptr};
  std::function<void()> loop_hook;
  bool setup_result{true};
  bool shutdown_called{false};
  unsigned recalibration_calls{0};
  unsigned setup_calls{0};
};

}  // namespace espectre
