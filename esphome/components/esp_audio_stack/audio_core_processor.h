#pragma once
#ifndef ESPHOME_AUDIO_CORE_PROCESSOR_H
#define ESPHOME_AUDIO_CORE_PROCESSOR_H

#include <cstdint>
#include <cstddef>

namespace esphome::esp_audio_stack {

/// Audio processing features that can be queried and toggled.
enum class AudioFeature : uint8_t {
  AEC = 0,  // Acoustic Echo Cancellation
  NS,       // Noise Suppression
  VAD,      // Voice Activity Detection
  AGC,      // Automatic Gain Control
  SE,       // Speech Enhancement (requires 2+ mics)
};

/// How a feature can be controlled at runtime.
enum class FeatureControl : uint8_t {
  NOT_SUPPORTED,     // feature not available in this processor
  BOOT_ONLY,         // set at boot, cannot change at runtime
  RESTART_REQUIRED,  // toggle requires processor reinit (~70ms gap)
  LIVE_TOGGLE,       // toggle is immediate, no audio gap
};

/// Describes the frame layout expected by process().
struct FrameSpec {
  uint32_t sample_rate{16000};
  uint8_t mic_channels{1};     // number of mic channels in input
  uint8_t ref_channels{1};     // number of reference channels (0 or 1)
  size_t input_samples{512};   // per-channel samples expected by process()
  size_t output_samples{512};  // per-channel samples produced by process()
};

/// Snapshot of processor telemetry (thread-safe reads via atomics in impl).
struct ProcessorTelemetry {
  bool voice_present{false};
  float input_volume_dbfs{-120.0f};
  float output_rms_dbfs{-120.0f};
  float ringbuf_free_pct{1.0f};
  uint32_t glitch_count{0};
  uint32_t frame_count{0};
  uint32_t input_ring_drop{0};
  uint32_t feed_ok{0};
  uint32_t feed_rejected{0};
  uint32_t fetch_ok{0};
  uint32_t fetch_timeout{0};
  uint32_t output_ring_drop{0};
  uint32_t feed_queue_frames{0};
  uint32_t feed_queue_peak{0};
  uint32_t fetch_queue_frames{0};
  uint32_t fetch_queue_peak{0};
  uint32_t process_us_last{0};
  uint32_t process_us_max{0};
  uint32_t feed_us_last{0};
  uint32_t feed_us_max{0};
  uint32_t fetch_us_last{0};
  uint32_t fetch_us_max{0};
};

/// Abstract audio processor interface.
///
/// Implementations: EspAec (standalone AEC), EspAfe (full AFE pipeline).
/// Consumers: esp_audio_stack and other audio components.
class AudioProcessor {
 public:
  virtual ~AudioProcessor() = default;

  virtual bool is_initialized() const = 0;

  /// Frame layout this processor expects and produces.
  virtual FrameSpec frame_spec() const = 0;

  /// Process one frame.
  /// @param in_mic  mic input buffer. Layout depends on mic_channels_in:
  ///                1 = mono (input_samples contiguous samples)
  ///                2 = interleaved stereo [mic1_0, mic2_0, mic1_1, mic2_1, ...]
  /// @param in_ref  reference input (speaker loopback), input_samples. May be nullptr.
  /// @param out     output buffer, output_samples
  /// @param mic_channels_in  how many mic channels are interleaved in in_mic
  /// @return true if processed by DSP, false if not processed (implementation
  ///         may emit silence rather than raw audio)
  virtual bool process(const int16_t *in_mic, const int16_t *in_ref, int16_t *out, uint8_t mic_channels_in) = 0;

  /// Query how a feature can be controlled.
  virtual FeatureControl feature_control(AudioFeature feature) const = 0;

  /// Toggle a feature on or off. Check feature_control() first.
  /// @return true if the change was applied successfully
  virtual bool set_feature(AudioFeature feature, bool enabled) = 0;

  /// Current telemetry snapshot.
  virtual ProcessorTelemetry telemetry() const = 0;

  /// Reconfigure the processor (e.g., switch SR/VC mode). Requires audio stop.
  /// @return true if reconfiguration succeeded
  virtual bool reconfigure(int type, int mode) = 0;

  /// Monotonic revision counter for frame_spec changes.
  /// Consumers cache this at init and poll in the audio loop.
  /// When it changes, the consumer must restart to re-read frame_spec().
  virtual uint32_t frame_spec_revision() const { return 0; }

  /// Hint that downstream consumers want frames or have all gone idle.
  /// Implementations may use this to suspend background tasks (e.g. esp-sr
  /// feed/fetch) when no consumer reads the output, then resume when a new
  /// consumer attaches. Must be idempotent and safe to call from any task.
  /// Default no-op: processors that always run regardless of consumers
  /// keep working unchanged.
  virtual void set_processing_active(bool active) { (void) active; }

  /// True when the processor explicitly needs mic frames even without an
  /// external microphone consumer. Example: AFE VAD may be configured as
  /// continuous background input, but a VAD toggle alone must not imply boot
  /// mic ownership. Default false preserves zero-cost idle behavior.
  virtual bool wants_background_input() const { return false; }
};

}  // namespace esphome::esp_audio_stack

#endif  // ESPHOME_AUDIO_CORE_PROCESSOR_H
