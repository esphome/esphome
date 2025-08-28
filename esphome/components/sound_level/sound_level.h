#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/core/component.h"
#include "esphome/core/ring_buffer.h"

namespace esphome {
namespace sound_level {

class SoundLevelComponent : public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;

  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

  void set_measurement_duration(uint32_t measurement_duration_ms) {
    this->measurement_duration_ms_ = measurement_duration_ms;
  }
  void set_microphone_source(microphone::MicrophoneSource *microphone_source) {
    this->microphone_source_ = microphone_source;
  }
  void set_peak_sensor(sensor::Sensor *peak_sensor) { this->peak_sensor_ = peak_sensor; }
  void set_rms_sensor(sensor::Sensor *rms_sensor) { this->rms_sensor_ = rms_sensor; }

  /// @brief Starts the MicrophoneSource to start measuring sound levels
  void start();

  /// @brief Stops the MicrophoneSource
  void stop();

 protected:
  /// @brief Internal start command that, if necessary, allocates ``audio_buffer_`` and a ring buffer which
  /// ``audio_buffer_`` owns and ``ring_buffer_`` points to. Returns true if allocations were successful.
  bool start_();

  /// @brief Internal stop command the deallocates ``audio_buffer_`` (which automatically deallocates its ring buffer)
  void stop_();

  microphone::MicrophoneSource *microphone_source_{nullptr};

  sensor::Sensor *peak_sensor_{nullptr};
  sensor::Sensor *rms_sensor_{nullptr};

  std::unique_ptr<audio::AudioSourceTransferBuffer> audio_buffer_;
  std::weak_ptr<RingBuffer> ring_buffer_;

  int32_t squared_peak_{0};
  uint64_t squared_samples_sum_{0};
  uint32_t sample_count_{0};

  uint32_t measurement_duration_ms_;
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<SoundLevelComponent> {
 public:
  void play(Ts... x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<SoundLevelComponent> {
 public:
  void play(Ts... x) override { this->parent_->stop(); }
};

}  // namespace sound_level
}  // namespace esphome
#endif
