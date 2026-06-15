#pragma once

#ifdef USE_ESP32

#include "esphome/components/audio/audio_transfer_buffer.h"
#include "esphome/components/microphone/microphone_source.h"
#include "esphome/components/ring_buffer/ring_buffer.h"
#include "esphome/components/sensor/sensor.h"

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#ifdef USE_ESP32
#include "esp_dsp.h"
#endif

namespace esphome::sound_frequency {

class SoundFrequencyComponent : public Component {
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
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_sample_rate(uint32_t sample_rate) { this->sample_rate_ = sample_rate; }

  /// @brief Starts the MicrophoneSource to start measuring sound levels
  void start();

  /// @brief Stops the MicrophoneSource
  void stop();

 protected:
  /// @brief Internal start command that, if necessary, allocates a ring buffer and a zero-copy
  /// ``RingBufferAudioSource`` that reads directly from it. ``ring_buffer_`` weakly references the
  /// ring buffer owned by ``audio_source_``. Returns true if allocations were successful.
  bool start_();

  /// @brief Internal stop command that deallocates ``audio_source_`` (which releases its ring buffer)
  void stop_();

  microphone::MicrophoneSource *microphone_source_{nullptr};

  sensor::Sensor *frequency_sensor_{nullptr};

  std::unique_ptr<audio::RingBufferAudioSource> audio_source_;
  std::weak_ptr<ring_buffer::RingBuffer> ring_buffer_;

  std::vector<int16_t> samples_buffer_;
  uint32_t sample_count_{0};

  uint32_t measurement_duration_ms_{0};
  uint32_t sample_rate_{16000};

  std::vector<float> fft_input_;
  std::vector<float> fft_output_;
  std::vector<float> window_;
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<SoundFrequencyComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<SoundFrequencyComponent> {
 public:
  void play(const Ts &...x) override { this->parent_->stop(); }
};

}  // namespace esphome::sound_frequency

#endif
