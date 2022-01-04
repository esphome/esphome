#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/lock/lock.h"

namespace esphome {
namespace template_ {

using namespace esphome::lock;

class TemplateLock : public lock::Lock, public Component {
 public:
  TemplateLock();

  void dump_config() override;

  void set_state_lambda(std::function<optional<LockState>()> &&f);
  Trigger<> *get_lock_trigger() const;
  Trigger<> *get_unlock_trigger() const;
  Trigger<> *get_open_trigger() const;
  void set_optimistic(bool optimistic);
  void loop() override;

  float get_setup_priority() const override;

 protected:
  void write_state(lock::LockState state) override;
  void open_latch() override;

  optional<std::function<optional<LockState>()>> f_;
  bool optimistic_{false};
  Trigger<> *lock_trigger_;
  Trigger<> *unlock_trigger_;
  Trigger<> *open_trigger_;
  Trigger<> *prev_trigger_{nullptr};
};

}  // namespace template_
}  // namespace esphome
