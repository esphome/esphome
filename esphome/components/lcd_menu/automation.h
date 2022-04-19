#pragma once

#include "esphome/core/automation.h"
#include "lcd_menu.h"

namespace esphome {
namespace lcd_menu {

template<typename... Ts> class UpAction : public Action<Ts...> {
 public:
  explicit UpAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->up(); }

 protected:
  LCDMenuComponent *menu_;
};

template<typename... Ts> class DownAction : public Action<Ts...> {
 public:
  explicit DownAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->down(); }

 protected:
  LCDMenuComponent *menu_;
};

template<typename... Ts> class EnterAction : public Action<Ts...> {
 public:
  explicit EnterAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->enter(); }

 protected:
  LCDMenuComponent *menu_;
};

template<typename... Ts> class ShowAction : public Action<Ts...> {
 public:
  explicit ShowAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->show(); }

 protected:
  LCDMenuComponent *menu_;
};

template<typename... Ts> class HideAction : public Action<Ts...> {
 public:
  explicit HideAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->hide(); }

 protected:
  LCDMenuComponent *menu_;
};

template<typename... Ts> class ShowMainAction : public Action<Ts...> {
 public:
  explicit ShowMainAction(LCDMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->show_main(); }

 protected:
  LCDMenuComponent *menu_;
};
template<typename... Ts> class IsActiveCondition : public Condition<Ts...> {
 public:
  explicit IsActiveCondition(LCDMenuComponent *menu) : menu_(menu) {}
  bool check(Ts... x) override { return this->menu_->is_active(); }

 protected:
  LCDMenuComponent *menu_;
};

class LCDMenuOnEnterTrigger : public Trigger<const MenuItem *> {
 public:
  explicit LCDMenuOnEnterTrigger(MenuItem *parent) {
    parent->add_on_enter_callback([this, parent]() { this->trigger(parent); });
  }
};

class LCDMenuOnLeaveTrigger : public Trigger<const MenuItem *> {
 public:
  explicit LCDMenuOnLeaveTrigger(MenuItem *parent) {
    parent->add_on_leave_callback([this, parent]() { this->trigger(parent); });
  }
};

class LCDMenuOnValueTrigger : public Trigger<const MenuItem *> {
 public:
  explicit LCDMenuOnValueTrigger(MenuItem *parent) {
    parent->add_on_value_callback([this, parent]() { this->trigger(parent); });
  }
};

}  // namespace lcd_menu
}  // namespace esphome
