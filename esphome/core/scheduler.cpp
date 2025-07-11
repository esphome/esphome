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
  const char *name_cstr = this->get_name_cstr_(is_static_string, name_ptr);

  if (delay == SCHEDULER_DONT_RUN) {
    // Still need to cancel existing timer if name is not empty
    LockGuard guard{this->lock_};
    this->cancel_item_locked_(component, name_cstr, type);
    return;
  }

  // Create and populate the scheduler item
  auto item = make_unique<SchedulerItem>();
  item->component = component;
  item->set_name(name_cstr, !is_static_string);
  item->type = type;
  item->callback = std::move(func);
  item->remove = false;

#if !defined(USE_ESP8266) && !defined(USE_RP2040)
  // Special handling for defer() (delay = 0, type = TIMEOUT)
  // ESP8266 and RP2040 are excluded because they don't need thread-safe defer handling
  if (delay == 0 && type == SchedulerItem::TIMEOUT) {
    // Put in defer queue for guaranteed FIFO execution
    LockGuard guard{this->lock_};
    this->cancel_item_locked_(component, name_cstr, type);
    this->defer_queue_.push_back(std::move(item));
    return;
  }
#endif

  const auto now = this->millis_();

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

  LockGuard guard{this->lock_};
  // If name is provided, do atomic cancel-and-add
  // Cancel existing items
  this->cancel_item_locked_(component, name_cstr, type);
  // Add new item directly to to_add_
  // since we have the lock held
  this->to_add_.push_back(std::move(item));
}

void HOT Scheduler::set_timeout(Component *component, const char *name, uint32_t timeout, std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, true, name, timeout, std::move(func));
}

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, false, &name, timeout, std::move(func));
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, false, &name, SchedulerItem::TIMEOUT);
}
bool HOT Scheduler::cancel_timeout(Component *component, const char *name) {
  return this->cancel_item_(component, true, name, SchedulerItem::TIMEOUT);
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
  return this->cancel_item_(component, false, &name, SchedulerItem::INTERVAL);
}
bool HOT Scheduler::cancel_interval(Component *component, const char *name) {
  return this->cancel_item_(component, true, name, SchedulerItem::INTERVAL);
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
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // It calls empty_() and accesses items_[0] without holding a lock, which is only
  // safe when called from the main thread. Other threads must not call this method.
  if (this->empty_())
    return {};
  auto &item = this->items_[0];
  const auto now = this->millis_();
  if (item->next_execution_ < now)
    return 0;
  return item->next_execution_ - now;
}
void HOT Scheduler::call() {
#if !defined(USE_ESP8266) && !defined(USE_RP2040)
  // Process defer queue first to guarantee FIFO execution order for deferred items.
  // Previously, defer() used the heap which gave undefined order for equal timestamps,
  // causing race conditions on multi-core systems (ESP32, BK7200).
  // With the defer queue:
  // - Deferred items (delay=0) go directly to defer_queue_ in set_timer_common_
  // - Items execute in exact order they were deferred (FIFO guarantee)
  // - No deferred items exist in to_add_, so processing order doesn't affect correctness
  // ESP8266 and RP2040 don't use this queue - they fall back to the heap-based approach
  // (ESP8266: single-core, RP2040: empty mutex implementation).
  //
  // Note: Items cancelled via cancel_item_locked_() are marked with remove=true but still
  // processed here. They are removed from the queue normally via pop_front() but skipped
  // during execution by should_skip_item_(). This is intentional - no memory leak occurs.
  while (!this->defer_queue_.empty()) {
    // The outer check is done without a lock for performance. If the queue
    // appears non-empty, we lock and process an item. We don't need to check
    // empty() again inside the lock because only this thread can remove items.
    std::unique_ptr<SchedulerItem> item;
    {
      LockGuard lock(this->lock_);
      item = std::move(this->defer_queue_.front());
      this->defer_queue_.pop_front();
    }

    // Execute callback without holding lock to prevent deadlocks
    // if the callback tries to call defer() again
    if (!this->should_skip_item_(item.get())) {
      this->execute_item_(item.get());
    }
  }
#endif

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
      std::unique_ptr<SchedulerItem> item;
      {
        LockGuard guard{this->lock_};
        item = std::move(this->items_[0]);
        this->pop_raw_();
      }

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
      // Rebuild heap after moving items back
      std::make_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
    }
  }
#endif  // ESPHOME_DEBUG_SCHEDULER

  // If we have too many items to remove
  if (this->to_remove_ > MAX_LOGICALLY_DELETED_ITEMS) {
    // We hold the lock for the entire cleanup operation because:
    // 1. We're rebuilding the entire items_ list, so we need exclusive access throughout
    // 2. Other threads must see either the old state or the new state, not intermediate states
    // 3. The operation is already expensive (O(n)), so lock overhead is negligible
    // 4. No operations inside can block or take other locks, so no deadlock risk
    LockGuard guard{this->lock_};

    std::vector<std::unique_ptr<SchedulerItem>> valid_items;

    // Move all non-removed items to valid_items
    for (auto &item : this->items_) {
      if (!item->remove) {
        valid_items.push_back(std::move(item));
      }
    }

    // Replace items_ with the filtered list
    this->items_ = std::move(valid_items);
    // Rebuild the heap structure since items are no longer in heap order
    std::make_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
    this->to_remove_ = 0;
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
#ifdef ESPHOME_DEBUG_SCHEDULER
      const char *item_name = item->get_name();
      ESP_LOGV(TAG, "Running %s '%s/%s' with interval=%" PRIu32 " next_execution=%" PRIu64 " (now=%" PRIu64 ")",
               item->get_type_str(), item->get_source(), item_name ? item_name : "(null)", item->interval,
               item->next_execution_, now);
#endif

      // Warning: During callback(), a lot of stuff can happen, including:
      //  - timeouts/intervals get added, potentially invalidating vector pointers
      //  - timeouts/intervals get cancelled
      this->execute_item_(item.get());
    }

    {
      LockGuard guard{this->lock_};

      // new scope, item from before might have been moved in the vector
      auto item = std::move(this->items_[0]);
      // Only pop after function call, this ensures we were reachable
      // during the function call and know if we were cancelled.
      this->pop_raw_();

      if (item->remove) {
        // We were removed/cancelled in the function call, stop
        this->to_remove_--;
        continue;
      }

      if (item->type == SchedulerItem::INTERVAL) {
        item->next_execution_ = now + item->interval;
        // Add new item directly to to_add_
        // since we have the lock held
        this->to_add_.push_back(std::move(item));
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
  // Fast path: if nothing to remove, just return
  // Reading to_remove_ without lock is safe because:
  // 1. We only call this from the main thread during call()
  // 2. If it's 0, there's definitely nothing to cleanup
  // 3. If it becomes non-zero after we check, cleanup will happen on the next loop iteration
  // 4. Not all platforms support atomics, so we accept this race in favor of performance
  // 5. The worst case is a one-loop-iteration delay in cleanup, which is harmless
  if (this->to_remove_ == 0)
    return;

  // We must hold the lock for the entire cleanup operation because:
  // 1. We're modifying items_ (via pop_raw_) which requires exclusive access
  // 2. We're decrementing to_remove_ which is also modified by other threads
  //    (though all modifications are already under lock)
  // 3. Other threads read items_ when searching for items to cancel in cancel_item_locked_()
  // 4. We need a consistent view of items_ and to_remove_ throughout the operation
  // Without the lock, we could access items_ while another thread is reading it,
  // leading to race conditions
  LockGuard guard{this->lock_};
  while (!this->items_.empty()) {
    auto &item = this->items_[0];
    if (!item->remove)
      return;
    this->to_remove_--;
    this->pop_raw_();
  }
}
void HOT Scheduler::pop_raw_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->items_.pop_back();
}

// Helper to execute a scheduler item
void HOT Scheduler::execute_item_(SchedulerItem *item) {
  App.set_current_component(item->component);

  uint32_t now_ms = millis();
  WarnIfComponentBlockingGuard guard{item->component, now_ms};
  item->callback();
  guard.finish();
}

// Common implementation for cancel operations
bool HOT Scheduler::cancel_item_(Component *component, bool is_static_string, const void *name_ptr,
                                 SchedulerItem::Type type) {
  // Get the name as const char*
  const char *name_cstr = this->get_name_cstr_(is_static_string, name_ptr);

  // obtain lock because this function iterates and can be called from non-loop task context
  LockGuard guard{this->lock_};
  return this->cancel_item_locked_(component, name_cstr, type);
}

// Helper to cancel items by name - must be called with lock held
bool HOT Scheduler::cancel_item_locked_(Component *component, const char *name_cstr, SchedulerItem::Type type) {
  // Early return if name is invalid - no items to cancel
  if (name_cstr == nullptr || name_cstr[0] == '\0') {
    return false;
  }

  size_t total_cancelled = 0;

  // Check all containers for matching items
#if !defined(USE_ESP8266) && !defined(USE_RP2040)
  // Only check defer queue for timeouts (intervals never go there)
  if (type == SchedulerItem::TIMEOUT) {
    for (auto &item : this->defer_queue_) {
      if (this->matches_item_(item, component, name_cstr, type)) {
        item->remove = true;
        total_cancelled++;
      }
    }
  }
#endif

  // Cancel items in the main heap
  for (auto &item : this->items_) {
    if (this->matches_item_(item, component, name_cstr, type)) {
      item->remove = true;
      total_cancelled++;
      this->to_remove_++;  // Track removals for heap items
    }
  }

  // Cancel items in to_add_
  for (auto &item : this->to_add_) {
    if (this->matches_item_(item, component, name_cstr, type)) {
      item->remove = true;
      total_cancelled++;
      // Don't track removals for to_add_ items
    }
  }

  return total_cancelled > 0;
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
