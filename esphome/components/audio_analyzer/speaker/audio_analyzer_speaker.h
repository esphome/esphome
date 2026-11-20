#pragma once

#include "esphome/components/audio_analyzer/audio_analyzer.h"
#include "esphome/components/audio_analyzer/pass_through.h"
#include "esphome/components/speaker/speaker.h"

#include <atomic>

namespace esphome::audio_analyzer {

/// Pass-through speaker that analyzes the audio flowing to its output speaker.
///
/// Every call is forwarded to the output at once. ``start()`` and ``stop()`` post a command; ``loop()`` alone moves the
/// state, following the output's (a restart waits for the output to pass through STOPPED). ``play()`` copies the bytes
/// the output accepted into the analysis buffer without blocking.
///
/// Like ``router``, it counts the frames written to the output and not yet reported as played, and only analyzes the
/// older samples, so the values follow what is heard. The analysis is taken before the output's volume; a muted output
/// analyzes silence.
class AudioAnalyzerSpeaker final : public AudioAnalyzer, public speaker::Speaker {
 public:
  AudioAnalyzerSpeaker(speaker::Speaker *output_speaker, uint32_t sample_rate)
      : output_speaker_(output_speaker), sample_rate_(sample_rate) {}

  void dump_config() override;
  void setup() override;
  void loop() override;

#ifdef USE_ESP32
  size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) override;
#endif
  size_t play(const uint8_t *data, size_t length) override;

  void start() override;
  void stop() override;
  void finish() override;

  void set_pause_state(bool pause_state) override { this->output_speaker_->set_pause_state(pause_state); }
  bool get_pause_state() const override { return this->output_speaker_->get_pause_state(); }

  bool has_buffered_data() const override { return this->output_speaker_->has_buffered_data(); }

  void set_mute_state(bool mute_state) override { this->output_speaker_->set_mute_state(mute_state); }
  bool get_mute_state() override { return this->output_speaker_->get_mute_state(); }

  void set_volume(float volume) override { this->output_speaker_->set_volume(volume); }
  float get_volume() override { return this->output_speaker_->get_volume(); }

  // Stop or resume copying the audio; it keeps passing through
  void start_analysis() override;
  void stop_analysis() override;

 protected:
  // Producer side, around the output's play()
  void prepare_play_();
  void forward_accepted_(const uint8_t *data, size_t bytes_written);
  /// True for the first ``start()`` of a new stream.
  bool claim_new_stream_();

  void apply_commands_(uint32_t now);
  void follow_output_(uint32_t now);
  void enter_state_(speaker::State state, uint32_t now);
  /// Warns once when the output never reports the audio it played.
  void check_output_reports_(uint32_t now);
  bool output_silent_() {
    return this->output_speaker_->get_mute_state() ||
           this->output_speaker_->get_volume() < speaker::SILENT_VOLUME_THRESHOLD;
  }

  speaker::Speaker *output_speaker_;
  const uint32_t sample_rate_;

  /// Frames written to the output and not yet reported as played. Decremented (clamped at 0) from the output's task.
  std::atomic<uint32_t> frames_in_output_{0};
  /// Rate of a stream that wasn't analyzed because it differs from ``sample_rate_``, for loop() to log
  std::atomic<uint32_t> unexpected_rate_{0};
  uint32_t state_since_ms_{0};
  /// Since when the output has run unpaused with audio in it, for the played-audio report check
  uint32_t reports_since_ms_{0};
  FrameAssembler frame_assembler_;  // producer side

  /// Commands posted by start() and stop() from any task for loop()
  std::atomic<uint8_t> commands_{0};
  /// Set by a new stream: the producer drops the partial frame it kept from the previous one
  std::atomic<bool> new_stream_{false};
  std::atomic<bool> analysis_enabled_{true};
  /// Set once the output reported played audio
  std::atomic<bool> output_reported_{false};
  // loop() side
  bool output_seen_starting_{false};
  /// STARTING after a stop: the output hasn't been seen STOPPED since
  bool awaiting_output_stop_{false};
  bool warned_no_start_{false};
  bool warned_no_reports_{false};
  bool warned_unexpected_rate_{false};
};

}  // namespace esphome::audio_analyzer
