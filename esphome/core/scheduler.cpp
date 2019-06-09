#include "scheduler.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {

static const char *TAG = "scheduler";

static const uint32_t SCHEDULER_DONT_RUN = 4294967295UL;

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> &&func) {
  const uint32_t now = millis();

  if (!name.empty())
    this->cancel_timeout(component, name);

  if (timeout == SCHEDULER_DONT_RUN)
    return;

  ESP_LOGVV(TAG, "set_timeout(name='%s', timeout=%u)", name.c_str(), timeout);

  auto *item = new SchedulerItem();
  item->component = component;
  item->name = name;
  item->type = SchedulerItem::TIMEOUT;
  item->timeout = timeout;
  item->last_execution = now;
  item->f = std::move(func);
  item->remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> &&func) {
  const uint32_t now = millis();

  if (!name.empty())
    this->cancel_interval(component, name);

  if (interval == SCHEDULER_DONT_RUN)
    return;

  // only put offset in lower half
  uint32_t offset = 0;
  if (interval != 0)
    offset = (random_uint32() % interval) / 2;

  ESP_LOGVV(TAG, "set_interval(name='%s', interval=%u, offset=%u)", name.c_str(), interval, offset);

  auto *item = new SchedulerItem();
  item->component = component;
  item->name = name;
  item->type = SchedulerItem::INTERVAL;
  item->interval = interval;
  item->last_execution = now - offset;
  item->f = std::move(func);
  item->remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}
optional<uint32_t> HOT Scheduler::next_schedule_in() {
  if (this->items_.empty())
    return {};
  auto *item = this->items_[0];
  const uint32_t now = millis();
  uint32_t next_time = item->last_execution + item->interval;
  if (next_time < now)
    return 0;
  return next_time - now;
}
void ICACHE_RAM_ATTR HOT Scheduler::call() {
  const uint32_t now = millis();
  this->process_to_add();

  while (true) {
    this->cleanup_();
    if (this->items_.empty())
      // No more item left, done!
      break;

    // Don't copy-by value yet
    auto *item = this->items_[0];
    if ((now - item->last_execution) < item->interval)
      // Not reached timeout yet, done for this call
      break;

    // Don't run on failed components
    if (item->component != nullptr && item->component->is_failed()) {
      this->pop_raw_();
      delete item;
      continue;
    }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
    const char *type = item->type == SchedulerItem::INTERVAL ? "interval" : "timeout";
    ESP_LOGVV(TAG, "Running %s '%s' with interval=%u last_execution=%u (now=%u)", type, item->name.c_str(),
              item->interval, item->last_execution, now);
#endif

    // Warning: During f(), a lot of stuff can happen, including:
    //  - timeouts/intervals get added, potentially invalidating vector pointers
    //  - timeouts/intervals get cancelled
    item->f();

    // Only pop after function call, this ensures we were reachable
    // during the function call and know if we were cancelled.
    this->pop_raw_();

    if (item->remove) {
      // We were removed/cancelled in the function call, stop
      delete item;
      continue;
    }

    if (item->type == SchedulerItem::INTERVAL) {
      if (item->interval != 0) {
        const uint32_t amount = (now - item->last_execution) / item->interval;
        item->last_execution += amount * item->interval;
      }
      this->push_(item);
    } else {
      delete item;
    }
  }

  this->process_to_add();
}
void HOT Scheduler::process_to_add() {
  for (auto *it : this->to_add_) {
    if (it->remove) {
      delete it;
      continue;
    }

    this->items_.push_back(it);
    std::push_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  }
  this->to_add_.clear();
}
void HOT Scheduler::cleanup_() {
  while (!this->items_.empty()) {
    auto item = this->items_[0];
    if (!item->remove)
      return;

    delete item;
    this->pop_raw_();
  }
}
void HOT Scheduler::pop_raw_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->items_.pop_back();
}
void HOT Scheduler::push_(Scheduler::SchedulerItem *item) { this->to_add_.push_back(item); }
bool HOT Scheduler::cancel_item_(Component *component, const std::string &name, Scheduler::SchedulerItem::Type type) {
  bool ret = false;
  for (auto *it : this->items_)
    if (it->component == component && it->name == name && it->type == type) {
      it->remove = true;
      ret = true;
    }
  for (auto *it : this->to_add_)
    if (it->component == component && it->name == name && it->type == type) {
      it->remove = true;
      ret = true;
    }

  return ret;
}

bool HOT Scheduler::SchedulerItem::cmp(Scheduler::SchedulerItem *a, Scheduler::SchedulerItem *b) {
  // min-heap
  uint32_t a_next_exec = a->last_execution + a->timeout;
  bool a_overflow = a_next_exec < a->last_execution;
  uint32_t b_next_exec = b->last_execution + b->timeout;
  bool b_overflow = b_next_exec < b->last_execution;
  if (a_overflow == b_overflow)
    return a_next_exec > b_next_exec;

  return a_overflow;
}
}  // namespace esphome
