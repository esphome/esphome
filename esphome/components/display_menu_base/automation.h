#pragma once

#include "esphome/core/automation.h"
#include "display_menu_base.h"

namespace esphome::display_menu_base {

class DisplayMenuOnEnterTrigger final : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnEnterTrigger(MenuItem *parent) : parent_(parent) {
    parent->add_on_enter_callback([this]() { this->trigger(this->parent_); });
  }

 protected:
  MenuItem *parent_;
};

class DisplayMenuOnLeaveTrigger final : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnLeaveTrigger(MenuItem *parent) : parent_(parent) {
    parent->add_on_leave_callback([this]() { this->trigger(this->parent_); });
  }

 protected:
  MenuItem *parent_;
};

class DisplayMenuOnValueTrigger final : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnValueTrigger(MenuItem *parent) : parent_(parent) {
    parent->add_on_value_callback([this]() { this->trigger(this->parent_); });
  }

 protected:
  MenuItem *parent_;
};

class DisplayMenuOnNextTrigger final : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnNextTrigger(MenuItemCustom *parent) : parent_(parent) {
    parent->add_on_next_callback([this]() { this->trigger(this->parent_); });
  }

 protected:
  MenuItemCustom *parent_;
};

class DisplayMenuOnPrevTrigger final : public Trigger<const MenuItem *> {
 public:
  explicit DisplayMenuOnPrevTrigger(MenuItemCustom *parent) : parent_(parent) {
    parent->add_on_prev_callback([this]() { this->trigger(this->parent_); });
  }

 protected:
  MenuItemCustom *parent_;
};

}  // namespace esphome::display_menu_base
