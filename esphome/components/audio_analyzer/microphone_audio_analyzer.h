#pragma once

#include "audio_analyzer.h"

#include "esphome/core/defines.h"

#ifdef USE_MICROPHONE
#include "esphome/components/microphone/microphone_source.h"
#endif

namespace esphome::audio_analyzer {

/// @brief What the microphone analyzer decides, without the microphone itself, so it runs in the host unit tests.
///
/// The microphone's data callback copies the 16-bit mono audio into the tap (``on_audio_()``) and wakes ``loop()``,
/// which analyzes everything buffered (``run_()``). A new run of the microphone starts from fresh audio. Once the
/// microphone stopped and the values released to silence, the loop sleeps until the next audio arrives.
class MicrophoneAnalyzerBase : public AudioAnalyzer {
 protected:
  /// @brief Copies 16-bit mono audio and wakes ``loop()``. THREAD CONTEXT: the microphone task.
  void on_audio_(const uint8_t *data, size_t length) {
    this->tap_.write(data, length / sizeof(int16_t), sizeof(int16_t), 1);
    this->enable_loop_soon_any_context();
  }
  /// @brief One ``loop()`` pass, given whether the microphone runs.
  void run_(bool microphone_running);
  /// @brief Whether ``start_analysis()`` / ``stop_analysis()`` may control the microphone: not in passive mode.
  bool allows_control_(bool passive) const;

  bool running_{false};
};

#ifdef USE_MICROPHONE
/// @brief Analyzer fed by a microphone, hooked up like ``sound_level``.
class MicrophoneAudioAnalyzer final : public MicrophoneAnalyzerBase {
 public:
  explicit MicrophoneAudioAnalyzer(microphone::MicrophoneSource *microphone_source)
      : microphone_source_(microphone_source) {}

  void dump_config() override;
  void setup() override;
  void loop() override { this->run_(this->microphone_source_->is_running()); }
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  /// @brief Starts the microphone. Not allowed in passive mode.
  void start_analysis() {
    if (this->allows_control_(this->microphone_source_->is_passive())) {
      this->microphone_source_->start();
    }
  }
  /// @brief Stops the microphone. Not allowed in passive mode.
  void stop_analysis() {
    if (this->allows_control_(this->microphone_source_->is_passive())) {
      this->microphone_source_->stop();
    }
  }

 protected:
  microphone::MicrophoneSource *microphone_source_;
};
#endif  // USE_MICROPHONE

}  // namespace esphome::audio_analyzer
