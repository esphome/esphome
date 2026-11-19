#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MICROPHONE

#include "audio_analyzer.h"

#include "esphome/components/microphone/microphone_source.h"

#include <atomic>

namespace esphome::audio_analyzer {

/// @brief Analyzer fed by a microphone, hooked up like ``sound_level``.
///
/// The microphone's data callback copies the 16-bit mono audio into the tap and wakes ``loop()``, which analyzes
/// everything buffered. A new run of the microphone starts from fresh audio. Once the microphone stopped and the
/// values released to silence, the loop sleeps until the next audio arrives.
class MicrophoneAudioAnalyzer final : public AudioAnalyzer {
 public:
  MicrophoneAudioAnalyzer(microphone::MicrophoneSource *microphone_source, uint32_t sample_rate)
      : microphone_source_(microphone_source), stream_info_(16, 1, sample_rate) {}

  void dump_config() override;
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// @brief Starts the microphone. Not allowed in passive mode.
  void start_analysis() override;
  /// @brief Stops the microphone. Not allowed in passive mode.
  void stop_analysis() override;

 protected:
  /// @brief Copies the audio and wakes ``loop()``. THREAD CONTEXT: the microphone task.
  void on_capture_(const microphone::AudioCapture &capture);
  /// @brief Whether ``start_analysis()`` / ``stop_analysis()`` may control the microphone: not in passive mode.
  bool allows_control_() const;

  microphone::MicrophoneSource *microphone_source_;
  /// The format the analysis was worked out for
  const audio::AudioStreamInfo stream_info_;
  /// Set by the microphone task when audio came in another format, for loop() to log
  std::atomic<bool> unexpected_format_{false};
  bool running_{false};
  bool warned_unexpected_format_{false};
};

}  // namespace esphome::audio_analyzer

#endif  // USE_MICROPHONE
