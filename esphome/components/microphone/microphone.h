#pragma once

#include "esphome/components/audio/audio.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>
#include "esphome/core/helpers.h"

namespace esphome::microphone {

enum State : uint8_t {
  STATE_STOPPED = 0,
  STATE_STARTING,
  STATE_RUNNING,
  STATE_STOPPING,
};

/// @brief Adapts a data callback to the (data, capture_time_us) form.
///
/// A callback that already takes the capture time is returned as is; one that only takes the data is wrapped so the
/// capture time is dropped.
template<typename F> auto with_capture_time(F &&callback) {
  using Fn = std::decay_t<F>;
  if constexpr (std::is_invocable_v<const Fn &, const std::vector<uint8_t> &, int64_t>) {
    return Fn(std::forward<F>(callback));
  } else {
    static_assert(std::is_invocable_v<const Fn &, const std::vector<uint8_t> &>,
                  "A data callback takes (const std::vector<uint8_t> &) or (const std::vector<uint8_t> &, int64_t)");
    return [callback = Fn(std::forward<F>(callback))](const std::vector<uint8_t> &data, int64_t) { callback(data); };
  }
}

class Microphone {
 public:
  virtual void start() = 0;
  virtual void stop() = 0;

  /// @brief Registers a callback for captured audio.
  ///
  /// The callback takes either (const std::vector<uint8_t> &data) or (const std::vector<uint8_t> &data,
  /// int64_t capture_time_us). capture_time_us is when the first frame in data was captured, in esp_timer_get_time()
  /// microseconds, or 0 if unknown. Callbacks run on the microphone's own task.
  template<typename F> void add_data_callback(F &&data_callback) {
    this->data_callbacks_.add([this, callback = with_capture_time(std::forward<F>(data_callback))](
                                  const std::vector<uint8_t> &data, int64_t capture_time_us) {
      if (this->mute_state_) {
        callback(std::vector<uint8_t>(data.size(), 0), capture_time_us);
      } else {
        callback(data, capture_time_us);
      }
    });
  }

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  void set_mute_state(bool is_muted) { this->mute_state_ = is_muted; }
  bool get_mute_state() { return this->mute_state_; }

  audio::AudioStreamInfo get_audio_stream_info() { return this->audio_stream_info_; }

 protected:
  /// @brief Passes captured audio to every registered callback.
  /// @param data Captured audio in the microphone's format
  /// @param capture_time_us When the first frame in data was captured, in esp_timer_get_time() microseconds, or 0 if
  ///        unknown
  void publish_audio_(const std::vector<uint8_t> &data, int64_t capture_time_us = 0) {
    this->data_callbacks_.call(data, capture_time_us);
  }

  bool has_data_callbacks_() const { return this->data_callbacks_.size() > 0; }

  State state_{STATE_STOPPED};
  bool mute_state_{false};

  audio::AudioStreamInfo audio_stream_info_;

 private:
  CallbackManager<void(const std::vector<uint8_t> &, int64_t)> data_callbacks_{};
};

}  // namespace esphome::microphone
