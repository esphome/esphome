#pragma once

#include "esphome/components/audio/audio.h"

#include <cstddef>
#include <cstdint>
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

/// @brief One buffer of captured audio, as passed to data callbacks.
struct AudioCapture {
  /// Interleaved PCM in the stream_info format. Only valid during the callback.
  std::span<const uint8_t> data;
  /// Format of data. Read it here rather than with get_audio_stream_info(), which another thread may be changing.
  audio::AudioStreamInfo stream_info;
  /// When the first frame in data was captured, in esp_timer_get_time() microseconds, or 0 if unknown.
  int64_t capture_time_us{0};
};

// Remove before 2027.5.0. A template, so it only warns for the callbacks that use it.
template<typename F>
ESPDEPRECATED("Take const microphone::AudioCapture & in data callbacks instead. Removed in 2027.5.0", "2026.11.0")
constexpr void warn_vector_data_callback() {}

/// @brief Adapts a data callback to the (const AudioCapture &) form.
///
/// A deprecated callback that takes a const std::vector<uint8_t> & is wrapped to copy each buffer into one.
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

/// @brief The data callback list, with the old call(data) kept for microphone platforms written before
/// publish_audio_().
class DataCallbackManager : public DataCallbackManagerBase {
 public:
  explicit DataCallbackManager(const audio::AudioStreamInfo *stream_info) : stream_info_(stream_info) {}

  using DataCallbackManagerBase::call;

  // Remove before 2027.5.0, along with this class (use a plain CallbackManager)
  ESPDEPRECATED("Use Microphone::publish_audio_() instead. Removed in 2027.5.0", "2026.11.0")
  void call(const std::vector<uint8_t> &data) { this->call(AudioCapture{data, *this->stream_info_, 0}); }

 protected:
  const audio::AudioStreamInfo *stream_info_;
};

class Microphone {
 public:
  virtual void start() = 0;
  virtual void stop() = 0;

  /// @brief Registers a callback for captured audio.
  ///
  /// The callback takes (const AudioCapture &capture). Callbacks run on the microphone's own task.
  template<typename F> void add_data_callback(F &&data_callback) {
    this->data_callbacks_.add(
        [this, callback = as_capture_callback(std::forward<F>(data_callback))](const AudioCapture &capture) {
          if (this->mute_state_) {
            const std::vector<uint8_t> zeros(capture.data.size(), 0);
            callback(AudioCapture{zeros, capture.stream_info, capture.capture_time_us});
          } else {
            callback(capture);
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
  /// @param data Captured audio
  /// @param stream_info Format of data
  /// @param capture_time_us When the first frame in data was captured, in esp_timer_get_time() microseconds, or 0 if
  ///        unknown
  void publish_audio_(std::span<const uint8_t> data, const audio::AudioStreamInfo &stream_info,
                      int64_t capture_time_us = 0) {
    this->data_callbacks_.call(AudioCapture{data, stream_info, capture_time_us});
  }

  bool has_data_callbacks_() const { return this->data_callbacks_.size() > 0; }

  State state_{STATE_STOPPED};
  bool mute_state_{false};

  audio::AudioStreamInfo audio_stream_info_;

  // Platforms use publish_audio_() and has_data_callbacks_(). Make private once the deprecated
  // DataCallbackManager::call(data) is removed in 2027.5.0.
  DataCallbackManager data_callbacks_{&this->audio_stream_info_};
};

}  // namespace esphome::microphone
