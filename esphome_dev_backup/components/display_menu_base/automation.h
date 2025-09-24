#pragma once

#include "esphome/core/automation.h"
#include "display_menu_base.h"

namespace esphome {
namespace display_menu_base {

template<typename... Ts> class UpAction : public Action<Ts...> {
 public:
  explicit UpAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->up(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class DownAction : public Action<Ts...> {
 public:
  explicit DownAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->down(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class LeftAction : public Action<Ts...> {
 public:
  explicit LeftAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->left(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class RightAction : public Action<Ts...> {
 public:
  explicit RightAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->right(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class EnterAction : public Action<Ts...> {
 public:
  explicit EnterAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->enter(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class ShowAction : public Action<Ts...> {
 public:
  explicit ShowAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->show(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class HideAction : public Action<Ts...> {
 public:
  explicit HideAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->hide(); }

 protected:
  DisplayMenuComponent *menu_;
};

template<typename... Ts> class ShowMainAction : public Action<Ts...> {
 public:
  explicit ShowMainAction(DisplayMenuComponent *menu) : menu_(menu) {}

  void play(Ts... x) override { this->menu_->show_main(); }

 protected:
  DisplayMenuComponent *menu_;
};
template<typename... Ts> class IsActiveCondition : public Condition<Ts...> {
 public:
  explicit IsActiveCondition(DisplayMenuComponent *menu) : menu_(menu) {}
  bool check(Ts... x) override { return this->menu_->is_active(); }

 protected:
  DisplayMenuComponent *menu_;
};

class DisplayMenuOnEnterTrigger : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnEnterTrigger(MenuItem *parent) {
    parent->add_on_enter_callback([this, parent]() { this->trigger(parent); });
  }
};

class DisplayMenuOnLeaveTrigger : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnLeaveTrigger(MenuItem *parent) {
    parent->add_on_leave_callback([this, parent]() { this->trigger(parent); });
  }
};

class DisplayMenuOnValueTrigger : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnValueTrigger(MenuItem *parent) {
    parent->add_on_value_callback([this, parent]() { this->trigger(parent); });
  }
};

class DisplayMenuOnNextTrigger : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnNextTrigger(MenuItemCustom *parent) {
    parent->add_on_next_callback([this, parent]() { this->trigger(parent); });
  }
};

class DisplayMenuOnPrevTrigger : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnPrevTrigger(MenuItemCustom *parent) {
    parent->add_on_prev_callback([this, parent]() { this->trigger(parent); });
  }
};

}  // namespace display_menu_base
}  // namespace esphome
