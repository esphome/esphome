#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esphome/components/api/api_pb2.h"
#include "esphome/components/api/api_server.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/socket/socket.h"

namespace esphome {
namespace voice_assistant {

class VoiceAssistant : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override;
  void start(struct sockaddr_storage *addr, uint16_t port);

  void set_microphone(microphone::Microphone *mic) { this->mic_ = mic; }

  void request_start();
  void signal_stop();

  void on_event(const api::VoiceAssistantEventResponse &msg);

 protected:
  std::unique_ptr<socket::Socket> socket_ = nullptr;
  struct sockaddr_storage dest_addr_;

  microphone::Microphone *mic_{nullptr};

  bool running_{false};
};

template<typename... Ts> class StartAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->request_start(); }
};

template<typename... Ts> class StopAction : public Action<Ts...>, public Parented<VoiceAssistant> {
 public:
  void play(Ts... x) override { this->parent_->signal_stop(); }
};

extern VoiceAssistant *global_voice_assistant;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace voice_assistant
}  // namespace esphome
