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

namespace esphome::speaker {

/// Volumes below this are treated as zero
static constexpr float SILENT_VOLUME_THRESHOLD = 0.001f;

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
  /// @brief Start an output before the media reader starts, if it needs a connection.
  virtual void prepare_for_media() { this->start(); }
  virtual void stop() = 0;
  /// @brief Whether a prepared output can receive media without a new connection.
  virtual bool is_ready_for_media() const { return true; }
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

  // Volume and mute are independent: changing one never alters the other's stored state. Volume control is
  // handled by the configured audio dac components, every one of them getting the same volume and mute. Individual
  // speaker components can override and implement in software if an audio dac isn't available.
  virtual void set_volume(float volume) {
    this->volume_ = volume;
#ifdef USE_AUDIO_DAC
    for (auto *audio_dac : this->audio_dacs_) {
      audio_dac->set_volume(volume);
    }
    this->apply_audio_dac_mute_();
#endif
  };
  virtual float get_volume() { return this->volume_; }

  virtual void set_mute_state(bool mute_state) {
    this->mute_state_ = mute_state;
#ifdef USE_AUDIO_DAC
    this->apply_audio_dac_mute_();
#endif
  }
  virtual bool get_mute_state() { return this->mute_state_; }

#ifdef USE_AUDIO_DAC
  /// Sets a single audio dac (nullptr for none), replacing any list; keeps its own copy of the pointer.
  void set_audio_dac(audio_dac::AudioDac *audio_dac) {
    this->audio_dacs_.assign_copy(&audio_dac, audio_dac != nullptr ? 1 : 0);
  }
  /// Codegen only: the table must outlive this speaker. Every DAC listed is fed by this speaker's audio output.
  void set_audio_dacs(audio_dac::AudioDac *const *audio_dacs, size_t count) {
    this->audio_dacs_.assign_static(audio_dacs, count);
  }
#endif

  void set_audio_stream_info(const audio::AudioStreamInfo &audio_stream_info) {
    this->audio_stream_info_ = audio_stream_info;
  }

  audio::AudioStreamInfo &get_audio_stream_info() { return this->audio_stream_info_; }

  /// Callback function for sending the duration of the audio written to the speaker since the last callback.
  /// Parameters:
  ///   - Frames played
  ///   - System time in microseconds when the frames were written to the DAC
  template<typename F> void add_audio_output_callback(F &&callback) {
    this->audio_output_callback_.add(std::forward<F>(callback));
  }

 protected:
  /// @brief Whether the output should be silent: muted, or the volume is effectively zero.
  /// Volume steps from media players can leave a positive value near float epsilon instead of exactly zero.
  bool is_silent_() const { return this->mute_state_ || this->volume_ < SILENT_VOLUME_THRESHOLD; }

#ifdef USE_AUDIO_DAC
  /// @brief Whether volume and mute are handled in hardware by at least one audio dac.
  bool has_audio_dac_() const { return !this->audio_dacs_.empty(); }

  /// @brief Uses the audio dacs' mute as the silence mechanism, since a dac's minimum volume is often audible.
  void apply_audio_dac_mute_() {
    const bool silent = this->is_silent_();
    for (auto *audio_dac : this->audio_dacs_) {
      if (silent) {
        audio_dac->set_mute_on();
      } else {
        audio_dac->set_mute_off();
      }
    }
  }

  /// @brief Tells every audio dac that its audio clocks are running; call from the main loop.
  void notify_audio_dacs_started_() {
    for (auto *audio_dac : this->audio_dacs_) {
      audio_dac->on_audio_started();
    }
  }
#endif

  State state_{STATE_STOPPED};
  bool mute_state_{false};
  audio::AudioStreamInfo audio_stream_info_;
  float volume_{1.0f};

#ifdef USE_AUDIO_DAC
  // all audio dacs fed by this speaker's audio output (eg two amplifiers on one I2S bus); empty without an audio dac
  ConstVector<audio_dac::AudioDac *, true> audio_dacs_;
#endif

  CallbackManager<void(uint32_t, int64_t)> audio_output_callback_{};
};

}  // namespace esphome::speaker
