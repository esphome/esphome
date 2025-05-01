#pragma once

#include "esphome/components/audio/audio.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include "esphome/core/helpers.h"

namespace esphome {
namespace microphone {

enum State : uint8_t {
  STATE_STOPPED = 0,
  STATE_STARTING,
  STATE_RUNNING,
  STATE_STOPPING,
};

class Microphone {
 public:
  virtual void start() = 0;
  virtual void stop() = 0;
  void add_data_callback(std::function<void(const std::vector<uint8_t> &)> &&data_callback);

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  void set_mute_state(bool is_muted) { this->mute_state_ = is_muted; }
  bool get_mute_state() { return this->mute_state_; }

  audio::AudioStreamInfo get_audio_stream_info() { return this->audio_stream_info_; }

 protected:
  std::vector<uint8_t> silence_audio_(std::vector<uint8_t> data);

  State state_{STATE_STOPPED};
  bool mute_state_{false};

  audio::AudioStreamInfo audio_stream_info_;

  CallbackManager<void(const std::vector<uint8_t> &)> data_callbacks_{};
};

}  // namespace microphone
}  // namespace esphome
