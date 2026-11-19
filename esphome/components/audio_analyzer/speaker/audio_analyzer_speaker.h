#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/speaker/speaker.h"

#include <atomic>
#include <memory>

namespace esphome::audio_analyzer {

/// @brief Pass-through speaker that analyzes the audio flowing through it.
///
/// Placed between an audio source (for example a media player pipeline) and its real speaker. Every call is forwarded
/// to the output speaker. ``play()`` also downmixes the bytes the output accepted to mono, decimates them to at most
/// 16 kHz and copies them as 16-bit samples into a buffer allocated once in ``setup()``, without blocking.
///
/// Audio written to a speaker runs ahead of what is heard (resampler, mixer and I2S buffers, often a few hundred ms).
/// The analyzer counts the frames written to the output and not yet reported as played through its audio output
/// callback, like ``router``. ``loop()`` only analyzes the buffered samples older than those frames, so the values
/// follow what is heard.
///
/// The analysis is taken before the output's volume. While this speaker is muted (or at zero volume) it analyzes
/// silence.
class AudioAnalyzerSpeaker final : public AudioAnalyzer, public speaker::Speaker {
 public:
  explicit AudioAnalyzerSpeaker(speaker::Speaker *output_speaker) : output_speaker_(output_speaker) {}

  void dump_config() override;
  void setup() override;
  void loop() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->output_speaker_->set_pause_state(pause_state); }
  bool get_pause_state() const override { return this->output_speaker_->get_pause_state(); }

  bool has_buffered_data() const override { return this->output_speaker_->has_buffered_data(); }

  void set_mute_state(bool mute_state) override {
    this->mute_state_ = mute_state;
    this->output_speaker_->set_mute_state(mute_state);
  }
  bool get_mute_state() override { return this->output_speaker_->get_mute_state(); }

  void set_volume(float volume) override {
    this->volume_ = volume;
    this->output_speaker_->set_volume(volume);
  }
  float get_volume() override { return this->output_speaker_->get_volume(); }

  void set_buffer_duration(uint32_t buffer_duration_ms) { this->buffer_duration_ms_ = buffer_duration_ms; }

 protected:
  /// Hand-over of the analysis buffer between the task that calls ``play()`` (the producer) and ``loop()``.
  enum class SyncState : uint8_t {
    /// Producer asked for a fresh start (new stream, format change, full buffer) and stopped copying
    SYNC_STATE_RESYNC,
    /// loop() emptied the buffer; the producer starts copying with the current format on its next play()
    SYNC_STATE_DRAINED,
    /// Producer copies every frame it forwards
    SYNC_STATE_COPYING,
  };

  /// @brief Forwards the stream format to the output speaker.
  void sync_stream_info_();

  /// @brief Producer side: decimates and copies forwarded frames into the analysis buffer.
  void copy_frames_(const uint8_t *data, uint32_t frames);
  /// @brief Producer side: writes decimated samples, or asks for a resync when they don't fit.
  bool write_samples_(const int16_t *samples, size_t count);

  /// @brief Mirrors the output speaker's state, which callers read through ``is_stopped()`` and ``is_running()``.
  void update_state_();
  /// @brief Analyzes the buffered samples that have been played.
  void analyze_heard_();

  speaker::Speaker *output_speaker_;

  // Decimated mono 16-bit samples, written by the producer and read by loop(). Allocated once in setup().
  std::unique_ptr<ring_buffer::RingBuffer> ring_buffer_;

  // Producer side (THREAD CONTEXT: the task that calls play())
  Decimator decimator_;
  audio::AudioStreamInfo copy_stream_info_;

  /// Frames written to the output and not yet reported as played. Decremented (clamped at 0) from the output's task.
  std::atomic<uint32_t> frames_in_output_{0};
  /// Input sample rate of the samples copied since the last resync; published before SYNC_STATE_COPYING
  std::atomic<uint32_t> copy_sample_rate_{0};
  std::atomic<SyncState> sync_state_{SyncState::SYNC_STATE_RESYNC};
  /// Set by the producer when the buffer was full
  std::atomic<bool> overflow_{false};

  uint32_t buffer_duration_ms_{1000};
  uint32_t state_start_ms_{0};
  // loop() side
  uint32_t analysis_sample_rate_{0};
  uint8_t decimation_{1};
  bool awaiting_copy_{true};
  bool overflow_logged_{false};
};

}  // namespace esphome::audio_analyzer

#endif  // USE_ESP32
