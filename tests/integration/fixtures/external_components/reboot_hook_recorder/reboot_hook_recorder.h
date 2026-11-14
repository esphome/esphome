#pragma once

#include "esphome/core/component.h"

namespace esphome::reboot_hook_recorder {

/// Logs every shutdown lifecycle hook it receives, so a test can check their order.
class RebootHookRecorder : public Component {
 public:
  void set_name(const char *name) { this->name_ = name; }

  void on_safe_shutdown() override { this->record_("on_safe_shutdown"); }
  void on_shutdown() override { this->record_("on_shutdown"); }
  // Needs a second call to finish, like a component waiting for a connection to close
  bool teardown() override {
    if (++this->teardown_calls_ < 2) {
      this->record_("teardown_pending");
      return false;
    }
    if (this->teardown_calls_ == 2)
      this->record_("teardown_done");
    return true;
  }
  void on_powerdown() override { this->record_("on_powerdown"); }

 protected:
  void record_(const char *hook);

  const char *name_{""};
  uint8_t teardown_calls_{0};
};

}  // namespace esphome::reboot_hook_recorder
