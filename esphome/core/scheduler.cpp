#include "scheduler.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {

static const char *TAG = "scheduler";

static const uint32_t SCHEDULER_DONT_RUN = 4294967295UL;

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> &&func) {
  const uint32_t now = this->millis_();

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
  item->last_execution_major = this->millis_major_;
  item->f = std::move(func);
  item->remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> &&func) {
  const uint32_t now = this->millis_();

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
  item->last_execution_major = this->millis_major_;
  if (item->last_execution > now)
    item->last_execution_major--;
  item->f = std::move(func);
  item->remove = false;
  this->push_(item);
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}
optional<uint32_t> HOT Scheduler::next_schedule_in() {
  if (this->empty_())
    return {};
  auto *item = this->items_[0];
  const uint32_t now = this->millis_();
  uint32_t next_time = item->last_execution + item->interval;
  if (next_time < now)
    return 0;
  return next_time - now;
}
void ICACHE_RAM_ATTR HOT Scheduler::call() {
  const uint32_t now = this->millis_();
  this->process_to_add();

  // Uncomment for debugging the scheduler:

  // if (random_uint32() % 400 == 0) {
  //  std::vector<SchedulerItem *> old_items = this->items_;
  //  ESP_LOGVV(TAG, "Items: count=%u, now=%u", this->items_.size(), now);
  //  while (!this->empty_()) {
  //    auto *item = this->items_[0];
  //    const char *type = item->type == SchedulerItem::INTERVAL ? "interval" : "timeout";
  //    ESP_LOGVV(TAG, "  %s '%s' interval=%u last_execution=%u (%u) next=%u",
  //             type, item->name.c_str(), item->interval, item->last_execution, item->last_execution_major,
  //             item->last_execution + item->interval);
  //    this->pop_raw_();
  //  }
  //  ESP_LOGVV(TAG, "\n");
  //  this->items_ = old_items;
  //}

  while (!this->empty_()) {
    // Don't copy-by value yet
    auto *item = this->items_[0];
    if ((now - item->last_execution) < item->interval)
      // Not reached timeout yet, done for this call
      break;
    uint8_t major = item->last_execution_major;
    if (item->last_execution + item->interval < item->last_execution)
      major++;
    if (major != this->millis_major_)
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
        const uint32_t before = item->last_execution;
        const uint32_t amount = (now - item->last_execution) / item->interval;
        item->last_execution += amount * item->interval;
        if (item->last_execution < before)
          item->last_execution_major++;
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
uint32_t Scheduler::millis_() {
  const uint32_t now = millis();
  if (now < this->last_millis_) {
    ESP_LOGD(TAG, "Incrementing scheduler major");
    this->millis_major_++;
  }
  return now;
}

bool HOT Scheduler::SchedulerItem::cmp(Scheduler::SchedulerItem *a, Scheduler::SchedulerItem *b) {
  // min-heap
  // return true if *a* will happen after *b*
  uint32_t a_next_exec = a->last_execution + a->timeout;
  uint8_t a_next_exec_major = a->last_execution_major;
  if (a_next_exec < a->last_execution)
    a_next_exec_major++;

  uint32_t b_next_exec = b->last_execution + b->timeout;
  uint8_t b_next_exec_major = b->last_execution_major;
  if (b_next_exec < b->last_execution)
    b_next_exec_major++;

  if (a_next_exec_major != b_next_exec_major) {
    return a_next_exec_major > b_next_exec_major;
  }

  return a_next_exec > b_next_exec;
}

}  // namespace esphome
