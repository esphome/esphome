#pragma once

#include "esphome/core/defines.h"

#if defined(USE_ESP32) && defined(USE_MICROPHONE)

#include "audio_analyzer.h"

#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"

#include <atomic>
#include <memory>

namespace esphome::audio_analyzer {

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

  /// @brief Starts the microphone. Not allowed in passive mode.
  void start();
  /// @brief Stops the microphone. Not allowed in passive mode.
  void stop();

 protected:
  microphone::MicrophoneSource *microphone_source_;
  // Allocated once in setup(), before the data callback is registered, and never replaced
  std::shared_ptr<ring_buffer::RingBuffer> ring_buffer_;
  std::unique_ptr<audio::RingBufferAudioSource> audio_source_;
  Decimator decimator_;
  /// Set by the microphone task when the ring buffer was full and the oldest audio was overwritten
  std::atomic<bool> overrun_{false};
  bool running_{false};
  bool overrun_logged_{false};
};

}  // namespace esphome::audio_analyzer

#endif
