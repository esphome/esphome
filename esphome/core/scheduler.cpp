#include "scheduler.h"

#include "application.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <algorithm>
#include <cinttypes>
#include <cstring>

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
// max delay to start an interval sequence
static constexpr uint32_t MAX_INTERVAL_DELAY = 5000;

#if defined(ESPHOME_LOG_HAS_VERBOSE) || defined(ESPHOME_DEBUG_SCHEDULER)
// Helper struct for formatting scheduler item names consistently in logs
// Uses a stack buffer to avoid heap allocation
// Uses ESPHOME_snprintf_P/ESPHOME_PSTR for ESP8266 to keep format strings in flash
struct SchedulerNameLog {
  char buffer[20];  // Enough for "id:4294967295" or "hash:0xFFFFFFFF" or "(null)"

  // Format a scheduler item name for logging
  // Returns pointer to formatted string (either static_name or internal buffer)
  const char *format(Scheduler::NameType name_type, const char *static_name, uint32_t hash_or_id) {
    using NameType = Scheduler::NameType;
    if (name_type == NameType::STATIC_STRING) {
      if (static_name)
        return static_name;
      // Copy "(null)" to buffer to keep it in flash on ESP8266
      ESPHOME_strncpy_P(buffer, ESPHOME_PSTR("(null)"), sizeof(buffer));
      return buffer;
    } else if (name_type == NameType::HASHED_STRING) {
      ESPHOME_snprintf_P(buffer, sizeof(buffer), ESPHOME_PSTR("hash:0x%08" PRIX32), hash_or_id);
      return buffer;
    } else if (name_type == NameType::NUMERIC_ID) {
      ESPHOME_snprintf_P(buffer, sizeof(buffer), ESPHOME_PSTR("id:%" PRIu32), hash_or_id);
      return buffer;
    } else {  // NUMERIC_ID_INTERNAL
      ESPHOME_snprintf_P(buffer, sizeof(buffer), ESPHOME_PSTR("iid:%" PRIu32), hash_or_id);
      return buffer;
    }
  }
};
#endif

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

// Calculate random offset for interval timers
// Extracted from set_timer_common_ to reduce code size - only needed for intervals, not timeouts
uint32_t Scheduler::calculate_interval_offset_(uint32_t delay) {
  uint32_t max_offset = std::min(delay / 2, MAX_INTERVAL_DELAY);
  // Multiply-and-shift: uniform random in [0, max_offset) without floating point
  return static_cast<uint32_t>((static_cast<uint64_t>(random_uint32()) * max_offset) >> 32);
}

// Check if a retry was already cancelled in items_ or to_add_
// Extracted from set_timer_common_ to reduce code size - retry path is cold and deprecated
// Remove before 2026.8.0 along with all retry code
bool Scheduler::is_retry_cancelled_locked_(Component *component, NameType name_type, const char *static_name,
                                           uint32_t hash_or_id) {
  for (auto *container : {&this->items_, &this->to_add_}) {
    for (auto *item : *container) {
      if (item != nullptr && this->is_item_removed_locked_(item) &&
          this->matches_item_locked_(item, component, name_type, static_name, hash_or_id, SchedulerItem::TIMEOUT,
                                     /* match_retry= */ true, /* skip_removed= */ false)) {
        return true;
      }
    }
  }
  return false;
}

// Common implementation for both timeout and interval
// name_type determines storage type: STATIC_STRING uses static_name, others use hash_or_id
void HOT Scheduler::set_timer_common_(Component *component, SchedulerItem::Type type, NameType name_type,
                                      const char *static_name, uint32_t hash_or_id, uint32_t delay,
                                      std::function<void()> &&func, bool is_retry, bool skip_cancel) {
  if (delay == SCHEDULER_DONT_RUN) {
    // Still need to cancel existing timer if we have a name/id
    if (!skip_cancel) {
      LockGuard guard{this->lock_};
      this->cancel_item_locked_(component, name_type, static_name, hash_or_id, type);
    }
    return;
  }

  // Take lock early to protect scheduler_item_pool_ access and retry-cancelled check
  LockGuard guard{this->lock_};

  // For retries, check if there's a cancelled timeout first - before allocating an item.
  // Skip check for anonymous retries (STATIC_STRING with nullptr) - they can't be cancelled by name
  // Skip check for defer (delay=0) - deferred retries bypass the cancellation check
  if (is_retry && delay != 0 && (name_type != NameType::STATIC_STRING || static_name != nullptr) &&
      type == SchedulerItem::TIMEOUT &&
      this->is_retry_cancelled_locked_(component, name_type, static_name, hash_or_id)) {
#ifdef ESPHOME_DEBUG_SCHEDULER
    SchedulerNameLog skip_name_log;
    ESP_LOGD(TAG, "Skipping retry '%s' - found cancelled item",
             skip_name_log.format(name_type, static_name, hash_or_id));
#endif
    return;
  }

  // Create and populate the scheduler item
  SchedulerItem *item = this->get_item_from_pool_locked_();
  item->component = component;
  item->set_name(name_type, static_name, hash_or_id);
  item->type = type;
  item->callback = std::move(func);
  // Reset remove flag - recycled items may have been cancelled (remove=true) in previous use
  this->set_item_removed_(item, false);
  item->is_retry = is_retry;

  // Determine target container: defer_queue_ for deferred items, to_add_ for everything else.
  // Using a pointer lets both paths share the cancel + push_back epilogue.
  auto *target = &this->to_add_;

#ifndef ESPHOME_THREAD_SINGLE
  // Special handling for defer() (delay = 0, type = TIMEOUT)
  // Single-core platforms don't need thread-safe defer handling
  if (delay == 0 && type == SchedulerItem::TIMEOUT) {
    // Put in defer queue for guaranteed FIFO execution
    target = &this->defer_queue_;
  } else
#endif /* not ESPHOME_THREAD_SINGLE */
  {
    // Only non-defer items need a timestamp for scheduling
    const uint64_t now_64 = millis_64();

    // Type-specific setup
    if (type == SchedulerItem::INTERVAL) {
      item->interval = delay;
      // first execution happens immediately after a random smallish offset
      uint32_t offset = this->calculate_interval_offset_(delay);
      item->set_next_execution(now_64 + offset);
#ifdef ESPHOME_LOG_HAS_VERBOSE
      SchedulerNameLog name_log;
      ESP_LOGV(TAG, "Scheduler interval for %s is %" PRIu32 "ms, offset %" PRIu32 "ms",
               name_log.format(name_type, static_name, hash_or_id), delay, offset);
#endif
    } else {
      item->interval = 0;
      item->set_next_execution(now_64 + delay);
    }

#ifdef ESPHOME_DEBUG_SCHEDULER
    this->debug_log_timer_(item, name_type, static_name, hash_or_id, type, delay, now_64);
#endif /* ESPHOME_DEBUG_SCHEDULER */
  }

  // Common epilogue: atomic cancel-and-add (unless skip_cancel is true)
  if (!skip_cancel) {
    this->cancel_item_locked_(component, name_type, static_name, hash_or_id, type);
  }
  target->push_back(item);
}

void HOT Scheduler::set_timeout(Component *component, const char *name, uint32_t timeout,
                                std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, NameType::STATIC_STRING, name, 0, timeout,
                          std::move(func));
}

void HOT Scheduler::set_timeout(Component *component, const std::string &name, uint32_t timeout,
                                std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, NameType::HASHED_STRING, nullptr, fnv1a_hash(name),
                          timeout, std::move(func));
}
void HOT Scheduler::set_timeout(Component *component, uint32_t id, uint32_t timeout, std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::TIMEOUT, NameType::NUMERIC_ID, nullptr, id, timeout,
                          std::move(func));
}
bool HOT Scheduler::cancel_timeout(Component *component, const std::string &name) {
  return this->cancel_item_(component, NameType::HASHED_STRING, nullptr, fnv1a_hash(name), SchedulerItem::TIMEOUT);
}
bool HOT Scheduler::cancel_timeout(Component *component, const char *name) {
  return this->cancel_item_(component, NameType::STATIC_STRING, name, 0, SchedulerItem::TIMEOUT);
}
bool HOT Scheduler::cancel_timeout(Component *component, uint32_t id) {
  return this->cancel_item_(component, NameType::NUMERIC_ID, nullptr, id, SchedulerItem::TIMEOUT);
}
void HOT Scheduler::set_interval(Component *component, const std::string &name, uint32_t interval,
                                 std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::INTERVAL, NameType::HASHED_STRING, nullptr, fnv1a_hash(name),
                          interval, std::move(func));
}

void HOT Scheduler::set_interval(Component *component, const char *name, uint32_t interval,
                                 std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::INTERVAL, NameType::STATIC_STRING, name, 0, interval,
                          std::move(func));
}
void HOT Scheduler::set_interval(Component *component, uint32_t id, uint32_t interval, std::function<void()> &&func) {
  this->set_timer_common_(component, SchedulerItem::INTERVAL, NameType::NUMERIC_ID, nullptr, id, interval,
                          std::move(func));
}
bool HOT Scheduler::cancel_interval(Component *component, const std::string &name) {
  return this->cancel_item_(component, NameType::HASHED_STRING, nullptr, fnv1a_hash(name), SchedulerItem::INTERVAL);
}
bool HOT Scheduler::cancel_interval(Component *component, const char *name) {
  return this->cancel_item_(component, NameType::STATIC_STRING, name, 0, SchedulerItem::INTERVAL);
}
bool HOT Scheduler::cancel_interval(Component *component, uint32_t id) {
  return this->cancel_item_(component, NameType::NUMERIC_ID, nullptr, id, SchedulerItem::INTERVAL);
}

// Suppress deprecation warnings for RetryResult usage in the still-present (but deprecated) retry implementation.
// Remove before 2026.8.0 along with all retry code.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

struct RetryArgs {
  // Ordered to minimize padding on 32-bit systems
  std::function<RetryResult(uint8_t)> func;
  Component *component;
  Scheduler *scheduler;
  // Union for name storage - only one is used based on name_type
  union {
    const char *static_name;  // For STATIC_STRING
    uint32_t hash_or_id;      // For HASHED_STRING or NUMERIC_ID
  } name_;
  uint32_t current_interval;
  float backoff_increase_factor;
  Scheduler::NameType name_type;  // Discriminator for name_ union
  uint8_t retry_countdown;
};

void retry_handler(const std::shared_ptr<RetryArgs> &args) {
  RetryResult const retry_result = args->func(--args->retry_countdown);
  if (retry_result == RetryResult::DONE || args->retry_countdown <= 0)
    return;
  // second execution of `func` happens after `initial_wait_time`
  // args->name_ is owned by the shared_ptr<RetryArgs>
  // which is captured in the lambda and outlives the SchedulerItem
  const char *static_name = (args->name_type == Scheduler::NameType::STATIC_STRING) ? args->name_.static_name : nullptr;
  uint32_t hash_or_id = (args->name_type != Scheduler::NameType::STATIC_STRING) ? args->name_.hash_or_id : 0;
  args->scheduler->set_timer_common_(
      args->component, Scheduler::SchedulerItem::TIMEOUT, args->name_type, static_name, hash_or_id,
      args->current_interval, [args]() { retry_handler(args); },
      /* is_retry= */ true);
  // backoff_increase_factor applied to third & later executions
  args->current_interval *= args->backoff_increase_factor;
}

void HOT Scheduler::set_retry_common_(Component *component, NameType name_type, const char *static_name,
                                      uint32_t hash_or_id, uint32_t initial_wait_time, uint8_t max_attempts,
                                      std::function<RetryResult(uint8_t)> func, float backoff_increase_factor) {
  this->cancel_retry_(component, name_type, static_name, hash_or_id);

  if (initial_wait_time == SCHEDULER_DONT_RUN)
    return;

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  {
    SchedulerNameLog name_log;
    ESP_LOGVV(TAG, "set_retry(name='%s', initial_wait_time=%" PRIu32 ", max_attempts=%u, backoff_factor=%0.1f)",
              name_log.format(name_type, static_name, hash_or_id), initial_wait_time, max_attempts,
              backoff_increase_factor);
  }
#endif

  if (backoff_increase_factor < 0.0001) {
    ESP_LOGE(TAG, "set_retry: backoff_factor %0.1f too small, using 1.0: %s", backoff_increase_factor,
             (name_type == NameType::STATIC_STRING && static_name) ? static_name : "");
    backoff_increase_factor = 1;
  }

  auto args = std::make_shared<RetryArgs>();
  args->func = std::move(func);
  args->component = component;
  args->scheduler = this;
  args->name_type = name_type;
  if (name_type == NameType::STATIC_STRING) {
    args->name_.static_name = static_name;
  } else {
    args->name_.hash_or_id = hash_or_id;
  }
  args->current_interval = initial_wait_time;
  args->backoff_increase_factor = backoff_increase_factor;
  args->retry_countdown = max_attempts;

  // First execution of `func` immediately - use set_timer_common_ with is_retry=true
  this->set_timer_common_(
      component, SchedulerItem::TIMEOUT, name_type, static_name, hash_or_id, 0, [args]() { retry_handler(args); },
      /* is_retry= */ true);
}

void HOT Scheduler::set_retry(Component *component, const char *name, uint32_t initial_wait_time, uint8_t max_attempts,
                              std::function<RetryResult(uint8_t)> func, float backoff_increase_factor) {
  this->set_retry_common_(component, NameType::STATIC_STRING, name, 0, initial_wait_time, max_attempts, std::move(func),
                          backoff_increase_factor);
}

bool HOT Scheduler::cancel_retry_(Component *component, NameType name_type, const char *static_name,
                                  uint32_t hash_or_id) {
  return this->cancel_item_(component, name_type, static_name, hash_or_id, SchedulerItem::TIMEOUT,
                            /* match_retry= */ true);
}
bool HOT Scheduler::cancel_retry(Component *component, const char *name) {
  return this->cancel_retry_(component, NameType::STATIC_STRING, name, 0);
}

void HOT Scheduler::set_retry(Component *component, const std::string &name, uint32_t initial_wait_time,
                              uint8_t max_attempts, std::function<RetryResult(uint8_t)> func,
                              float backoff_increase_factor) {
  this->set_retry_common_(component, NameType::HASHED_STRING, nullptr, fnv1a_hash(name), initial_wait_time,
                          max_attempts, std::move(func), backoff_increase_factor);
}

bool HOT Scheduler::cancel_retry(Component *component, const std::string &name) {
  return this->cancel_retry_(component, NameType::HASHED_STRING, nullptr, fnv1a_hash(name));
}

void HOT Scheduler::set_retry(Component *component, uint32_t id, uint32_t initial_wait_time, uint8_t max_attempts,
                              std::function<RetryResult(uint8_t)> func, float backoff_increase_factor) {
  this->set_retry_common_(component, NameType::NUMERIC_ID, nullptr, id, initial_wait_time, max_attempts,
                          std::move(func), backoff_increase_factor);
}

bool HOT Scheduler::cancel_retry(Component *component, uint32_t id) {
  return this->cancel_retry_(component, NameType::NUMERIC_ID, nullptr, id);
}

#pragma GCC diagnostic pop  // End suppression of deprecated RetryResult warnings

optional<uint32_t> HOT Scheduler::next_schedule_in(uint32_t now) {
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // It performs cleanup and accesses items_[0] without holding a lock, which is only
  // safe when called from the main thread. Other threads must not call this method.

  // If no items, return empty optional
  if (this->cleanup_() == 0)
    return {};

  SchedulerItem *item = this->items_[0];
  const auto now_64 = this->millis_64_from_(now);
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

  // Compact in-place: move valid items forward, recycle removed ones
  size_t write = 0;
  for (size_t read = 0; read < this->items_.size(); ++read) {
    if (!is_item_removed_locked_(this->items_[read])) {
      if (write != read) {
        this->items_[write] = this->items_[read];
      }
      ++write;
    } else {
      this->recycle_item_main_loop_(this->items_[read]);
    }
  }
  this->items_.erase(this->items_.begin() + write, this->items_.end());
  // Rebuild the heap structure since items are no longer in heap order
  std::make_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);
  this->to_remove_ = 0;
}

#ifndef ESPHOME_THREAD_SINGLE
void Scheduler::compact_defer_queue_locked_() {
  // Rare case: new items were added during processing - compact the vector
  // This only happens when:
  // 1. A deferred callback calls defer() again, or
  // 2. Another thread calls defer() while we're processing
  //
  // Move unprocessed items (added during this loop) to the front for next iteration
  //
  // SAFETY: Compacted items may include cancelled items (marked for removal via
  // cancel_item_locked_() during execution). This is safe because should_skip_item_()
  // checks is_item_removed_() before executing, so cancelled items will be skipped
  // and recycled on the next loop iteration.
  size_t remaining = this->defer_queue_.size() - this->defer_queue_front_;
  for (size_t i = 0; i < remaining; i++) {
    this->defer_queue_[i] = this->defer_queue_[this->defer_queue_front_ + i];
  }
  // Use erase() instead of resize() to avoid instantiating _M_default_append
  // (saves ~156 bytes flash). Erasing from the end is O(1) - no shifting needed.
  this->defer_queue_.erase(this->defer_queue_.begin() + remaining, this->defer_queue_.end());
}
#endif /* not ESPHOME_THREAD_SINGLE */

void HOT Scheduler::call(uint32_t now) {
#ifndef ESPHOME_THREAD_SINGLE
  this->process_defer_queue_(now);
#endif /* not ESPHOME_THREAD_SINGLE */

  // Extend the caller's 32-bit timestamp to 64-bit for scheduler operations
  const auto now_64 = this->millis_64_from_(now);
  this->process_to_add();

  // Track if any items were added to to_add_ during this call (intervals or from callbacks)
  bool has_added_items = false;

#ifdef ESPHOME_DEBUG_SCHEDULER
  static uint64_t last_print = 0;

  if (now_64 - last_print > 2000) {
    last_print = now_64;
    std::vector<SchedulerItem *> old_items;
    ESP_LOGD(TAG, "Items: count=%zu, pool=%zu, now=%" PRIu64, this->items_.size(), this->scheduler_item_pool_.size(),
             now_64);
    // Cleanup before debug output
    this->cleanup_();
    while (!this->items_.empty()) {
      SchedulerItem *item;
      {
        LockGuard guard{this->lock_};
        item = this->pop_raw_locked_();
      }

      SchedulerNameLog name_log;
      bool is_cancelled = is_item_removed_(item);
      ESP_LOGD(TAG, "  %s '%s/%s' interval=%" PRIu32 " next_execution in %" PRIu64 "ms at %" PRIu64 "%s",
               item->get_type_str(), LOG_STR_ARG(item->get_source()),
               name_log.format(item->get_name_type(), item->get_name(), item->get_name_hash_or_id()), item->interval,
               item->get_next_execution() - now_64, item->get_next_execution(), is_cancelled ? " [CANCELLED]" : "");

      old_items.push_back(item);
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
    SchedulerItem *item = this->items_[0];
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
      if (is_item_removed_locked_(item)) {
        this->recycle_item_main_loop_(this->pop_raw_locked_());
        this->to_remove_--;
        continue;
      }
    }
#else
    // Single-threaded or multi-threaded with atomics: can check without lock
    if (is_item_removed_(item)) {
      LockGuard guard{this->lock_};
      this->recycle_item_main_loop_(this->pop_raw_locked_());
      this->to_remove_--;
      continue;
    }
#endif

#ifdef ESPHOME_DEBUG_SCHEDULER
    {
      SchedulerNameLog name_log;
      ESP_LOGV(TAG, "Running %s '%s/%s' with interval=%" PRIu32 " next_execution=%" PRIu64 " (now=%" PRIu64 ")",
               item->get_type_str(), LOG_STR_ARG(item->get_source()),
               name_log.format(item->get_name_type(), item->get_name(), item->get_name_hash_or_id()), item->interval,
               item->get_next_execution(), now_64);
    }
#endif /* ESPHOME_DEBUG_SCHEDULER */

    // Warning: During callback(), a lot of stuff can happen, including:
    //  - timeouts/intervals get added, potentially invalidating vector pointers
    //  - timeouts/intervals get cancelled
    now = this->execute_item_(item, now);

    LockGuard guard{this->lock_};

    // Only pop after function call, this ensures we were reachable
    // during the function call and know if we were cancelled.
    SchedulerItem *executed_item = this->pop_raw_locked_();

    if (this->is_item_removed_locked_(executed_item)) {
      // We were removed/cancelled in the function call, recycle and continue
      this->to_remove_--;
      this->recycle_item_main_loop_(executed_item);
      continue;
    }

    if (executed_item->type == SchedulerItem::INTERVAL) {
      executed_item->set_next_execution(now_64 + executed_item->interval);
      // Add new item directly to to_add_
      // since we have the lock held
      this->to_add_.push_back(executed_item);
    } else {
      // Timeout completed - recycle it
      this->recycle_item_main_loop_(executed_item);
    }

    has_added_items |= !this->to_add_.empty();
  }

  if (has_added_items) {
    this->process_to_add();
  }

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Verify no items were leaked during this call() cycle.
  // All items must be in items_, to_add_, defer_queue_, or the pool.
  // Safe to check here because:
  // - process_defer_queue_ has already run its cleanup_defer_queue_locked_(),
  //   so defer_queue_ contains no nullptr slots inflating the count.
  // - The while loop above has finished, so no items are held in local variables;
  //   every item has been returned to a container (items_, to_add_, or pool).
  // Lock needed to get a consistent snapshot of all containers.
  {
    LockGuard guard{this->lock_};
    this->debug_verify_no_leak_();
  }
#endif
}
void HOT Scheduler::process_to_add() {
  LockGuard guard{this->lock_};
  for (auto *&it : this->to_add_) {
    if (is_item_removed_locked_(it)) {
      // Recycle cancelled items
      this->recycle_item_main_loop_(it);
      it = nullptr;
      continue;
    }

    this->items_.push_back(it);
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
    SchedulerItem *item = this->items_[0];
    if (!this->is_item_removed_locked_(item))
      break;
    this->to_remove_--;
    this->recycle_item_main_loop_(this->pop_raw_locked_());
  }
  return this->items_.size();
}
Scheduler::SchedulerItem *HOT Scheduler::pop_raw_locked_() {
  std::pop_heap(this->items_.begin(), this->items_.end(), SchedulerItem::cmp);

  SchedulerItem *item = this->items_.back();
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

// Common implementation for cancel operations - handles locking
bool HOT Scheduler::cancel_item_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id,
                                 SchedulerItem::Type type, bool match_retry) {
  LockGuard guard{this->lock_};
  return this->cancel_item_locked_(component, name_type, static_name, hash_or_id, type, match_retry);
}

// Helper to cancel items - must be called with lock held
// name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
bool HOT Scheduler::cancel_item_locked_(Component *component, NameType name_type, const char *static_name,
                                        uint32_t hash_or_id, SchedulerItem::Type type, bool match_retry) {
  // Early return if static string name is invalid
  if (name_type == NameType::STATIC_STRING && static_name == nullptr) {
    return false;
  }

  size_t total_cancelled = 0;

#ifndef ESPHOME_THREAD_SINGLE
  // Mark items in defer queue as cancelled (they'll be skipped when processed)
  if (type == SchedulerItem::TIMEOUT) {
    total_cancelled += this->mark_matching_items_removed_locked_(this->defer_queue_, component, name_type, static_name,
                                                                 hash_or_id, type, match_retry);
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Cancel items in the main heap
  // We only mark items for removal here - never recycle directly.
  // The main loop may be executing an item's callback right now, and recycling
  // would destroy the callback while it's running (use-after-free).
  // Only the main loop in call() should recycle items after execution completes.
  if (!this->items_.empty()) {
    size_t heap_cancelled = this->mark_matching_items_removed_locked_(this->items_, component, name_type, static_name,
                                                                      hash_or_id, type, match_retry);
    total_cancelled += heap_cancelled;
    this->to_remove_ += heap_cancelled;
  }

  // Cancel items in to_add_
  total_cancelled += this->mark_matching_items_removed_locked_(this->to_add_, component, name_type, static_name,
                                                               hash_or_id, type, match_retry);

  return total_cancelled > 0;
}

bool HOT Scheduler::SchedulerItem::cmp(SchedulerItem *a, SchedulerItem *b) {
  // High bits are almost always equal (change only on 32-bit rollover ~49 days)
  // Optimize for common case: check low bits first when high bits are equal
  return (a->next_execution_high_ == b->next_execution_high_) ? (a->next_execution_low_ > b->next_execution_low_)
                                                              : (a->next_execution_high_ > b->next_execution_high_);
}

// Recycle a SchedulerItem back to the pool for reuse.
// IMPORTANT: Caller must hold the scheduler lock before calling this function.
// This protects scheduler_item_pool_ from concurrent access by other threads
// that may be acquiring items from the pool in set_timer_common_().
void Scheduler::recycle_item_main_loop_(SchedulerItem *item) {
  if (item == nullptr)
    return;

  if (this->scheduler_item_pool_.size() < MAX_POOL_SIZE) {
    // Clear callback to release captured resources
    item->callback = nullptr;
    this->scheduler_item_pool_.push_back(item);
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Recycled item to pool (pool size now: %zu)", this->scheduler_item_pool_.size());
#endif
  } else {
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Pool full (size: %zu), deleting item", this->scheduler_item_pool_.size());
#endif
    delete item;
#ifdef ESPHOME_DEBUG_SCHEDULER
    this->debug_live_items_--;
#endif
  }
}

#ifdef ESPHOME_DEBUG_SCHEDULER
void Scheduler::debug_log_timer_(const SchedulerItem *item, NameType name_type, const char *static_name,
                                 uint32_t hash_or_id, SchedulerItem::Type type, uint32_t delay, uint64_t now) {
  // Validate static strings in debug mode
  if (name_type == NameType::STATIC_STRING && static_name != nullptr) {
    validate_static_string(static_name);
  }

  // Debug logging
  SchedulerNameLog name_log;
  const char *type_str = (type == SchedulerItem::TIMEOUT) ? "timeout" : "interval";
  if (type == SchedulerItem::TIMEOUT) {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ")", type_str, LOG_STR_ARG(item->get_source()),
             name_log.format(name_type, static_name, hash_or_id), type_str, delay);
  } else {
    ESP_LOGD(TAG, "set_%s(name='%s/%s', %s=%" PRIu32 ", offset=%" PRIu32 ")", type_str, LOG_STR_ARG(item->get_source()),
             name_log.format(name_type, static_name, hash_or_id), type_str, delay,
             static_cast<uint32_t>(item->get_next_execution() - now));
  }
}
#endif /* ESPHOME_DEBUG_SCHEDULER */

// Helper to get or create a scheduler item from the pool
// IMPORTANT: Caller must hold the scheduler lock before calling this function.
Scheduler::SchedulerItem *Scheduler::get_item_from_pool_locked_() {
  if (!this->scheduler_item_pool_.empty()) {
    SchedulerItem *item = this->scheduler_item_pool_.back();
    this->scheduler_item_pool_.pop_back();
#ifdef ESPHOME_DEBUG_SCHEDULER
    ESP_LOGD(TAG, "Reused item from pool (pool size now: %zu)", this->scheduler_item_pool_.size());
#endif
    return item;
  }
#ifdef ESPHOME_DEBUG_SCHEDULER
  ESP_LOGD(TAG, "Allocated new item (pool empty)");
#endif
  auto *item = new SchedulerItem();
#ifdef ESPHOME_DEBUG_SCHEDULER
  this->debug_live_items_++;
#endif
  return item;
}

#ifdef ESPHOME_DEBUG_SCHEDULER
bool Scheduler::debug_verify_no_leak_() const {
  // Invariant: every live SchedulerItem must be in exactly one container.
  // debug_live_items_ tracks allocations minus deletions.
  size_t accounted = this->items_.size() + this->to_add_.size() + this->scheduler_item_pool_.size();
#ifndef ESPHOME_THREAD_SINGLE
  accounted += this->defer_queue_.size();
#endif
  if (accounted != this->debug_live_items_) {
    ESP_LOGE(TAG,
             "SCHEDULER LEAK DETECTED: live=%" PRIu32 " but accounted=%" PRIu32 " (items=%" PRIu32 " to_add=%" PRIu32
             " pool=%" PRIu32
#ifndef ESPHOME_THREAD_SINGLE
             " defer=%" PRIu32
#endif
             ")",
             static_cast<uint32_t>(this->debug_live_items_), static_cast<uint32_t>(accounted),
             static_cast<uint32_t>(this->items_.size()), static_cast<uint32_t>(this->to_add_.size()),
             static_cast<uint32_t>(this->scheduler_item_pool_.size())
#ifndef ESPHOME_THREAD_SINGLE
                 ,
             static_cast<uint32_t>(this->defer_queue_.size())
#endif
    );
    assert(false);
    return false;
  }
  return true;
}
#endif

}  // namespace esphome
