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

template<typename... Ts> class EmulationOffAction : public Action<Ts...> {
 public:
  explicit EmulationOffAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->set_tag_emulation_off(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class EmulationOnAction : public Action<Ts...> {
 public:
  explicit EmulationOnAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->set_tag_emulation_on(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class PollingOffAction : public Action<Ts...> {
 public:
  explicit PollingOffAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->set_polling_off(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class PollingOnAction : public Action<Ts...> {
 public:
  explicit PollingOnAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->set_polling_on(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetCleanModeAction : public Action<Ts...> {
 public:
  explicit SetCleanModeAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->clean_mode(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetFormatModeAction : public Action<Ts...> {
 public:
  explicit SetFormatModeAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->format_mode(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetReadModeAction : public Action<Ts...> {
 public:
  explicit SetReadModeAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->read_mode(); }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetEmulationMessageAction : public Action<Ts...> {
 public:
  explicit SetEmulationMessageAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  TEMPLATABLE_VALUE(std::string, message)
  TEMPLATABLE_VALUE(bool, include_android_app_record)

  void play(Ts... x) override {
    this->pn7160_->set_tag_emulation_message(this->message_.optional_value(x...),
                                             this->include_android_app_record_.optional_value(x...));
  }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetWriteMessageAction : public Action<Ts...> {
 public:
  explicit SetWriteMessageAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  TEMPLATABLE_VALUE(std::string, message)
  TEMPLATABLE_VALUE(bool, include_android_app_record)

  void play(Ts... x) override {
    this->pn7160_->set_tag_write_message(this->message_.optional_value(x...),
                                         this->include_android_app_record_.optional_value(x...));
  }

 protected:
  PN7160 *pn7160_;
};

template<typename... Ts> class SetWriteModeAction : public Action<Ts...> {
 public:
  explicit SetWriteModeAction(PN7160 *a_pn7160) : pn7160_(a_pn7160) {}

  void play(Ts... x) override { this->pn7160_->write_mode(); }

 protected:
  PN7160 *pn7160_;
};

}  // namespace pn7160
}  // namespace esphome
