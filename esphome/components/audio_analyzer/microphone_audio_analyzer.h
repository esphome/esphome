#pragma once

#include "esphome/core/defines.h"

#ifdef USE_MICROPHONE

#include "audio_analyzer.h"

#include "esphome/components/microphone/microphone_source.h"

#include <atomic>

namespace esphome::audio_analyzer {

/// Analyzes a microphone, hooked up like ``sound_level``. Each run of the microphone starts from fresh audio; the loop
/// sleeps once the microphone stopped and the values released.
class MicrophoneAudioAnalyzer final : public AudioAnalyzer {
 public:
  MicrophoneAudioAnalyzer(microphone::MicrophoneSource *microphone_source, uint32_t sample_rate)
      : microphone_source_(microphone_source), stream_info_(16, 1, sample_rate) {}

  void dump_config() override;
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  // Start and stop the microphone, unless passive
  void start_analysis() override;
  void stop_analysis() override;

 protected:
  /// THREAD CONTEXT: the microphone task.
  void on_capture_(const microphone::AudioCapture &capture);
  bool allows_control_() const;

  microphone::MicrophoneSource *microphone_source_;
  const audio::AudioStreamInfo stream_info_;
  /// Set by the microphone task, logged by loop()
  std::atomic<bool> unexpected_format_{false};
  bool running_{false};
  bool warned_unexpected_format_{false};
};

}  // namespace esphome::audio_analyzer

#endif  // USE_MICROPHONE
