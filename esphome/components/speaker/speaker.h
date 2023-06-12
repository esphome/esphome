#pragma once

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
  virtual bool play(const uint8_t *data, size_t length) = 0;
  virtual bool play(const std::vector<uint8_t> &data) { return this->play(data.data(), data.size()); }

  virtual void stop() = 0;

  bool is_running() const { return this->state_ == STATE_RUNNING; }

 protected:
  State state_{STATE_STOPPED};
};

}  // namespace speaker
}  // namespace esphome
