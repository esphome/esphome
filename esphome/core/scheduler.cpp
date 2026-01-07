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

// Memory pool configuration constants
// Pool size of 5 matches typical usage patterns (2-4 active timers)
// - Minimal memory overhead (~250 bytes on ESP32)
// - Sufficient for most configs with a couple sensors/components
// - Still prevents heap fragmentation and allocation stalls
// - Complex setups with many timers will just allocate beyond the pool
// See https://github.com/esphome/backlog/issues/52
static constexpr size_t MAX_POOL_SIZE = 5;

// Maximum number of logically deleted (cancelled) items before forcing cleanup.
// Set to 5 to match the pool size - when we have as many cancelled items as our
// pool can hold, it's time to clean up and recycle them.
static constexpr uint32_t MAX_LOGICALLY_DELETED_ITEMS = 5;
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

  // Get fresh timestamp BEFORE taking lock - millis_64_ may need to acquire lock itself
  const uint64_t now = this->millis_64_(millis());

  // Take lock early to protect scheduler_item_pool_ access
  LockGuard guard{this->lock_};

  // Create and populate the scheduler item
  std::unique_ptr<SchedulerItem> item;
  if (!this->scheduler_item_pool_.empty()) {
    // Reuse from pool
    item = std::move(this->scheduler_item_pool_.back());
    this->scheduler_item_pool_.pop_back();
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Reused item from pool (pool size now: %zu)", this->scheduler_item_pool_.size());
#endif
  } else {
    // Allocate new if pool is empty
    item = make_unique<SchedulerItem>();
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Allocated new item (pool empty)");
#endif
  }
  item->component = component;
  item->set_name(name_cstr, !is_static_string);
  item->type = type;
  item->callback = std::move(func);
  // Reset remove flag - recycled items may have been cancelled (remove=true) in previous use
  this->set_item_removed_(item.get(), false);
  item->is_retry = is_retry;

#ifndef ESPHOME_THREAD_SINGLE
  // Special handling for defer() (delay = 0, type = TIMEOUT)
  // Single-core platforms don't need thread-safe defer handling
  if (delay == 0 && type == SchedulerItem::TIMEOUT) {
    // Put in defer queue for guaranteed FIFO execution
    if (!skip_cancel) {
      this->cancel_item_locked_(component, name_cstr, type);
    }
    this->defer_queue_.push_back(std::move(item));
    return;
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Type-specific setup
  if (type == SchedulerItem::INTERVAL) {
    item->interval = delay;
    // first execution happens immediately after a random smallish offset
    // Calculate random offset (0 to min(interval/2, 5s))
    uint32_t offset = (uint32_t) (std::min(delay / 2, MAX_INTERVAL_DELAY) * random_float());
    item->set_next_execution(now + offset);
    ESP_LOGV(TAG, "Scheduler interval for %s is %" PRIu32 "ms, offset %" PRIu32 "ms", name_cstr ? name_cstr : "", delay,
             offset);
  } else {
    item->interval = 0;
    item->set_next_execution(now + delay);
  }

#ifdef ESPHOME_DEBUG_SCHEDULER
  this->debug_log_timer_(item.get(), is_static_string, name_cstr, type, delay, now);
#endif /* ESPHOME_DEBUG_SCHEDULER */

  // For retries, check if there's a cancelled timeout first
  if (is_retry && name_cstr != nullptr && type == SchedulerItem::TIMEOUT &&
      (has_cancelled_timeout_in_container_locked_(this->items_, component, name_cstr, /* match_retry= */ true) ||
       has_cancelled_timeout_in_container_locked_(this->to_add_, component, name_cstr, /* match_retry= */ true))) {
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
  // Ordered to minimize padding on 32-bit systems
  std::function<RetryResult(uint8_t)> func;
  Component *component;
  Scheduler *scheduler;
  const char *name;  // Points to static string or owned copy
  uint32_t current_interval;
  float backoff_increase_factor;
  uint8_t retry_countdown;
  bool name_is_dynamic;  // True if name needs delete[]

  ~RetryArgs() {
    if (this->name_is_dynamic && this->name) {
      delete[] this->name;
    }
  }
};

void retry_handler(const std::shared_ptr<RetryArgs> &args) {
  RetryResult const retry_result = args->func(--args->retry_countdown);
  if (retry_result == RetryResult::DONE || args->retry_countdown <= 0)
    return;
  // second execution of `func` happens after `initial_wait_time`
  // Pass is_static_string=true because args->name is owned by the shared_ptr<RetryArgs>
  // which is captured in the lambda and outlives the SchedulerItem
  args->scheduler->set_timer_common_(
      args->component, Scheduler::SchedulerItem::TIMEOUT, true, args->name, args->current_interval,
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
  args->component = component;
  args->scheduler = this;
  args->current_interval = initial_wait_time;
  args->backoff_increase_factor = backoff_increase_factor;
  args->retry_countdown = max_attempts;

  // Store name - either as static pointer or owned copy
  if (name_cstr == nullptr || name_cstr[0] == '\0') {
    // Empty or null name - use empty string literal
    args->name = "";
    args->name_is_dynamic = false;
  } else if (is_static_string) {
    // Static string - just store the pointer
    args->name = name_cstr;
    args->name_is_dynamic = false;
  } else {
    // Dynamic string - make a copy
    size_t len = strlen(name_cstr);
    char *copy = new char[len + 1];
    memcpy(copy, name_cstr, len + 1);
    args->name = copy;
    args->name_is_dynamic = true;
  }

  // First execution of `func` immediately - use set_timer_common_ with is_retry=true
  // Pass is_static_string=true because args->name is owned by the shared_ptr<RetryArgs>
  // which is captured in the lambda and outlives the SchedulerItem
  this->set_timer_common_(
      component, SchedulerItem::TIMEOUT, true, args->name, 0, [args]() { retry_handler(args); },
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
  const uint64_t next_exec = item->get_next_execution();
  if (next_exec < now_64)
    return 0;
  return next_exec - now_64;
}

void Scheduler::full_cleanup_removed_items_() {
  // We hold the lock for the entire cleanup operation because:
  // 1. We're rebuilding the entire items_ list, so we need exclusive access throughout
  // 2. Other threads must see either the old state or the new state, not intermediate states
  // 3. The operation is already expensive (O(n)), so lock overhead is negligible
  // 4. No operations inside can block or take other locks, so no deadlock risk
  LockGuard guard{this->lock_};

  std::vector<std::unique_ptr<SchedulerItem>> valid_items;

  // Move all non-removed items to valid_items, recycle removed ones
  for (auto &item : this->items_) {
    if (!is_item_removed_(item.get())) {
      valid_items.push_back(std::move(item));
    } else {
      // Recycle removed items
      this->recycle_item_main_loop_(std::move(item));
    }
  }

  // Replace items_ with the filtered list
  this->items_ = std::move(valid_items);
  // Rebuild the heap structure since items are no longer in heap order
  std::make_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->to_remove_ = 0;
}

void HOT Scheduler::call(uint32_t now) {
#ifndef ESPHOME_THREAD_SINGLE
  this->process_defer_queue_(now);
#endif /* not ESPHOME_THREAD_SINGLE */

  // Convert the fresh timestamp from main loop to 64-bit for scheduler operations
  const auto now_64 = this->millis_64_(now);  // 'now' from parameter - fresh from Application::loop()
  this->process_to_add();

  // Track if any items were added to to_add_ during this call (intervals or from callbacks)
  bool has_added_items = false;

#ifdef ESPHOME_DEBUG_SCHEDULER
  static uint64_t last_print = 0;

  if (now_64 - last_print > 2000) {
    last_print = now_64;
    std::vector<std::unique_ptr<SchedulerItem>> old_items;
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    const auto last_dbg = this->last_millis_.load(std::memory_order_relaxed);
    const auto major_dbg = this->millis_major_.load(std::memory_order_relaxed);
    ESP_LOGD(TAG, "Items: count=%zu, pool=%zu, now=%" PRIu64 " (%" PRIu16 ", %" PRIu32 ")", this->items_.size(),
             this->scheduler_item_pool_.size(), now_64, major_dbg, last_dbg);
#else  /* not ESPHOME_THREAD_MULTI_ATOMICS */
    ESP_LOGD(TAG, "Items: count=%zu, pool=%zu, now=%" PRIu64 " (%" PRIu16 ", %" PRIu32 ")", this->items_.size(),
             this->scheduler_item_pool_.size(), now_64, this->millis_major_, this->last_millis_);
#endif /* else ESPHOME_THREAD_MULTI_ATOMICS */
    // Cleanup before debug output
    this->cleanup_();
    while (!this->items_.empty()) {
      std::unique_ptr<SchedulerItem> item;
      {
        LockGuard guard{this->lock_};
        item = this->pop_raw_locked_();
      }

      const char *name = item->get_name();
      bool is_cancelled = is_item_removed_(item.get());
      ESP_LOGD(TAG, "  %s '%s/%s' interval=%" PRIu32 " next_execution in %" PRIu64 "ms at %" PRIu64 "%s",
               item->get_type_str(), LOG_STR_ARG(item->get_source()), name ? name : "(null)", item->interval,
               item->get_next_execution() - now_64, item->get_next_execution(), is_cancelled ? " [CANCELLED]" : "");

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

  // Cleanup removed items before processing
  // First try to clean items from the top of the heap (fast path)
  this->cleanup_();

  // If we still have too many cancelled items, do a full cleanup
  // This only happens if cancelled items are stuck in the middle/bottom of the heap
  if (this->to_remove_ >= MAX_LOGICALLY_DELETED_ITEMS) {
    this->full_cleanup_removed_items_();
  }
  while (!this->items_.empty()) {
    // Don't copy-by value yet
    auto &item = this->items_[0];
    if (item->get_next_execution() > now_64) {
      // Not reached timeout yet, done for this call
      break;
    }
    // Don't run on failed components
    if (item->component != nullptr && item->component->is_failed()) {
      LockGuard guard{this->lock_};
      this->recycle_item_main_loop_(this->pop_raw_locked_());
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
        this->recycle_item_main_loop_(this->pop_raw_locked_());
        this->to_remove_--;
        continue;
      }
    }
#else
    // Single-threaded or multi-threaded with atomics: can check without lock
    if (is_item_removed_(item.get())) {
      LockGuard guard{this->lock_};
      this->recycle_item_main_loop_(this->pop_raw_locked_());
      this->to_remove_--;
      continue;
    }
#endif

#ifdef ESPHOME_DEBUG_SCHEDULER
    const char *item_name = item->get_name();
    ESP_LOGV(TAG, "Running %s '%s/%s' with interval=%" PRIu32 " next_execution=%" PRIu64 " (now=%" PRIu64 ")",
             item->get_type_str(), LOG_STR_ARG(item->get_source()), item_name ? item_name : "(null)", item->interval,
             item->get_next_execution(), now_64);
#endif /* ESPHOME_DEBUG_SCHEDULER */

    // Warning: During callback(), a lot of stuff can happen, including:
    //  - timeouts/intervals get added, potentially invalidating vector pointers
    //  - timeouts/intervals get cancelled
    now = this->execute_item_(item.get(), now);

    LockGuard guard{this->lock_};

    // Only pop after function call, this ensures we were reachable
    // during the function call and know if we were cancelled.
    auto executed_item = this->pop_raw_locked_();

    if (executed_item->remove) {
      // We were removed/cancelled in the function call, recycle and continue
      this->to_remove_--;
      this->recycle_item_main_loop_(std::move(executed_item));
      continue;
    }

    if (executed_item->type == SchedulerItem::INTERVAL) {
      executed_item->set_next_execution(now_64 + executed_item->interval);
      // Add new item directly to to_add_
      // since we have the lock held
      this->to_add_.push_back(std::move(executed_item));
    } else {
      // Timeout completed - recycle it
      this->recycle_item_main_loop_(std::move(executed_item));
    }

    has_added_items |= !this->to_add_.empty();
  }

  if (has_added_items) {
    this->process_to_add();
  }
}
void HOT Scheduler::process_to_add() {
  LockGuard guard{this->lock_};
  for (auto &it : this->to_add_) {
    if (is_item_removed_(it.get())) {
      // Recycle cancelled items
      this->recycle_item_main_loop_(std::move(it));
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
  // 1. We're modifying items_ (via pop_raw_locked_) which requires exclusive access
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
    this->recycle_item_main_loop_(this->pop_raw_locked_());
  }
  return this->items_.size();
}
std::unique_ptr<Scheduler::SchedulerItem> HOT Scheduler::pop_raw_locked_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);

  // Move the item out before popping - this is the item that was at the front of the heap
  auto item = std::move(this->items_.back());

  this->items_.pop_back();
  return item;
}

// Helper to execute a scheduler item
uint32_t HOT Scheduler::execute_item_(SchedulerItem *item, uint32_t now) {
  App.set_current_component(item->component);
  WarnIfComponentBlockingGuard guard{item->component, now};
  item->callback();
  return guard.finish();
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
  // Mark items in defer queue as cancelled (they'll be skipped when processed)
  if (type == SchedulerItem::TIMEOUT) {
    total_cancelled +=
        this->mark_matching_items_removed_locked_(this->defer_queue_, component, name_cstr, type, match_retry);
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Cancel items in the main heap
  // We only mark items for removal here - never recycle directly.
  // The main loop may be executing an item's callback right now, and recycling
  // would destroy the callback while it's running (use-after-free).
  // Only the main loop in call() should recycle items after execution completes.
  if (!this->items_.empty()) {
    size_t heap_cancelled =
        this->mark_matching_items_removed_locked_(this->items_, component, name_cstr, type, match_retry);
    total_cancelled += heap_cancelled;
    this->to_remove_ += heap_cancelled;
  }

  // Cancel items in to_add_
  total_cancelled += this->mark_matching_items_removed_locked_(this->to_add_, component, name_cstr, type, match_retry);

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
    this->last_millis_ = now;
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Detected true 32-bit rollover at %" PRIu32 "ms (was %" PRIu32 ")", now, last);
#endif /* ESPHOME_DEBUG_SCHEDULER */
  } else if (now > last) {
    // Only update if time moved forward
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
  // High bits are almost always equal (change only on 32-bit rollover ~49 days)
  // Optimize for common case: check low bits first when high bits are equal
  return (a->next_execution_high_ == b->next_execution_high_) ? (a->next_execution_low_ > b->next_execution_low_)
                                                              : (a->next_execution_high_ > b->next_execution_high_);
}

// Recycle a SchedulerItem back to the pool for reuse.
// IMPORTANT: Caller must hold the scheduler lock before calling this function.
// This protects scheduler_item_pool_ from concurrent access by other threads
// that may be acquiring items from the pool in set_timer_common_().
void Scheduler::recycle_item_main_loop_(std::unique_ptr<SchedulerItem> item) {
  if (!item)
    return;

  if (this->scheduler_item_pool_.size() < MAX_POOL_SIZE) {
    // Clear callback to release captured resources
    item->callback = nullptr;
    // Clear dynamic name if any
    item->clear_dynamic_name();
    this->scheduler_item_pool_.push_back(std::move(item));
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Recycled item to pool (pool size now: %zu)", this->scheduler_item_pool_.size());
#endif
  } else {
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Pool full (size: %zu), deleting item", this->scheduler_item_pool_.size());
#endif
  }
  // else: unique_ptr will delete the item when it goes out of scope
}

#ifdef ESPHOME_DEBUG_SCHEDULER
void Scheduler::debug_log_timer_(const SchedulerItem *item, bool is_static_string, const char *name_cstr,
                                 SchedulerItem::Type type, uint32_t delay, uint64_t now) {
  // Validate static strings in debug mode
  if (is_static_string && name_cstr != nullptr) {
    validate_static_string(name_cstr);
  }

  // Debug logging
  const char *type_str = (type == SchedulerItem::TIMEOUT) ? "timeout" : "interval";
  if (type == SchedulerItem::TIMEOUT) {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ")", type_str, LOG_STR_ARG(item->get_source()),
             name_cstr ? name_cstr : "(null)", type_str, delay);
  } else {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ", offset=%" PRIu32 ")", type_str, LOG_STR_ARG(item->get_source()),
             name_cstr ? name_cstr : "(null)", type_str, delay,
             static_cast<uint32_t>(item->get_next_execution() - now));
  }
}
#endif /* ESPHOME_DEBUG_SCHEDULER */

}  // namespace esphome
