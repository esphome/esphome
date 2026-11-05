#pragma once

#include "esphome/components/audio/audio.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
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

struct AudioCapture {
  /// Only valid during the callback
  std::span<const uint8_t> data;
  /// Format of data
  audio::AudioStreamInfo stream_info;
  /// esp_timer_get_time() of the first frame, or 0 if unknown
  int64_t capture_time_us{0};
};

// Remove before 2027.5.0. A template, so it only warns for the callbacks that use it.
template<typename F>
ESPDEPRECATED("Take const microphone::AudioCapture & in data callbacks instead. Removed in 2027.5.0", "2026.11.0")
constexpr void warn_vector_data_callback() {}

template<typename F> auto as_capture_callback(F &&callback) {
  using Fn = std::decay_t<F>;
  if constexpr (std::is_invocable_v<const Fn &, const AudioCapture &>) {
    return Fn(std::forward<F>(callback));
  } else {
    // Remove before 2027.5.0
    static_assert(std::is_invocable_v<const Fn &, const std::vector<uint8_t> &>,
                  "A data callback takes (const microphone::AudioCapture &)");
    warn_vector_data_callback<Fn>();
    return [callback = Fn(std::forward<F>(callback))](const AudioCapture &capture) {
      const std::vector<uint8_t> copy(capture.data.begin(), capture.data.end());
      callback(copy);
    };
  }
}

using DataCallbackManagerBase = CallbackManager<void(const AudioCapture &)>;

class DataCallbackManager : public DataCallbackManagerBase {
 public:
  DataCallbackManager(const audio::AudioStreamInfo *stream_info, const bool *mute_state)
      : stream_info_(stream_info), mute_state_(mute_state) {}

  using DataCallbackManagerBase::call;

  // Remove before 2027.5.0, along with this class (use a plain CallbackManager)
  ESPDEPRECATED("Use Microphone::publish_audio_() instead. Removed in 2027.5.0", "2026.11.0")
  void call(const std::vector<uint8_t> &data) {
    if (*this->mute_state_) {
      const std::vector<uint8_t> zeros(data.size(), 0);
      this->call(AudioCapture{zeros, *this->stream_info_, 0});
    } else {
      this->call(AudioCapture{data, *this->stream_info_, 0});
    }
  }

 protected:
  const audio::AudioStreamInfo *stream_info_;
  const bool *mute_state_;
};

class Microphone {
 public:
  virtual void start() = 0;
  virtual void stop() = 0;

  /// Callbacks take (const AudioCapture &) and run on the microphone's task.
  template<typename F> void add_data_callback(F &&data_callback) {
    this->data_callbacks_.add(as_capture_callback(std::forward<F>(data_callback)));
  }

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  void set_mute_state(bool is_muted) { this->mute_state_ = is_muted; }
  bool get_mute_state() { return this->mute_state_; }

  audio::AudioStreamInfo get_audio_stream_info() { return this->audio_stream_info_; }

 protected:
  /// While muted, data is zeroed first, so it must be a buffer the platform owns.
  void publish_audio_(std::span<uint8_t> data, const audio::AudioStreamInfo &stream_info, int64_t capture_time_us = 0) {
    if (this->mute_state_) {
      std::memset(data.data(), 0, data.size());
    }
    this->data_callbacks_.call(AudioCapture{data, stream_info, capture_time_us});
  }

  bool has_data_callbacks_() const { return this->data_callbacks_.size() > 0; }

  State state_{STATE_STOPPED};
  bool mute_state_{false};

  audio::AudioStreamInfo audio_stream_info_;

  // Make private when the deprecated call(data) is removed in 2027.5.0
  DataCallbackManager data_callbacks_{&this->audio_stream_info_, &this->mute_state_};
};

}  // namespace esphome::microphone
