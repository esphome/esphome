#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "esphome/components/speaker/speaker.h"

#include <atomic>

namespace esphome::audio_analyzer {

/// @brief Pass-through speaker that analyzes the audio flowing through it.
///
/// Placed between an audio source (for example a media player pipeline) and its real speaker. Every call is forwarded
/// unchanged to the output speaker; ``play()`` also copies the bytes the output accepted into a ring buffer, without
/// blocking.
///
/// Audio written to a speaker runs ahead of what is heard (resampler, mixer and I2S buffers, often a few hundred ms).
/// To keep the analysis in step with the sound, ``loop()`` only analyzes as many frames as the output speaker reports
/// as played through its audio output callback.
class AudioAnalyzerSpeaker final : public AudioAnalyzer, public speaker::Speaker {
 public:
  explicit AudioAnalyzerSpeaker(speaker::Speaker *output_speaker) : output_speaker_(output_speaker) {}

  void dump_config() override;
  void setup() override;
  void loop() override;

  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
  size_t play(const uint8_t *data, size_t length) override { return this->play(data, length, 0); }

  void start() override;
  void stop() override { this->output_speaker_->stop(); }
  void finish() override { this->output_speaker_->finish(); }

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
  /// @brief Forwards the stream format to the output speaker.
  void sync_stream_info_();

  /// @brief Allocates the ring buffer and its reader, again if the stream format changed. Returns false on failure.
  bool start_();
  void stop_();

  speaker::Speaker *output_speaker_;

  std::unique_ptr<audio::RingBufferAudioSource> audio_source_;
  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;
  audio::AudioStreamInfo allocated_stream_info_;

  /// Frames the output speaker reported as played but not yet analyzed. Written from the output speaker's task.
  std::atomic<uint32_t> played_frames_{0};

  uint32_t buffer_duration_ms_{1000};
};

}  // namespace esphome::audio_analyzer

#endif  // USE_ESP32
