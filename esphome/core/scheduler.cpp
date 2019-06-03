#include "scheduler.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {

static const char *TAG = "scheduler";

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> &&func) {
  const uint32_t now = millis();

  if (!name.empty())
    this->cancel_timeout(component, name);

  ESP_LOGVV(TAG, "set_timeout(name='%s', timeout=%u)", name.c_str(), timeout);

  SchedulerItem item{};
  item.component = component;
  item.name = name;
  item.type = SchedulerItem::TIMEOUT;
  item.timeout = timeout;
  item.last_execution = now;
  item.f = std::move(func);
  item.remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> &&func) {
  const uint32_t now = millis();

  // only put offset in lower half
  uint32_t offset = 0;
  if (interval != 0)
    offset = (random_uint32() % interval) / 2;

  if (!name.empty())
    this->cancel_interval(component, name);

  ESP_LOGVV(TAG, "set_interval(name='%s', interval=%u, offset=%u)", name.c_str(), interval, offset);

  SchedulerItem item{};
  item.component = component;
  item.name = name;
  item.type = SchedulerItem::INTERVAL;
  item.interval = interval;
  item.last_execution = now - offset;
  item.f = std::move(func);
  item.remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}
optional<uint32_t> HOT Scheduler::next_schedule_in() {
  if (!this->peek_())
    return {};
  auto &item = this->items_[0];
  const uint32_t now = millis();
  uint32_t next_time = item.last_execution + item.interval;
  if (next_time < now)
    return 0;
  return next_time - now;
}
void ICACHE_RAM_ATTR HOT Scheduler::call() {
  const uint32_t now = millis();
  this->process_to_add_();

  while (true) {
    bool has_item = this->peek_();
    if (!has_item)
      break;

    // Don't copy-by value yet
    auto &item_ref = this->items_[0];
    if ((now - item_ref.last_execution) < item_ref.interval)
      break;

    auto item = this->items_[0];
    this->pop_();

    // Don't run failed components
    if (item.component->is_failed())
      continue;

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    const char *type = item.type == SchedulerItem::INTERVAL ? "interval" : "timeout";
    ESP_LOGVV(TAG, "Running %s '%s' with interval=%u last_execution=%u (now=%u)", type, item.name.c_str(),
              item.interval, item.last_execution, now);
#endif

    item.f();
    if (item.type == SchedulerItem::INTERVAL) {
      if (item.interval != 0) {
        const uint32_t amount = (now - item.last_execution) / item.interval;
        item.last_execution += amount * item.interval;
      }
      this->push_(item);
    }
  }

  this->process_to_add_();
}
void HOT Scheduler::process_to_add_() {
  for (auto &it : this->to_add_) {
    if (it.remove)
      continue;

    this->items_.push_back(it);
    std::push_heap(this->items_.begin(), this->items_.end());
  }
  this->to_add_.clear();
}
void HOT Scheduler::cleanup_() {
  while (!this->items_.empty()) {
    auto item = this->items_[0];
    if (!item.remove)
      return;
    this->pop_raw_();
  }
}
bool HOT Scheduler::peek_() {
  this->cleanup_();
  return !this->items_.empty();
}
bool HOT Scheduler::pop_() {
  this->cleanup_();
  return !this->items_.empty();
}
void HOT Scheduler::pop_raw_() {
  std::pop_heap(this->items_.begin(), this->items_.end());
  auto item = this->items_.back();
  this->items_.pop_back();
}
void HOT Scheduler::push_(const Scheduler::SchedulerItem &item) { this->to_add_.push_back(item); }
bool HOT Scheduler::cancel_item_(Component *component, const std::string &name, Scheduler::SchedulerItem::Type type) {
  bool ret = false;
  for (auto &it : this->items_)
    if (it.component == component && it.name == name && it.type == type) {
      it.remove = true;
      ret = true;
    }
  for (auto &it : this->to_add_)
    if (it.component == component && it.name == name && it.type == type) {
      it.remove = true;
      ret = true;
    }

  return ret;
}

bool HOT Scheduler::SchedulerItem::operator<(const Scheduler::SchedulerItem &other) const {
  // min-heap
  uint32_t this_next_exec = this->last_execution + this->timeout;
  bool this_overflow = this_next_exec < this->last_execution;
  uint32_t other_next_exec = other.last_execution + other.timeout;
  bool other_overflow = other_next_exec < other.last_execution;
  if (this_overflow == other_overflow)
    return this_next_exec > other_next_exec;

  return this_overflow;
}

}  // namespace esphome
