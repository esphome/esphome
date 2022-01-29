#pragma once

#include <utility>

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "api_pb2.h"

namespace esphome {
namespace api {

class UserTriggerDescriptor {
 public:
  virtual ListEntitiesTriggersResponse encode_list_trigger_response() = 0;
};

class UserTriggerBase : public UserTriggerDescriptor {
 public:
  UserTriggerBase(std::string trigger) : trigger_(std::move(trigger)) { this->key_ = fnv1_hash(this->trigger_); }

  ListEntitiesTriggersResponse encode_list_trigger_response() override {
    ListEntitiesTriggersResponse msg;
    msg.trigger = this->trigger_;
    msg.key = this->key_;
    return msg;
  }

 protected:
  std::string trigger_;
  uint32_t key_{0};
};

class UserTriggerTrigger : public UserTriggerBase {
 public:
  UserTriggerTrigger(const std::string &trigger) : UserTriggerBase(trigger) {}
};

}  // namespace api
}  // namespace esphome
