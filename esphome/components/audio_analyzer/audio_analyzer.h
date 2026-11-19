#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio.h"
#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#include <array>
#include <memory>

namespace esphome::audio_analyzer {

/// Number of log-spaced frequency bands.
static constexpr uint8_t NUM_BANDS = 12;

/// @brief Live power and spectrum of an audio stream, for visualizers, level meters and light effects.
///
/// Like ``sound_level``, audio arrives on another task and is only copied into a ring buffer there. All the analysis
/// runs in ``loop()`` on the main task, so the getters can be called from lambdas (light effects, templates) without
/// locking.
///
/// The audio is decimated to about 16 kHz and analyzed in frames of 512 samples (about 32 ms, about 31 updates per
/// second). Each frame gives:
///   - power: mean square of the frame in dBFS (same scale as ``sound_level``'s RMS sensor)
///   - 12 bands: Hann-windowed FFT, bins summed into log-spaced bands between the minimum and maximum frequency, in
///     dBFS and mapped linearly from the band range to 0..1
/// All outputs are smoothed with the attack and release time constants.
class AudioAnalyzer : public PollingComponent {
#ifdef USE_SENSOR
  SUB_SENSOR(power)
#endif

 public:
  void update() override;

  /// Smoothed power in dBFS, -100 at most quiet.
  float get_power_db() const { return this->power_db_; }
  /// Smoothed power mapped from the power range to 0..1.
  float get_power() const { return this->power_level_; }
  /// Smoothed level of a band, 0..1. Band 0 is the lowest frequency.
  float get_band(uint8_t band) const { return band < NUM_BANDS ? this->band_level_[band] : 0.0f; }
  /// Smoothed level of a band in dBFS, -100 at most quiet.
  float get_band_db(uint8_t band) const { return band < NUM_BANDS ? this->band_db_[band] : -100.0f; }
  static constexpr uint8_t get_num_bands() { return NUM_BANDS; }
  /// True while audio is being analyzed.
  bool is_active() const { return this->active_; }

  void set_frequency_range(float min_hz, float max_hz) {
    this->min_frequency_ = min_hz;
    this->max_frequency_ = max_hz;
  }
  void set_power_range(float min_db, float max_db) {
    this->power_min_db_ = min_db;
    this->power_max_db_ = max_db;
  }
  void set_band_range(float min_db, float max_db) {
    this->band_min_db_ = min_db;
    this->band_max_db_ = max_db;
  }
  void set_attack(uint32_t attack_ms) { this->attack_ms_ = attack_ms; }
  void set_release(uint32_t release_ms) { this->release_ms_ = release_ms; }
#ifdef USE_SENSOR
  void set_band_sensor(uint8_t band, sensor::Sensor *sensor) { this->band_sensors_[band] = sensor; }
#endif

 protected:
  void dump_analysis_config_();

  /// @brief Allocates the FFT buffers and tables. Returns false if the allocation failed.
  bool allocate_fft_();

  /// @brief Sets up decimation and band edges for a stream format. Drops the partial frame if the format changed.
  void configure_stream_(const audio::AudioStreamInfo &stream_info);

  /// @brief Analyzes interleaved PCM frames, using channel 0 only. Runs the FFT every 512 decimated samples.
  void process_frames_(const uint8_t *data, uint32_t frames);

  /// @brief Runs the FFT on the filled frame and updates the smoothed outputs.
  void analyze_frame_();

  /// @brief Releases the outputs towards silence while no audio arrives. Call it from ``loop()``.
  void decay_if_idle_();

  /// @brief Applies one smoothing step towards the given targets.
  void apply_targets_(float power_db, const float *band_db, float frame_seconds);

  /// @brief Drops the partial analysis frame (stream stopped or format changed).
  void reset_frame_();

  // FFT buffers, one allocation
  float *fft_re_{nullptr};
  float *fft_im_{nullptr};
  float *window_{nullptr};
  float *twiddle_cos_{nullptr};
  float *twiddle_sin_{nullptr};

#ifdef USE_SENSOR
  std::array<sensor::Sensor *, NUM_BANDS> band_sensors_{};
  // Highest values since the last update(), so short sounds still show at a low sensor rate
  std::array<float, NUM_BANDS> band_level_hold_{};
  float power_db_hold_{-100.0f};
#endif

  std::array<float, NUM_BANDS> band_db_{};
  std::array<float, NUM_BANDS> band_level_{};
  std::array<uint16_t, NUM_BANDS + 1> band_bins_{};

  audio::AudioStreamInfo analysis_stream_info_;

  float effective_sample_rate_{16000.0f};
  float window_power_sum_{0.0f};
  float decimation_sum_{0.0f};
  float squared_sum_{0.0f};
  float power_db_{-100.0f};
  float power_level_{0.0f};

  float min_frequency_{100.0f};
  float max_frequency_{4000.0f};
  float power_min_db_{-70.0f};
  float power_max_db_{-20.0f};
  float band_min_db_{-80.0f};
  float band_max_db_{-30.0f};
  uint32_t attack_ms_{30};
  uint32_t release_ms_{250};

  uint32_t squared_count_{0};
  uint32_t last_frame_ms_{0};
  uint16_t frame_fill_{0};
  uint8_t decimation_{1};
  uint8_t decimation_count_{0};
  bool stream_configured_{false};
  bool active_{false};
};

/// @brief Analyzer fed by a microphone, hooked up like ``sound_level``: the microphone's data callback copies the
/// audio into a ring buffer and ``loop()`` analyzes it.
class MicrophoneAudioAnalyzer final : public AudioAnalyzer {
 public:
  explicit MicrophoneAudioAnalyzer(microphone::MicrophoneSource *microphone_source)
      : microphone_source_(microphone_source) {}

  void dump_config() override;
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

 protected:
  bool start_();
  void stop_();

  microphone::MicrophoneSource *microphone_source_;
  std::unique_ptr<audio::RingBufferAudioSource> audio_source_;
  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;
};

}  // namespace esphome::audio_analyzer

#endif  // USE_ESP32
