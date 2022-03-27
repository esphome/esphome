#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "remote.h"

namespace esphome {
namespace remote {

template<typename... Ts> class SendCommandAction : public Action<Ts...> {
 public:
  explicit SendCommandAction(Remote *a_remote) : remote_(a_remote) {}

  TEMPLATABLE_VALUE(uint32_t, send_times);
  TEMPLATABLE_VALUE(uint32_t, send_wait);

  void play(Ts... x) override {
    this->remote_->send_command(send_times_.value(x...), send_wait_.value(x...), protocol_, args_);
  }

  void set_command(std::string protocol, std::vector<int64_t> args) {
    protocol_ = protocol;
    args_.clear();
    args_.reserve(args.size());
    for (auto arg : args)
      args_.push_back((arg_t) arg);
  }

 protected:
  Remote *remote_;
  std::string protocol_;
  std::vector<arg_t> args_;
};

class RemoteTurnOnTrigger : public Trigger<> {
 public:
  RemoteTurnOnTrigger(Remote *a_remote) {
    a_remote->add_on_state_callback([this](bool state) {
      if (state) {
        this->trigger();
      }
    });
  }
};

class RemoteTurnOffTrigger : public Trigger<> {
 public:
  RemoteTurnOffTrigger(Remote *a_remote) {
    a_remote->add_on_state_callback([this](bool state) {
      if (!state) {
        this->trigger();
      }
    });
  }
};

}  // namespace remote
}  // namespace esphome
