#pragma once

#include "esphome/core/component.h"
#include <vector>

namespace esphome {

class Component;

class Scheduler {
 public:
  void set_timeout(Component *component, const std::string &name, uint32_t timeout, std::function<void()> &&func);
  bool cancel_timeout(Component *component, const std::string &name);
  void set_interval(Component *component, const std::string &name, uint32_t interval, std::function<void()> &&func);
  bool cancel_interval(Component *component, const std::string &name);

  optional<uint32_t> next_schedule_in();

  void call();

  void process_to_add();

 protected:
  struct SchedulerItem {
    Component *component;
    std::string name;
    enum Type { TIMEOUT, INTERVAL } type;
    union {
      uint32_t interval;
      uint32_t timeout;
    };
    uint32_t last_execution;
    std::function<void()> f;
    bool remove;

    bool operator<(const SchedulerItem &other) const;
  };

  void cleanup_();
  bool peek_();
  void pop_raw_();
  void push_(SchedulerItem *item);
  bool cancel_item_(Component *component, const std::string &name, SchedulerItem::Type type);

  std::vector<SchedulerItem *> items_;
  std::vector<SchedulerItem *> to_add_;
};

}  // namespace esphome
