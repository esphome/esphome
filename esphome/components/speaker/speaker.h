#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#endif

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#include "esphome/components/audio/audio.h"
#ifdef USE_AUDIO_DAC
#include "esphome/components/audio_dac/audio_dac.h"
#endif

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

  // Pauses processing incoming audio. Needs to be implemented specifically per speaker component
  virtual void set_pause_state(bool pause_state) {}
  virtual bool get_pause_state() const { return false; }

  virtual bool has_buffered_data() const = 0;

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  // Volume control is handled by a configured audio dac component. Individual speaker components can
  // override and implement in software if an audio dac isn't available.
  virtual void set_volume(float volume) {
    this->volume_ = volume;
#ifdef USE_AUDIO_DAC
    if (this->audio_dac_ != nullptr) {
      this->audio_dac_->set_volume(volume);
    }
#endif
  };
  float get_volume() { return this->volume_; }

  virtual void set_mute_state(bool mute_state) {
    this->mute_state_ = mute_state;
#ifdef USE_AUDIO_DAC
    if (this->audio_dac_) {
      if (mute_state) {
        this->audio_dac_->set_mute_on();
      } else {
        this->audio_dac_->set_mute_off();
      }
    }
#endif
  }
  bool get_mute_state() { return this->mute_state_; }

#ifdef USE_AUDIO_DAC
  void set_audio_dac(audio_dac::AudioDac *audio_dac) { this->audio_dac_ = audio_dac; }
#endif

  void set_audio_stream_info(const audio::AudioStreamInfo &audio_stream_info) {
    this->audio_stream_info_ = audio_stream_info;
  }

  audio::AudioStreamInfo &get_audio_stream_info() { return this->audio_stream_info_; }

  /// Callback function for sending the duration of the audio written to the speaker since the last callback.
  /// Parameters:
  ///   - Duration in milliseconds. Never rounded and should always be less than or equal to the actual duration.
  ///   - Remainder duration in microseconds. Rounded duration after subtracting the previous parameter from the actual
  ///     duration.
  ///   - Duration of remaining, unwritten audio buffered in the speaker in milliseconds.
  ///   - System time in microseconds when the last write was completed.
  void add_audio_output_callback(std::function<void(uint32_t, uint32_t, uint32_t, uint32_t)> &&callback) {
    this->audio_output_callback_.add(std::move(callback));
  }

 protected:
  State state_{STATE_STOPPED};
  audio::AudioStreamInfo audio_stream_info_;
  float volume_{1.0f};
  bool mute_state_{false};

#ifdef USE_AUDIO_DAC
  audio_dac::AudioDac *audio_dac_{nullptr};
#endif

  CallbackManager<void(uint32_t, uint32_t, uint32_t, uint32_t)> audio_output_callback_{};
};

}  // namespace speaker
}  // namespace esphome
