#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/components/pn7160/pn7160.h"

namespace esphome {
namespace pn7160 {

class PN7160OnEmulatedTagScanTrigger : public Trigger<> {
 public:
  explicit PN7160OnEmulatedTagScanTrigger(PN7160 *parent) {
    parent->add_on_emulated_tag_scan_callback([this]() { this->trigger(); });
  }
};

class PN7160OnFinishedWriteTrigger : public Trigger<> {
 public:
  explicit PN7160OnFinishedWriteTrigger(PN7160 *parent) {
    parent->add_on_finished_write_callback([this]() { this->trigger(); });
  }
};

template<typename... Ts> class PN7160IsWritingCondition : public Condition<Ts...>, public Parented<PN7160> {
 public:
  bool check(Ts... x) override { return this->parent_->is_writing(); }
};

template<typename... Ts> class EmulationOffAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->set_tag_emulation_off(); }
};

template<typename... Ts> class EmulationOnAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->set_tag_emulation_on(); }
};

template<typename... Ts> class PollingOffAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->set_polling_off(); }
};

template<typename... Ts> class PollingOnAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->set_polling_on(); }
};

template<typename... Ts> class SetCleanModeAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->clean_mode(); }
};

template<typename... Ts> class SetFormatModeAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->format_mode(); }
};

template<typename... Ts> class SetReadModeAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->read_mode(); }
};

template<typename... Ts> class SetEmulationMessageAction : public Action<Ts...>, public Parented<PN7160> {
  TEMPLATABLE_VALUE(std::string, message)
  TEMPLATABLE_VALUE(bool, include_android_app_record)

  void play(Ts... x) override {
    this->parent_->set_tag_emulation_message(this->message_.optional_value(x...),
                                             this->include_android_app_record_.optional_value(x...));
  }
};

template<typename... Ts> class SetWriteMessageAction : public Action<Ts...>, public Parented<PN7160> {
  TEMPLATABLE_VALUE(std::string, message)
  TEMPLATABLE_VALUE(bool, include_android_app_record)

  void play(Ts... x) override {
    this->parent_->set_tag_write_message(this->message_.optional_value(x...),
                                         this->include_android_app_record_.optional_value(x...));
  }
};

template<typename... Ts> class SetWriteModeAction : public Action<Ts...>, public Parented<PN7160> {
  void play(Ts... x) override { this->parent_->write_mode(); }
};

}  // namespace pn7160
}  // namespace esphome
