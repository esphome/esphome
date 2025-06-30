#include "scheduler.h"

#include "application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>

namespace esphome {

static const char *const TAG = "scheduler";

static const uint32_t MAX_LOGICALLY_DELETED_ITEMS = 10;

// Uncomment to debug scheduler
// #define ESPHOME_DEBUG_SCHEDULER

#ifdef ESPHOME_DEBUG_SCHEDULER
// Helper to validate that a pointer looks like it's in static memory
static void validate_static_string(const char *name) {
  if (name == nullptr)
    return;

  // This is a heuristic check - stack and heap pointers are typically
  // much higher in memory than static data
  uintptr_t addr = reinterpret_cast<uintptr_t>(name);

  // Create a stack variable to compare against
  int stack_var;
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(&stack_var);

  // If the string pointer is near our stack variable, it's likely on the stack
  // Using 8KB range as ESP32 main task stack is typically 8192 bytes
  if (addr > (stack_addr - 0x2000) && addr < (stack_addr + 0x2000)) {
    ESP_LOGW(TAG,
             "WARNING: Scheduler name '%s' at %p appears to be on the stack - this is unsafe!\n"
             "         Stack reference at %p",
             name, name, &stack_var);
  }

  // Also check if it might be on the heap by seeing if it's in a very different range
  // This is platform-specific but generally heap is allocated far from static memory
  static const char *static_str = "test";
  uintptr_t static_addr = reinterpret_cast<uintptr_t>(static_str);

  // If the address is very far from known static memory, it might be heap
  if (addr > static_addr + 0x100000 || (static_addr > 0x100000 && addr < static_addr - 0x100000)) {
    ESP_LOGW(TAG, "WARNING: Scheduler name '%s' at %p might be on heap (static ref at %p)", name, name, static_str);
  }
}
#endif

// A note on locking: the `lock_` lock protects the `items_` and `to_add_` containers. It must be taken when writing to
// them (i.e. when adding/removing items, but not when changing items). As items are only deleted from the loop task,
// iterating over them from the loop task is fine; but iterating from any other context requires the lock to be held to
// avoid the main thread modifying the list while it is being accessed.

// Common implementation for both timeout and interval
void HOT Scheduler::set_timer_common_(Component *component, SchedulerItem::Type type, bool is_static_string,
                                      const void *name_ptr, uint32_t delay, std::function<void()> func) {
  // Get the name as const char*
  const char *name_cstr =
      is_static_string ? static_cast<const char *>(name_ptr) : static_cast<const std::string *>(name_ptr)->c_str();

  // Cancel existing timer if name is not empty
  if (name_cstr != nullptr && name_cstr[0] != '\0') {
    this->cancel_item_(component, name_cstr, type);
  }

  if (delay == SCHEDULER_DONT_RUN)
    return;

  const auto now = this->millis_();

  // Create and populate the scheduler item
  auto item = make_unique<SchedulerItem>();
  item->component = component;
  item->set_name(name_cstr, !is_static_string);
  item->type = type;
  item->callback = std::move(func);
  item->remove = false;

  // Type-specific setup
  if (type == SchedulerItem::INTERVAL) {
    item->interval = delay;
    // Calculate random offset (0 to interval/2)
    uint32_t offset = (delay != 0) ? (random_uint32() % delay) / 2 : 0;
    item->next_execution_ = now + offset;
  } else {
    item->interval = 0;
    item->next_execution_ = now + delay;
  }

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Validate static strings in debug mode
  if (is_static_string && name_cstr != nullptr) {
    validate_static_string(name_cstr);
  }

  // Debug logging
  const char *type_str = (type == SchedulerItem::TIMEOUT) ? "timeout" : "interval";
  if (type == SchedulerItem::TIMEOUT) {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ")", type_str, item->get_source(),
             name_cstr ? name_cstr : "(null)", type_str, delay);
  } else {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ", offset=%" PRIu32 ")", type_str, item->get_source(),
             name_cstr ? name_cstr : "(null)", type_str, delay, static_cast<uint32_t>(item->next_execution_ - now));
  }
#endif

  this->push_(std::move(item));
}

void HOT Scheduler::set_timeout(Component *component, const char *name, uint32_t timeout, std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, true, name, timeout, std::move(func));
}

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, false, &name, timeout, std::move(func));
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
bool HOT Scheduler::cancel_timeout(Component *component, const char *name) {
  return this->cancel_item_(component, name, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::INTERVAL, false, &name, interval, std::move(func));
}

void HOT Scheduler::set_interval(Component *component, const char *name, uint32_t interval,
                                 std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::INTERVAL, true, name, interval, std::move(func));
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}
bool HOT Scheduler::cancel_interval(Component *component, const char *name) {
  return this->cancel_item_(component, name, SchedulerItem::INTERVAL);
}

struct RetryArgs {
  std::function<RetryResult(uint8_t)> func;
  uint8_t retry_countdown;
  uint32_t current_interval;
  Component *component;
  std::string name;  // Keep as std::string since retry uses it dynamically
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
  const auto now = this->millis_();
  if (item->next_execution_ < now)
    return 0;
  return item->next_execution_ - now;
}
void HOT Scheduler::call() {
  const auto now = this->millis_();
  this->process_to_add();

#ifdef ESPHOME_DEBUG_SCHEDULER
  static uint64_t last_print = 0;

  if (now - last_print > 2000) {
    last_print = now;
    std::vector<std::unique_ptr<SchedulerItem>> old_items;
    ESP_LOGD(TAG, "Items: count=%zu, now=%" PRIu64 " (%u, %" PRIu32 ")", this->items_.size(), now, this->millis_major_,
             this->last_millis_);
    while (!this->empty_()) {
      this->lock_.lock();
      auto item = std::move(this->items_[0]);
      this->pop_raw_();
      this->lock_.unlock();

      const char *name = item->get_name();
      ESP_LOGD(TAG, "  %s '%s/%s' interval=%" PRIu32 " next_execution in %" PRIu64 "ms at %" PRIu64,
               item->get_type_str(), item->get_source(), name ? name : "(null)", item->interval,
               item->next_execution_ - now, item->next_execution_);

      old_items.push_back(std::move(item));
    }
    ESP_LOGD(TAG, "\n");

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
      if (item->next_execution_ > now) {
        // Not reached timeout yet, done for this call
        break;
      }
      // Don't run on failed components
      if (item->component != nullptr && item->component->is_failed()) {
        LockGuard guard{this->lock_};
        this->pop_raw_();
        continue;
      }
      App.set_current_component(item->component);

#ifdef ESPHOME_DEBUG_SCHEDULER
      const char *item_name = item->get_name();
      ESP_LOGV(TAG, "Running %s '%s/%s' with interval=%" PRIu32 " next_execution=%" PRIu64 " (now=%" PRIu64 ")",
               item->get_type_str(), item->get_source(), item_name ? item_name : "(null)", item->interval,
               item->next_execution_, now);
#endif

      // Warning: During callback(), a lot of stuff can happen, including:
      //  - timeouts/intervals get added, potentially invalidating vector pointers
      //  - timeouts/intervals get cancelled
      {
        uint32_t now_ms = millis();
        WarnIfComponentBlockingGuard guard{item->component, now_ms};
        item->callback();
        // Call finish to ensure blocking time is properly calculated and reported
        guard.finish();
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
        item->next_execution_ = now + item->interval;
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
// Common implementation for cancel operations
bool HOT Scheduler::cancel_item_common_(Component *component, bool is_static_string, const void *name_ptr,
                                        SchedulerItem::Type type) {
  // Get the name as const char*
  const char *name_cstr =
      is_static_string ? static_cast<const char *>(name_ptr) : static_cast<const std::string *>(name_ptr)->c_str();

  // Handle null or empty names
  if (name_cstr == nullptr)
    return false;

  // obtain lock because this function iterates and can be called from non-loop task context
  LockGuard guard{this->lock_};
  bool ret = false;

  for (auto &it : this->items_) {
    const char *item_name = it->get_name();
    if (it->component == component && item_name != nullptr && strcmp(name_cstr, item_name) == 0 && it->type == type &&
        !it->remove) {
      to_remove_++;
      it->remove = true;
      ret = true;
    }
  }
  for (auto &it : this->to_add_) {
    const char *item_name = it->get_name();
    if (it->component == component && item_name != nullptr && strcmp(name_cstr, item_name) == 0 && it->type == type) {
      it->remove = true;
      ret = true;
    }
  }

  return ret;
}

bool HOT Scheduler::cancel_item_(Component *component, const std::string &name, Scheduler::SchedulerItem::Type type) {
  return this->cancel_item_common_(component, false, &name, type);
}

bool HOT Scheduler::cancel_item_(Component *component, const char *name, SchedulerItem::Type type) {
  return this->cancel_item_common_(component, true, name, type);
}

uint64_t Scheduler::millis_() {
  // Get the current 32-bit millis value
  const uint32_t now = millis();
  // Check for rollover by comparing with last value
  if (now < this->last_millis_) {
    // Detected rollover (happens every ~49.7 days)
    this->millis_major_++;
    ESP_LOGD(TAG, "Incrementing scheduler major at %" PRIu64 "ms",
             now + (static_cast<uint64_t>(this->millis_major_) << 32));
  }
  this->last_millis_ = now;
  // Combine major (high 32 bits) and now (low 32 bits) into 64-bit time
  return now + (static_cast<uint64_t>(this->millis_major_) << 32);
}

bool HOT Scheduler::SchedulerItem::cmp(const std::unique_ptr<SchedulerItem> &a,
                                       const std::unique_ptr<SchedulerItem> &b) {
  return a->next_execution_ > b->next_execution_;
}

}  // namespace esphome
