#pragma once

#include <vector>
#include <memory>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {

class Component;

class Scheduler {
 public:
  void set_timeout(Component *component, const std::string &name, uint32_t timeout, std::function<void()> func);
  bool cancel_timeout(Component *component, const std::string &name);
  void set_interval(Component *component, const std::string &name, uint32_t interval, std::function<void()> func);
  bool cancel_interval(Component *component, const std::string &name);

  void set_retry(Component *component, const std::string &name, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);
  bool cancel_retry(Component *component, const std::string &name);

  optional<uint32_t> next_schedule_in();

  void call();

  void process_to_add();

 protected:
  struct SchedulerItem {
    Component *component;
    std::string name;
    enum Type { TIMEOUT, INTERVAL } type;
    uint32_t interval;
    uint64_t next_execution_;
    std::function<void()> callback;
    bool remove;

    static bool cmp(const std::unique_ptr<SchedulerItem> &a, const std::unique_ptr<SchedulerItem> &b);
    const char *get_type_str() {
      switch (this->type) {
        case SchedulerItem::INTERVAL:
          return "interval";
        case SchedulerItem::TIMEOUT:
          return "timeout";
        default:
          return "";
      }
    }
    const char *get_source() {
      return this->component != nullptr ? this->component->get_component_source() : "unknown";
    }
  };

  uint64_t millis_();
  void cleanup_();
  void pop_raw_();
  void push_(std::unique_ptr<SchedulerItem> item);
  bool cancel_item_(Component *component, const std::string &name, SchedulerItem::Type type);
  bool empty_() {
    this->cleanup_();
    return this->items_.empty();
  }

  Mutex lock_;
  std::vector<std::unique_ptr<SchedulerItem>> items_;
  std::vector<std::unique_ptr<SchedulerItem>> to_add_;
  uint32_t last_millis_{0};
  uint16_t millis_major_{0};
  uint32_t to_remove_{0};
};

}  // namespace esphome
