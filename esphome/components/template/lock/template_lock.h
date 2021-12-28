#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/lock/lock.h"

namespace esphome {
namespace template_ {

class TemplateLock : public lock_::Lock, public Component {
 public:
  TemplateLock();

  void setup() override;
  void dump_config() override;

  void set_state_lambda(std::function<optional<bool>()> &&f);
  void set_restore_state(bool restore_state);
  Trigger<> *get_lock_trigger() const;
  Trigger<> *get_unlock_trigger() const;
  Trigger<> *get_open_trigger() const;
  void set_optimistic(bool optimistic);
  void set_assumed_state(bool assumed_state);
  void loop() override;

  float get_setup_priority() const override;

 protected:
  bool assumed_state() override;

  void write_state(bool state) override;
  void open_latch() override;

  optional<std::function<optional<bool>()>> f_;
  bool optimistic_{false};
  bool assumed_state_{false};
  Trigger<> *lock_trigger_;
  Trigger<> *unlock_trigger_;
  Trigger<> *open_trigger_;
  Trigger<> *prev_trigger_{nullptr};
  bool restore_state_{false};
};

}  // namespace template_
}  // namespace esphome
