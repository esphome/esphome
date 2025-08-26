#include "scheduler.h"

#include "application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <limits>

namespace esphome {

static const char *const TAG = "scheduler";

static const uint32_t MAX_LOGICALLY_DELETED_ITEMS = 10;
// Half the 32-bit range - used to detect rollovers vs normal time progression
static constexpr uint32_t HALF_MAX_UINT32 = std::numeric_limits<uint32_t>::max() / 2;
// max delay to start an interval sequence
static constexpr uint32_t MAX_INTERVAL_DELAY = 5000;

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
#endif /* ESPHOME_DEBUG_SCHEDULER */

// A note on locking: the `lock_` lock protects the `items_` and `to_add_` containers. It must be taken when writing to
// them (i.e. when adding/removing items, but not when changing items). As items are only deleted from the loop task,
// iterating over them from the loop task is fine; but iterating from any other context requires the lock to be held to
// avoid the main thread modifying the list while it is being accessed.

// Common implementation for both timeout and interval
void HOT Scheduler::set_timer_common_(Component *component, SchedulerItem::Type type, bool is_static_string,
                                      const void *name_ptr, uint32_t delay, std::function<void()> func, bool is_retry,
                                      bool skip_cancel) {
  // Get the name as const char*
  const char *name_cstr = this->get_name_cstr_(is_static_string, name_ptr);

  if (delay == SCHEDULER_DONT_RUN) {
    // Still need to cancel existing timer if name is not empty
    if (!skip_cancel) {
      LockGuard guard{this->lock_};
      this->cancel_item_locked_(component, name_cstr, type);
    }
    return;
  }

  // Create and populate the scheduler item
  auto item = make_unique<SchedulerItem>();
  item->component = component;
  item->set_name(name_cstr, !is_static_string);
  item->type = type;
  item->callback = std::move(func);
  // Initialize remove to false (though it should already be from constructor)
  // Not using mark_item_removed_ helper since we're setting to false, not true
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  item->remove.store(false, std::memory_order_relaxed);
#else
  item->remove = false;
#endif
  item->is_retry = is_retry;

#ifndef ESPHOME_THREAD_SINGLE
  // Special handling for defer() (delay = 0, type = TIMEOUT)
  // Single-core platforms don't need thread-safe defer handling
  if (delay == 0 && type == SchedulerItem::TIMEOUT) {
    // Put in defer queue for guaranteed FIFO execution
    LockGuard guard{this->lock_};
    if (!skip_cancel) {
      this->cancel_item_locked_(component, name_cstr, type);
    }
    this->defer_queue_.push_back(std::move(item));
    return;
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Get fresh timestamp for new timer/interval - ensures accurate scheduling
  const auto now = this->millis_64_(millis());  // Fresh millis() call

  // Type-specific setup
  if (type == SchedulerItem::INTERVAL) {
    item->interval = delay;
    // first execution happens immediately after a random smallish offset
    // Calculate random offset (0 to min(interval/2, 5s))
    uint32_t offset = (uint32_t) (std::min(delay / 2, MAX_INTERVAL_DELAY) * random_float());
    item->next_execution_ = now + offset;
    ESP_LOGV(TAG, "Scheduler interval for %s is %" PRIu32 "ms, offset %" PRIu32 "ms", name_cstr ? name_cstr : "", delay,
             offset);
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
#endif /* ESPHOME_DEBUG_SCHEDULER */

  LockGuard guard{this->lock_};

  // For retries, check if there's a cancelled timeout first
  if (is_retry && name_cstr != nullptr && type == SchedulerItem::TIMEOUT &&
      (has_cancelled_timeout_in_container_(this->items_, component, name_cstr, /* match_retry= */ true) ||
       has_cancelled_timeout_in_container_(this->to_add_, component, name_cstr, /* match_retry= */ true))) {
    // Skip scheduling - the retry was cancelled
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Skipping retry '%s' - found cancelled item", name_cstr);
#endif
    return;
  }

  // If name is provided, do atomic cancel-and-add (unless skip_cancel is true)
  // Cancel existing items
  if (!skip_cancel) {
    this->cancel_item_locked_(component, name_cstr, type);
  }
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

void retry_handler(const std::shared_ptr<RetryArgs> &args) {
  RetryResult const retry_result = args->func(--args->retry_countdown);
  if (retry_result == RetryResult::DONE || args->retry_countdown <= 0)
    return;
  // second execution of `func` happens after `initial_wait_time`
  args->scheduler->set_timer_common_(
      args->component, Scheduler::SchedulerItem::TIMEOUT, false, &args->name, args->current_interval,
      [args]() { retry_handler(args); }, /* is_retry= */ true);
  // backoff_increase_factor applied to third & later executions
  args->current_interval *= args->backoff_increase_factor;
}

void HOT Scheduler::set_retry_common_(Component *component, bool is_static_string, const void *name_ptr,
                                      uint32_t initial_wait_time, uint8_t max_attempts,
                                      std::function<RetryResult(uint8_t)> func, float backoff_increase_factor) {
  const char *name_cstr = this->get_name_cstr_(is_static_string, name_ptr);

  if (name_cstr != nullptr)
    this->cancel_retry(component, name_cstr);

  if (initial_wait_time == SCHEDULER_DONT_RUN)
    return;

  ESP_LOGVV(TAG, "set_retry(name='%s', initial_wait_time=%" PRIu32 ", max_attempts=%u, backoff_factor=%0.1f)",
            name_cstr ? name_cstr : "", initial_wait_time, max_attempts, backoff_increase_factor);

  if (backoff_increase_factor < 0.0001) {
    ESP_LOGE(TAG, "backoff_factor %0.1f too small, using 1.0: %s", backoff_increase_factor, name_cstr ? name_cstr : "");
    backoff_increase_factor = 1;
  }

  auto args = std::make_shared<RetryArgs>();
  args->func = std::move(func);
  args->retry_countdown = max_attempts;
  args->current_interval = initial_wait_time;
  args->component = component;
  args->name = name_cstr ? name_cstr : "";  // Convert to std::string for RetryArgs
  args->backoff_increase_factor = backoff_increase_factor;
  args->scheduler = this;

  // First execution of `func` immediately - use set_timer_common_ with is_retry=true
  this->set_timer_common_(
      component, SchedulerItem::TIMEOUT, false, &args->name, 0, [args]() { retry_handler(args); },
      /* is_retry= */ true);
}

void HOT Scheduler::set_retry(Component *component, const std::string &name, uint32_t initial_wait_time,
                              uint8_t max_attempts, std::function<RetryResult(uint8_t)> func,
                              float backoff_increase_factor) {
  this->set_retry_common_(component, false, &name, initial_wait_time, max_attempts, std::move(func),
                          backoff_increase_factor);
}

void HOT Scheduler::set_retry(Component *component, const char *name, uint32_t initial_wait_time, uint8_t max_attempts,
                              std::function<RetryResult(uint8_t)> func, float backoff_increase_factor) {
  this->set_retry_common_(component, true, name, initial_wait_time, max_attempts, std::move(func),
                          backoff_increase_factor);
}
bool HOT Scheduler::cancel_retry(Component *component, const std::string &name) {
  return this->cancel_retry(component, name.c_str());
}

bool HOT Scheduler::cancel_retry(Component *component, const char *name) {
  // Cancel timeouts that have is_retry flag set
  LockGuard guard{this->lock_};
  return this->cancel_item_locked_(component, name, SchedulerItem::TIMEOUT, /* match_retry= */ true);
}

optional<uint32_t> HOT Scheduler::next_schedule_in(uint32_t now) {
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // It performs cleanup and accesses items_[0] without holding a lock, which is only
  // safe when called from the main thread. Other threads must not call this method.

  // If no items, return empty optional
  if (this->cleanup_() == 0)
    return {};

  auto &item = this->items_[0];
  // Convert the fresh timestamp from caller (usually Application::loop()) to 64-bit
  const auto now_64 = this->millis_64_(now);  // 'now' from parameter - fresh from caller
  if (item->next_execution_ < now_64)
    return 0;
  return item->next_execution_ - now_64;
}
void HOT Scheduler::call(uint32_t now) {
#ifndef ESPHOME_THREAD_SINGLE
  // Process defer queue first to guarantee FIFO execution order for deferred items.
  // Previously, defer() used the heap which gave undefined order for equal timestamps,
  // causing race conditions on multi-core systems (ESP32, BK7200).
  // With the defer queue:
  // - Deferred items (delay=0) go directly to defer_queue_ in set_timer_common_
  // - Items execute in exact order they were deferred (FIFO guarantee)
  // - No deferred items exist in to_add_, so processing order doesn't affect correctness
  // Single-core platforms don't use this queue and fall back to the heap-based approach.
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
      this->execute_item_(item.get(), now);
    }
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Convert the fresh timestamp from main loop to 64-bit for scheduler operations
  const auto now_64 = this->millis_64_(now);  // 'now' from parameter - fresh from Application::loop()
  this->process_to_add();

#ifdef ESPHOME_DEBUG_SCHEDULER
  static uint64_t last_print = 0;

  if (now_64 - last_print > 2000) {
    last_print = now_64;
    std::vector<std::unique_ptr<SchedulerItem>> old_items;
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    const auto last_dbg = this->last_millis_.load(std::memory_order_relaxed);
    const auto major_dbg = this->millis_major_.load(std::memory_order_relaxed);
    ESP_LOGD(TAG, "Items: count=%zu, now=%" PRIu64 " (%" PRIu16 ", %" PRIu32 ")", this->items_.size(), now_64,
             major_dbg, last_dbg);
#else  /* not ESPHOME_THREAD_MULTI_ATOMICS */
    ESP_LOGD(TAG, "Items: count=%zu, now=%" PRIu64 " (%" PRIu16 ", %" PRIu32 ")", this->items_.size(), now_64,
             this->millis_major_, this->last_millis_);
#endif /* else ESPHOME_THREAD_MULTI_ATOMICS */
    // Cleanup before debug output
    this->cleanup_();
    while (!this->items_.empty()) {
      std::unique_ptr<SchedulerItem> item;
      {
        LockGuard guard{this->lock_};
        item = std::move(this->items_[0]);
        this->pop_raw_();
      }

      const char *name = item->get_name();
      ESP_LOGD(TAG, "  %s '%s/%s' interval=%" PRIu32 " next_execution in %" PRIu64 "ms at %" PRIu64,
               item->get_type_str(), item->get_source(), name ? name : "(null)", item->interval,
               item->next_execution_ - now_64, item->next_execution_);

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
#endif /* ESPHOME_DEBUG_SCHEDULER */

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

  // Cleanup removed items before processing
  this->cleanup_();
  while (!this->items_.empty()) {
    // use scoping to indicate visibility of `item` variable
    {
      // Don't copy-by value yet
      auto &item = this->items_[0];
      if (item->next_execution_ > now_64) {
        // Not reached timeout yet, done for this call
        break;
      }
      // Don't run on failed components
      if (item->component != nullptr && item->component->is_failed()) {
        LockGuard guard{this->lock_};
        this->pop_raw_();
        continue;
      }

      // Check if item is marked for removal
      // This handles two cases:
      // 1. Item was marked for removal after cleanup_() but before we got here
      // 2. Item is marked for removal but wasn't at the front of the heap during cleanup_()
#ifdef ESPHOME_THREAD_MULTI_NO_ATOMICS
      // Multi-threaded platforms without atomics: must take lock to safely read remove flag
      {
        LockGuard guard{this->lock_};
        if (is_item_removed_(item.get())) {
          this->pop_raw_();
          this->to_remove_--;
          continue;
        }
      }
#else
      // Single-threaded or multi-threaded with atomics: can check without lock
      if (is_item_removed_(item.get())) {
        LockGuard guard{this->lock_};
        this->pop_raw_();
        this->to_remove_--;
        continue;
      }
#endif

#ifdef ESPHOME_DEBUG_SCHEDULER
      const char *item_name = item->get_name();
      ESP_LOGV(TAG, "Running %s '%s/%s' with interval=%" PRIu32 " next_execution=%" PRIu64 " (now=%" PRIu64 ")",
               item->get_type_str(), item->get_source(), item_name ? item_name : "(null)", item->interval,
               item->next_execution_, now_64);
#endif /* ESPHOME_DEBUG_SCHEDULER */

      // Warning: During callback(), a lot of stuff can happen, including:
      //  - timeouts/intervals get added, potentially invalidating vector pointers
      //  - timeouts/intervals get cancelled
      this->execute_item_(item.get(), now);
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
        item->next_execution_ = now_64 + item->interval;
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
size_t HOT Scheduler::cleanup_() {
  // Fast path: if nothing to remove, just return the current size
  // Reading to_remove_ without lock is safe because:
  // 1. We only call this from the main thread during call()
  // 2. If it's 0, there's definitely nothing to cleanup
  // 3. If it becomes non-zero after we check, cleanup will happen on the next loop iteration
  // 4. Not all platforms support atomics, so we accept this race in favor of performance
  // 5. The worst case is a one-loop-iteration delay in cleanup, which is harmless
  if (this->to_remove_ == 0)
    return this->items_.size();

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
      break;
    this->to_remove_--;
    this->pop_raw_();
  }
  return this->items_.size();
}
void HOT Scheduler::pop_raw_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->items_.pop_back();
}

// Helper to execute a scheduler item
void HOT Scheduler::execute_item_(SchedulerItem *item, uint32_t now) {
  App.set_current_component(item->component);
  WarnIfComponentBlockingGuard guard{item->component, now};
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
bool HOT Scheduler::cancel_item_locked_(Component *component, const char *name_cstr, SchedulerItem::Type type,
                                        bool match_retry) {
  // Early return if name is invalid - no items to cancel
  if (name_cstr == nullptr) {
    return false;
  }

  size_t total_cancelled = 0;

  // Check all containers for matching items
#ifndef ESPHOME_THREAD_SINGLE
  // Only check defer queue for timeouts (intervals never go there)
  if (type == SchedulerItem::TIMEOUT) {
    for (auto &item : this->defer_queue_) {
      if (this->matches_item_(item, component, name_cstr, type, match_retry)) {
        this->mark_item_removed_(item.get());
        total_cancelled++;
      }
    }
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Cancel items in the main heap
  for (auto &item : this->items_) {
    if (this->matches_item_(item, component, name_cstr, type, match_retry)) {
      this->mark_item_removed_(item.get());
      total_cancelled++;
      this->to_remove_++;  // Track removals for heap items
    }
  }

  // Cancel items in to_add_
  for (auto &item : this->to_add_) {
    if (this->matches_item_(item, component, name_cstr, type, match_retry)) {
      this->mark_item_removed_(item.get());
      total_cancelled++;
      // Don't track removals for to_add_ items
    }
  }

  return total_cancelled > 0;
}

uint64_t Scheduler::millis_64_(uint32_t now) {
  // THREAD SAFETY NOTE:
  // This function has three implementations, based on the precompiler flags
  // - ESPHOME_THREAD_SINGLE - Runs on single-threaded platforms (ESP8266, RP2040, etc.)
  // - ESPHOME_THREAD_MULTI_NO_ATOMICS - Runs on multi-threaded platforms without atomics (LibreTiny)
  // - ESPHOME_THREAD_MULTI_ATOMICS - Runs on multi-threaded platforms with atomics (ESP32, HOST, etc.)
  //
  // Make sure all changes are synchronized if you edit this function.
  //
  // IMPORTANT: Always pass fresh millis() values to this function. The implementation
  // handles out-of-order timestamps between threads, but minimizing time differences
  // helps maintain accuracy.
  //

#ifdef ESPHOME_THREAD_SINGLE
  // This is the single core implementation.
  //
  // Single-core platforms have no concurrency, so this is a simple implementation
  // that just tracks 32-bit rollover (every 49.7 days) without any locking or atomics.

  uint16_t major = this->millis_major_;
  uint32_t last = this->last_millis_;

  // Check for rollover
  if (now < last && (last - now) > HALF_MAX_UINT32) {
    this->millis_major_++;
    major++;
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
  }

  // Only update if time moved forward
  if (now > last) {
    this->last_millis_ = now;
  }

  // Combine major (high 32 bits) and now (low 32 bits) into 64-bit time
  return now + (static_cast<uint64_t>(major) << 32);

#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
  // This is the multi core no atomics implementation.
  //
  // Without atomics, this implementation uses locks more aggressively:
  // 1. Always locks when near the rollover boundary (within 10 seconds)
  // 2. Always locks when detecting a large backwards jump
  // 3. Updates without lock in normal forward progression (accepting minor races)
  // This is less efficient but necessary without atomic operations.
  uint16_t major = this->millis_major_;
  uint32_t last = this->last_millis_;

  // Define a safe window around the rollover point (10 seconds)
  // This covers any reasonable scheduler delays or thread preemption
  static const uint32_t ROLLOVER_WINDOW = 10000;  // 10 seconds in milliseconds

  // Check if we're near the rollover boundary (close to std::numeric_limits<uint32_t>::max() or just past 0)
  bool near_rollover = (last > (std::numeric_limits<uint32_t>::max() - ROLLOVER_WINDOW)) || (now < ROLLOVER_WINDOW);

  if (near_rollover || (now < last && (last - now) > HALF_MAX_UINT32)) {
    // Near rollover or detected a rollover - need lock for safety
    LockGuard guard{this->lock_};
    // Re-read with lock held
    last = this->last_millis_;

    if (now < last && (last - now) > HALF_MAX_UINT32) {
      // True rollover detected (happens every ~49.7 days)
      this->millis_major_++;
      major++;
#ifdef ESPHOME_DEBUG_SCHEDULER
      ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
    }
    // Update last_millis_ while holding lock
    this->last_millis_ = now;
  } else if (now > last) {
    // Normal case: Not near rollover and time moved forward
    // Update without lock. While this may cause minor races (microseconds of
    // backwards time movement), they're acceptable because:
    // 1. The scheduler operates at millisecond resolution, not microsecond
    // 2. We've already prevented the critical rollover race condition
    // 3. Any backwards movement is orders of magnitude smaller than scheduler delays
    this->last_millis_ = now;
  }
  // If now <= last and we're not near rollover, don't update
  // This minimizes backwards time movement

  // Combine major (high 32 bits) and now (low 32 bits) into 64-bit time
  return now + (static_cast<uint64_t>(major) << 32);

#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
  // This is the multi core with atomics implementation.
  //
  // Uses atomic operations with acquire/release semantics to ensure coherent
  // reads of millis_major_ and last_millis_ across cores. Features:
  // 1. Epoch-coherency retry loop to handle concurrent updates
  // 2. Lock only taken for actual rollover detection and update
  // 3. Lock-free CAS updates for normal forward time progression
  // 4. Memory ordering ensures cores see consistent time values

  for (;;) {
    uint16_t major = this->millis_major_.load(std::memory_order_acquire);

    /*
     * Acquire so that if we later decide **not** to take the lock we still
     * observe a `millis_major_` value coherent with the loaded `last_millis_`.
     * The acquire load ensures any later read of `millis_major_` sees its
     * corresponding increment.
     */
    uint32_t last = this->last_millis_.load(std::memory_order_acquire);

    // If we might be near a rollover (large backwards jump), take the lock for the entire operation
    // This ensures rollover detection and last_millis_ update are atomic together
    if (now < last && (last - now) > HALF_MAX_UINT32) {
      // Potential rollover - need lock for atomic rollover detection + update
      LockGuard guard{this->lock_};
      // Re-read with lock held; mutex already provides ordering
      last = this->last_millis_.load(std::memory_order_relaxed);

      if (now < last && (last - now) > HALF_MAX_UINT32) {
        // True rollover detected (happens every ~49.7 days)
        this->millis_major_.fetch_add(1, std::memory_order_relaxed);
        major++;
#ifdef ESPHOME_DEBUG_SCHEDULER
        ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
      }
      /*
       * Update last_millis_ while holding the lock to prevent races
       * Publish the new low-word *after* bumping `millis_major_` (done above)
       * so readers never see a mismatched pair.
       */
      this->last_millis_.store(now, std::memory_order_release);
    } else {
      // Normal case: Try lock-free update, but only allow forward movement within same epoch
      // This prevents accidentally moving backwards across a rollover boundary
      while (now > last && (now - last) < HALF_MAX_UINT32) {
        if (this->last_millis_.compare_exchange_weak(last, now,
                                                     std::memory_order_release,     // success
                                                     std::memory_order_relaxed)) {  // failure
          break;
        }
        // CAS failure means no data was published; relaxed is fine
        // last is automatically updated by compare_exchange_weak if it fails
      }
    }
    uint16_t major_end = this->millis_major_.load(std::memory_order_relaxed);
    if (major_end == major)
      return now + (static_cast<uint64_t>(major) << 32);
  }
  // Unreachable - the loop always returns when major_end == major
  __builtin_unreachable();

#else
#error \
    "No platform threading model defined. One of ESPHOME_THREAD_SINGLE, ESPHOME_THREAD_MULTI_NO_ATOMICS, or ESPHOME_THREAD_MULTI_ATOMICS must be defined."
#endif
}

bool HOT Scheduler::SchedulerItem::cmp(const std::unique_ptr<SchedulerItem> &a,
                                       const std::unique_ptr<SchedulerItem> &b) {
  return a->next_execution_ > b->next_execution_;
}

}  // namespace esphome
