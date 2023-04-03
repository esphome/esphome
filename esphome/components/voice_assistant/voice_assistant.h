#pragma once

#include "esphome/core/helpers.h"

#include "esphome/components/microphone/microphone.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace voice_assistant {

class VoiceAssistant {
 public:
  void start(struct sockaddr_storage *addr, uint16_t port);

  void set_microphone(microphone::Microphone *mic) { this->mic_ = mic; }

 protected:
  bool setup_udp_socket_();

  void request_start_();
  void signal_stop_();

  std::unique_ptr<socket::Socket> socket_ = nullptr;
  struct sockaddr_storage dest_addr_;

  microphone::Microphone *mic_{nullptr};

  bool running_{false};
};

extern VoiceAssistant *global_voice_assistant;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome
