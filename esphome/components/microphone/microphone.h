#pragma once

#include "esphome/components/audio/audio.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include "esphome/core/helpers.h"

namespace esphome::microphone {

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
  template<typename F> void add_data_callback(F &&data_callback) {
    this->data_callbacks_.add([this, data_callback](const std::vector<uint8_t> &data) {
      if (this->mute_state_) {
        data_callback(std::vector<uint8_t>(data.size(), 0));
      } else {
        data_callback(data);
      }
    });
  }

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

  void set_mute_state(bool is_muted) { this->mute_state_ = is_muted; }
  bool get_mute_state() { return this->mute_state_; }

  audio::AudioStreamInfo get_audio_stream_info() { return this->audio_stream_info_; }

 protected:
  State state_{STATE_STOPPED};
  bool mute_state_{false};

  audio::AudioStreamInfo audio_stream_info_;

  CallbackManager<void(const std::vector<uint8_t> &)> data_callbacks_{};
};

}  // namespace esphome::microphone
