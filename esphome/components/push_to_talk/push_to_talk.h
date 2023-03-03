#pragma once

#include "esphome/core/defines.h"

#ifdef USE_PUSH_TO_TALK

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/core/component.h"

#include "esphome/components/socket/socket.h"

namespace esphome {
namespace push_to_talk {

static const size_t BUFFER_SIZE = 512;

class PushToTalk : public Component {
 public:
  PushToTalk();
  void setup() override;

  void start(struct sockaddr_storage *addr, uint16_t port);

  void set_binary_sensor(binary_sensor::BinarySensor *binary_sensor) { this->binary_sensor_ = binary_sensor; }
  void set_microphone(microphone::Microphone *mic) { this->mic_ = mic; }

  float get_setup_priority() const override;

 protected:
  std::unique_ptr<socket::Socket> socket_ = nullptr;
  struct sockaddr_storage dest_addr_;

  binary_sensor::BinarySensor *binary_sensor_{nullptr};
  microphone::Microphone *mic_{nullptr};

  bool running_{false};
};

extern PushToTalk *global_push_to_talk;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#endif

}  // namespace push_to_talk
}  // namespace esphome
