#include "scheduler.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <cinttypes>

namespace esphome {

static const char *const TAG = "scheduler";

static const uint32_t MAX_LOGICALLY_DELETED_ITEMS = 10;

// Uncomment to debug scheduler
// #define ESPHOME_DEBUG_SCHEDULER

// A note on locking: the `lock_` lock protects the `items_` and `to_add_` containers. It must be taken when writing to
// them (i.e. when adding/removing items, but not when changing items). As items are only deleted from the loop task,
// iterating over them from the loop task is fine; but iterating from any other context requires the lock to be held to
// avoid the main thread modifying the list while it is being accessed.

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> func) {
  const uint32_t now = this->millis_();

  if (!name.empty())
    this->cancel_timeout(component, name);

  if (timeout == SCHEDULER_DONT_RUN)
    return;

  ESP_LOGVV(TAG, "set_timeout(name='%s', timeout=%" PRIu32 ")", name.c_str(), timeout);

  auto item = make_unique<SchedulerItem>();
  item->component = component;
  item->name = name;
  item->type = SchedulerItem::TIMEOUT;
  item->timeout = timeout;
  item->last_execution = now;
  item->last_execution_major = this->millis_major_;
  item->callback = std::move(func);
  item->remove = false;
  this->push_(std::move(item));
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> func) {
  const uint32_t now = this->millis_();

  if (!name.empty())
    this->cancel_interval(component, name);

  if (interval == SCHEDULER_DONT_RUN)
    return;

  // only put offset in lower half
  uint32_t offset = 0;
  if (interval != 0)
    offset = (random_uint32() % interval) / 2;

  ESP_LOGVV(TAG, "set_interval(name='%s', interval=%" PRIu32 ", offset=%" PRIu32 ")", name.c_str(), interval, offset);

  auto item = make_unique<SchedulerItem>();
  item->component = component;
  item->name = name;
  item->type = SchedulerItem::INTERVAL;
  item->interval = interval;
  item->last_execution = now - offset - interval;
  item->last_execution_major = this->millis_major_;
  if (item->last_execution > now)
    item->last_execution_major--;
  item->callback = std::move(func);
  item->remove = false;
  this->push_(std::move(item));
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}

struct RetryArgs {
  std::function<RetryResult(uint8_t)> func;
  uint8_t retry_countdown;
  uint32_t current_interval;
  Component *component;
  std::string name;
  float backoff_increase_factor;
  Scheduler *scheduler;
};

static void retry_handler(const std::shared_ptr<RetryArgs> &args) {
  RetryResult const retry_result = args->func(--args->retry_countdown);
  if (retry_result == RetryResult::DONE || args->retry_countdown <= 0)
    return;
  // second execution of `func` happens after `initial_wait_time`
  args->scheduler->set_timeout(args->component, args->name, args->current_interval, [args]() { retry_handler(args); });
  // backoff_increase_factor applied to third & later executions
  args->current_interval *= args->backoff_increase_factor;
}

void HOT Scheduler::set_retry(Component *component, const std::string &name, uint32_t initial_wait_time,
                              uint8_t max_attempts, std::function<RetryResult(uint8_t)> func,
                              float backoff_increase_factor) {
  if (!name.empty())
    this->cancel_retry(component, name);

  if (initial_wait_time == SCHEDULER_DONT_RUN)
    return;

  ESP_LOGVV(TAG, "set_retry(name='%s', initial_wait_time=%" PRIu32 ", max_attempts=%u, backoff_factor=%0.1f)",
            name.c_str(), initial_wait_time, max_attempts, backoff_increase_factor);

  if (backoff_increase_factor < 0.0001) {
    ESP_LOGE(TAG,
             "set_retry(name='%s'): backoff_factor cannot be close to zero nor negative (%0.1f). Using 1.0 instead",
             name.c_str(), backoff_increase_factor);
    backoff_increase_factor = 1;
  }

  auto args = std::make_shared<RetryArgs>();
  args->func = std::move(func);
  args->retry_countdown = max_attempts;
  args->current_interval = initial_wait_time;
  args->component = component;
  args->name = "retry$" + name;
  args->backoff_increase_factor = backoff_increase_factor;
  args->scheduler = this;

  // First execution of `func` immediately
  this->set_timeout(component, args->name, 0, [args]() { retry_handler(args); });
}
bool HOT Scheduler::cancel_retry(Component *component, const std::string &name) {
  return this->cancel_timeout(component, "retry$" + name);
}

optional<uint32_t> HOT Scheduler::next_schedule_in() {
  if (this->empty_())
    return {};
  auto &item = this->items_[0];
  const uint32_t now = this->millis_();
  uint32_t next_time = item->last_execution + item->interval;
  if (next_time < now)
    return 0;
  return next_time - now;
}
void HOT Scheduler::call() {
  const uint32_t now = this->millis_();
  this->process_to_add();

#ifdef ESPHOME_DEBUG_SCHEDULER
  static uint32_t last_print = 0;

  if (now - last_print > 2000) {
    last_print = now;
    std::vector<std::unique_ptr<SchedulerItem>> old_items;
    ESP_LOGVV(TAG, "Items: count=%u, now=%" PRIu32, this->items_.size(), now);
    while (!this->empty_()) {
      this->lock_.lock();
      auto item = std::move(this->items_[0]);
      this->pop_raw_();
      this->lock_.unlock();

      ESP_LOGVV(TAG, "  %s '%s' interval=%" PRIu32 " last_execution=%" PRIu32 " (%u) next=%" PRIu32 " (%u)",
                item->get_type_str(), item->name.c_str(), item->interval, item->last_execution,
                item->last_execution_major, item->next_execution(), item->next_execution_major());

      old_items.push_back(std::move(item));
    }
    ESP_LOGVV(TAG, "\n");

    {
      LockGuard guard{this->lock_};
      this->items_ = std::move(old_items);
    }
  }
#endif  // ESPHOME_DEBUG_SCHEDULER

  auto to_remove_was = to_remove_;
  auto items_was = this->items_.size();
  // If we have too many items to remove
  if (to_remove_ > MAX_LOGICALLY_DELETED_ITEMS) {
    std::vector<std::unique_ptr<SchedulerItem>> valid_items;
    while (!this->empty_()) {
      LockGuard guard{this->lock_};
      auto item = std::move(this->items_[0]);
      this->pop_raw_();
      valid_items.push_back(std::move(item));
    }

    {
      LockGuard guard{this->lock_};
      this->items_ = std::move(valid_items);
    }

    // The following should not happen unless I'm missing something
    if (to_remove_ != 0) {
      ESP_LOGW(TAG, "to_remove_ was %" PRIu32 " now: %" PRIu32 " items where %zu now %zu. Please report this",
               to_remove_was, to_remove_, items_was, items_.size());
      to_remove_ = 0;
    }
  }

  while (!this->empty_()) {
    // use scoping to indicate visibility of `item` variable
    {
      // Don't copy-by value yet
      auto &item = this->items_[0];
      if ((now - item->last_execution) < item->interval) {
        // Not reached timeout yet, done for this call
        break;
      }
      uint8_t major = item->next_execution_major();
      if (this->millis_major_ - major > 1)
        break;

      // Don't run on failed components
      if (item->component != nullptr && item->component->is_failed()) {
        LockGuard guard{this->lock_};
        this->pop_raw_();
        continue;
      }

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
      ESP_LOGVV(TAG, "Running %s '%s' with interval=%" PRIu32 " last_execution=%" PRIu32 " (now=%" PRIu32 ")",
                item->get_type_str(), item->name.c_str(), item->interval, item->last_execution, now);
#endif

      // Warning: During callback(), a lot of stuff can happen, including:
      //  - timeouts/intervals get added, potentially invalidating vector pointers
      //  - timeouts/intervals get cancelled
      {
        WarnIfComponentBlockingGuard guard{item->component};
        item->callback();
      }
    }

    {
      this->lock_.lock();

      // new scope, item from before might have been moved in the vector
      auto item = std::move(this->items_[0]);

      // Only pop after function call, this ensures we were reachable
      // during the function call and know if we were cancelled.
      this->pop_raw_();

      this->lock_.unlock();

      if (item->remove) {
        // We were removed/cancelled in the function call, stop
        to_remove_--;
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
        this->push_(std::move(item));
      }
    }
  }

  this->process_to_add();
}
void HOT Scheduler::process_to_add() {
  LockGuard guard{this->lock_};
  for (auto &it : this->to_add_) {
    if (it->remove) {
      continue;
    }

    this->items_.push_back(std::move(it));
    std::push_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  }
  this->to_add_.clear();
}
void HOT Scheduler::cleanup_() {
  while (!this->items_.empty()) {
    auto &item = this->items_[0];
    if (!item->remove)
      return;

    to_remove_--;

    {
      LockGuard guard{this->lock_};
      this->pop_raw_();
    }
  }
}
void HOT Scheduler::pop_raw_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->items_.pop_back();
}
void HOT Scheduler::push_(std::unique_ptr<Scheduler::SchedulerItem> item) {
  LockGuard guard{this->lock_};
  this->to_add_.push_back(std::move(item));
}
bool HOT Scheduler::cancel_item_(Component *component, const std::string &name, Scheduler::SchedulerItem::Type type) {
  // obtain lock because this function iterates and can be called from non-loop task context
  LockGuard guard{this->lock_};
  bool ret = false;
  for (auto &it : this->items_) {
    if (it->component == component && it->name == name && it->type == type && !it->remove) {
      to_remove_++;
      it->remove = true;
      ret = true;
    }
  }
  for (auto &it : this->to_add_) {
    if (it->component == component && it->name == name && it->type == type) {
      it->remove = true;
      ret = true;
    }
  }

  return ret;
}
uint32_t Scheduler::millis_() {
  const uint32_t now = millis();
  if (now < this->last_millis_) {
    ESP_LOGD(TAG, "Incrementing scheduler major");
    this->millis_major_++;
  }
  this->last_millis_ = now;
  return now;
}

bool HOT Scheduler::SchedulerItem::cmp(const std::unique_ptr<SchedulerItem> &a,
                                       const std::unique_ptr<SchedulerItem> &b) {
  // min-heap
  // return true if *a* will happen after *b*
  uint32_t a_next_exec = a->next_execution();
  uint8_t a_next_exec_major = a->next_execution_major();
  uint32_t b_next_exec = b->next_execution();
  uint8_t b_next_exec_major = b->next_execution_major();

  if (a_next_exec_major != b_next_exec_major) {
    // The "major" calculation is quite complicated.
    // Basically, we need to check if the major value lies in the future or
    //

    // Here are some cases to think about:
    // Format: a_major,b_major -> expected result (a-b, b-a)
    // a=255,b=0 -> false (255, 1)
    // a=0,b=1 -> false   (255, 1)
    // a=1,b=0 -> true    (1, 255)
    // a=0,b=255 -> true  (1, 255)

    uint8_t diff1 = a_next_exec_major - b_next_exec_major;
    uint8_t diff2 = b_next_exec_major - a_next_exec_major;
    return diff1 < diff2;
  }

  return a_next_exec > b_next_exec;
}

}  // namespace esphome
