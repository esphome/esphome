#pragma once

#include <stddef.h>
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
  void add_data_callback(std::function<void(const std::vector<int16_t> &)> &&data_callback) {
    this->data_callbacks_.add(std::move(data_callback));
  }
  virtual size_t read(int16_t *buf, size_t len) = 0;

  bool is_running() const { return this->state_ == STATE_RUNNING; }
  bool is_stopped() const { return this->state_ == STATE_STOPPED; }

 protected:
  State state_{STATE_STOPPED};

  CallbackManager<void(const std::vector<int16_t> &)> data_callbacks_{};
};

}  // namespace microphone
}  // namespace esphome
