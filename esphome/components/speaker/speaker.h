#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

#include "esphome/components/audio/audio.h"

namespace esphome {
namespace speaker {

enum State : uint8_t {
  STATE_STOPPED = 0,
  STATE_STARTING,
  STATE_RUNNING,
  STATE_STOPPING,
};

class Speaker {
 public:
#ifdef USE_ESP32
  /// @brief Plays the provided audio data.
  /// If the speaker component doesn't implement this method, it falls back to the play method without this parameter.
  /// @param data Audio data in the format specified by ``set_audio_stream_info`` method.
  /// @param length The length of the audio data in bytes.
  /// @param ticks_to_wait The FreeRTOS ticks to wait before writing as much data as possible to the ring buffer.
  /// @return The number of bytes that were actually written to the speaker's internal buffer.
  virtual size_t play(const uint8_t *data, size_t length, TickType_t ticks_to_wait) {
    return this->play(data, length);
  };
#endif

  /// @brief Plays the provided audio data.
  /// If the audio stream is not the default defined in "esphome/core/audio.h" and the speaker component implements it,
  /// then this should be called after calling ``set_audio_stream_info``.
  /// @param data Audio data in the format specified by ``set_audio_stream_info`` method.
  /// @param length The length of the audio data in bytes.
  /// @return The number of bytes that were actually written to the speaker's internal buffer.
  virtual size_t play(const uint8_t *data, size_t length) = 0;

  size_t play(const std::vector<uint8_t> &data) { return this->play(data.data(), data.size()); }

  virtual void start() = 0;
  virtual void stop() = 0;
  // In compare between *STOP()* and *FINISH()*; *FINISH()* will stop after emptying the play buffer,
  // while *STOP()* will break directly.
  // When finish() is not implemented on the platform component it should just do a normal stop.
  virtual void finish() { this->stop(); }

  virtual bool has_buffered_data() const = 0;

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  // Volume control must be implemented by each speaker component, otherwise it will have no effect.
  virtual void set_volume(float volume) { this->volume_ = volume; };
  virtual float get_volume() { return this->volume_; }

  void set_audio_stream_info(const audio::AudioStreamInfo &audio_stream_info) {
    this->audio_stream_info_ = audio_stream_info;
  }

 protected:
  State state_{STATE_STOPPED};
  audio::AudioStreamInfo audio_stream_info_;
  float volume_{1.0f};
};

}  // namespace speaker
}  // namespace esphome
