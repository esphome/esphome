#pragma once

#include "esphome/core/defines.h"
#include <vector>
#include <memory>
#include <cstring>
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
#endif

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {

class Component;
struct RetryArgs;

// Forward declaration of retry_handler - needs to be non-static for friend declaration
void retry_handler(const std::shared_ptr<RetryArgs> &args);

class Scheduler {
  // Allow retry_handler to access protected members for internal retry mechanism
  friend void ::esphome::retry_handler(const std::shared_ptr<RetryArgs> &args);
  // Allow DelayAction to call set_timer_common_ with skip_cancel=true for parallel script delays.
  // This is needed to fix issue #10264 where parallel scripts with delays interfere with each other.
  // We use friend instead of a public API because skip_cancel is dangerous - it can cause delays
  // to accumulate and overload the scheduler if misused.
  template<typename... Ts> friend class DelayAction;

 public:
  // Public API - accepts std::string for backward compatibility
  void set_timeout(Component *component, const std::string &name, uint32_t timeout, std::function<void()> func);

  /** Set a timeout with a const char* name.
   *
   * IMPORTANT: The provided name pointer must remain valid for the lifetime of the scheduler item.
   * This means the name should be:
   *   - A string literal (e.g., "update")
   *   - A static const char* variable
   *   - A pointer with lifetime >= the scheduled task
   *
   * For dynamic strings, use the std::string overload instead.
   */
  void set_timeout(Component *component, const char *name, uint32_t timeout, std::function<void()> func);

  bool cancel_timeout(Component *component, const std::string &name);
  bool cancel_timeout(Component *component, const char *name);

  void set_interval(Component *component, const std::string &name, uint32_t interval, std::function<void()> func);

  /** Set an interval with a const char* name.
   *
   * IMPORTANT: The provided name pointer must remain valid for the lifetime of the scheduler item.
   * This means the name should be:
   *   - A string literal (e.g., "update")
   *   - A static const char* variable
   *   - A pointer with lifetime >= the scheduled task
   *
   * For dynamic strings, use the std::string overload instead.
   */
  void set_interval(Component *component, const char *name, uint32_t interval, std::function<void()> func);

  bool cancel_interval(Component *component, const std::string &name);
  bool cancel_interval(Component *component, const char *name);
  void set_retry(Component *component, const std::string &name, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);
  void set_retry(Component *component, const char *name, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);
  bool cancel_retry(Component *component, const std::string &name);
  bool cancel_retry(Component *component, const char *name);

  // Calculate when the next scheduled item should run
  // @param now Fresh timestamp from millis() - must not be stale/cached
  // Returns the time in milliseconds until the next scheduled item, or nullopt if no items
  // This method performs cleanup of removed items before checking the schedule
  // IMPORTANT: This method should only be called from the main thread (loop task).
  optional<uint32_t> next_schedule_in(uint32_t now);

  // Execute all scheduled items that are ready
  // @param now Fresh timestamp from millis() - must not be stale/cached
  void call(uint32_t now);

  void process_to_add();

 protected:
  struct SchedulerItem {
    // Ordered by size to minimize padding
    Component *component;
    // Optimized name storage using tagged union
    union {
      const char *static_name;  // For string literals (no allocation)
      char *dynamic_name;       // For allocated strings
    } name_;
    uint32_t interval;
    // Split time to handle millis() rollover. The scheduler combines the 32-bit millis()
    // with a 16-bit rollover counter to create a 48-bit time space (using 32+16 bits).
    // This is intentionally limited to 48 bits, not stored as a full 64-bit value.
    // With 49.7 days per 32-bit rollover, the 16-bit counter supports
    // 49.7 days × 65536 = ~8900 years. This ensures correct scheduling
    // even when devices run for months. Split into two fields for better memory
    // alignment on 32-bit systems.
    uint32_t next_execution_low_;  // Lower 32 bits of execution time (millis value)
    std::function<void()> callback;
    uint16_t next_execution_high_;  // Upper 16 bits (millis_major counter)

#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    // Multi-threaded with atomics: use atomic for lock-free access
    // Place atomic<bool> separately since it can't be packed with bit fields
    std::atomic<bool> remove{false};

    // Bit-packed fields (3 bits used, 5 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    bool name_is_dynamic : 1;  // True if name was dynamically allocated (needs delete[])
    bool is_retry : 1;         // True if this is a retry timeout
                               // 5 bits padding
#else
    // Single-threaded or multi-threaded without atomics: can pack all fields together
    // Bit-packed fields (4 bits used, 4 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    bool remove : 1;
    bool name_is_dynamic : 1;  // True if name was dynamically allocated (needs delete[])
    bool is_retry : 1;         // True if this is a retry timeout
                               // 4 bits padding
#endif

    // Constructor
    SchedulerItem()
        : component(nullptr),
          interval(0),
          next_execution_low_(0),
          next_execution_high_(0),
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
          // remove is initialized in the member declaration as std::atomic<bool>{false}
          type(TIMEOUT),
          name_is_dynamic(false),
          is_retry(false) {
#else
          type(TIMEOUT),
          remove(false),
          name_is_dynamic(false),
          is_retry(false) {
#endif
      name_.static_name = nullptr;
    }

    // Destructor to clean up dynamic names
    ~SchedulerItem() { clear_dynamic_name(); }

    // Delete copy operations to prevent accidental copies
    SchedulerItem(const SchedulerItem &) = delete;
    SchedulerItem &operator=(const SchedulerItem &) = delete;

    // Delete move operations: SchedulerItem objects are only managed via unique_ptr, never moved directly
    SchedulerItem(SchedulerItem &&) = delete;
    SchedulerItem &operator=(SchedulerItem &&) = delete;

    // Helper to get the name regardless of storage type
    const char *get_name() const { return name_is_dynamic ? name_.dynamic_name : name_.static_name; }

    // Helper to clear dynamic name if allocated
    void clear_dynamic_name() {
      if (name_is_dynamic && name_.dynamic_name) {
        delete[] name_.dynamic_name;
        name_.dynamic_name = nullptr;
        name_is_dynamic = false;
      }
    }

    // Helper to set name with proper ownership
    void set_name(const char *name, bool make_copy = false) {
      // Clean up old dynamic name if any
      clear_dynamic_name();

      if (!name) {
        // nullptr case - no name provided
        name_.static_name = nullptr;
      } else if (make_copy) {
        // Make a copy for dynamic strings (including empty strings)
        size_t len = strlen(name);
        name_.dynamic_name = new char[len + 1];
        memcpy(name_.dynamic_name, name, len + 1);
        name_is_dynamic = true;
      } else {
        // Use static string directly (including empty strings)
        name_.static_name = name;
      }
    }

    static bool cmp(const std::unique_ptr<SchedulerItem> &a, const std::unique_ptr<SchedulerItem> &b);

    // Note: We use 48 bits total (32 + 16), stored in a 64-bit value for API compatibility.
    // The upper 16 bits of the 64-bit value are always zero, which is fine since
    // millis_major_ is also 16 bits and they must match.
    constexpr uint64_t get_next_execution() const {
      return (static_cast<uint64_t>(next_execution_high_) << 32) | next_execution_low_;
    }

    constexpr void set_next_execution(uint64_t value) {
      next_execution_low_ = static_cast<uint32_t>(value);
      // Cast to uint16_t intentionally truncates to lower 16 bits of the upper 32 bits.
      // This is correct because millis_major_ that creates these values is also 16 bits.
      next_execution_high_ = static_cast<uint16_t>(value >> 32);
    }
    constexpr const char *get_type_str() const { return (type == TIMEOUT) ? "timeout" : "interval"; }
    const LogString *get_source() const { return component ? component->get_component_log_str() : LOG_STR("unknown"); }
  };

  // Common implementation for both timeout and interval
  void set_timer_common_(Component *component, SchedulerItem::Type type, bool is_static_string, const void *name_ptr,
                         uint32_t delay, std::function<void()> func, bool is_retry = false, bool skip_cancel = false);

  // Common implementation for retry
  void set_retry_common_(Component *component, bool is_static_string, const void *name_ptr, uint32_t initial_wait_time,
                         uint8_t max_attempts, std::function<RetryResult(uint8_t)> func, float backoff_increase_factor);

  uint64_t millis_64_(uint32_t now);
  // Cleanup logically deleted items from the scheduler
  // Returns the number of items remaining after cleanup
  // IMPORTANT: This method should only be called from the main thread (loop task).
  size_t cleanup_();
  // Remove and return the front item from the heap
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  std::unique_ptr<SchedulerItem> pop_raw_locked_();

 private:
  // Helper to cancel items by name - must be called with lock held
  bool cancel_item_locked_(Component *component, const char *name, SchedulerItem::Type type, bool match_retry = false);

  // Helper to extract name as const char* from either static string or std::string
  inline const char *get_name_cstr_(bool is_static_string, const void *name_ptr) {
    return is_static_string ? static_cast<const char *>(name_ptr) : static_cast<const std::string *>(name_ptr)->c_str();
  }

  // Common implementation for cancel operations
  bool cancel_item_(Component *component, bool is_static_string, const void *name_ptr, SchedulerItem::Type type);

  // Helper to check if two scheduler item names match
  inline bool HOT names_match_(const char *name1, const char *name2) const {
    // Check pointer equality first (common for static strings), then string contents
    // The core ESPHome codebase uses static strings (const char*) for component names,
    // making pointer comparison effective. The std::string overloads exist only for
    // compatibility with external components but are rarely used in practice.
    return (name1 != nullptr && name2 != nullptr) && ((name1 == name2) || (strcmp(name1, name2) == 0));
  }

  // Helper function to check if item matches criteria for cancellation
  // IMPORTANT: Must be called with scheduler lock held
  inline bool HOT matches_item_locked_(const std::unique_ptr<SchedulerItem> &item, Component *component,
                                       const char *name_cstr, SchedulerItem::Type type, bool match_retry,
                                       bool skip_removed = true) const {
    // THREAD SAFETY: Check for nullptr first to prevent LoadProhibited crashes. On multi-threaded
    // platforms, items can be moved out of defer_queue_ during processing, leaving nullptr entries.
    // PR #11305 added nullptr checks in callers (mark_matching_items_removed_locked_() and
    // has_cancelled_timeout_in_container_locked_()), but this check provides defense-in-depth: helper
    // functions should be safe regardless of caller behavior.
    // Fixes: https://github.com/esphome/esphome/issues/11940
    if (!item)
      return false;
    if (item->component != component || item->type != type || (skip_removed && item->remove) ||
        (match_retry && !item->is_retry)) {
      return false;
    }
    return this->names_match_(item->get_name(), name_cstr);
  }

  // Helper to execute a scheduler item
  uint32_t execute_item_(SchedulerItem *item, uint32_t now);

  // Helper to check if item should be skipped
  bool should_skip_item_(SchedulerItem *item) const {
    return is_item_removed_(item) || (item->component != nullptr && item->component->is_failed());
  }

  // Helper to recycle a SchedulerItem back to the pool.
  // IMPORTANT: Only call from main loop context! Recycling clears the callback,
  // so calling from another thread while the callback is executing causes use-after-free.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  void recycle_item_main_loop_(std::unique_ptr<SchedulerItem> item);

  // Helper to perform full cleanup when too many items are cancelled
  void full_cleanup_removed_items_();

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Helper for debug logging in set_timer_common_ - extracted to reduce code size
  void debug_log_timer_(const SchedulerItem *item, bool is_static_string, const char *name_cstr,
                        SchedulerItem::Type type, uint32_t delay, uint64_t now);
#endif /* ESPHOME_DEBUG_SCHEDULER */

#ifndef ESPHOME_THREAD_SINGLE
  // Helper to process defer queue - inline for performance in hot path
  inline void process_defer_queue_(uint32_t &now) {
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
    // processed here. They are skipped during execution by should_skip_item_().
    // This is intentional - no memory leak occurs.
    //
    // We use an index (defer_queue_front_) to track the read position instead of calling
    // erase() on every pop, which would be O(n). The queue is processed once per loop -
    // any items added during processing are left for the next loop iteration.

    // Snapshot the queue end point - only process items that existed at loop start
    // Items added during processing (by callbacks or other threads) run next loop
    // No lock needed: single consumer (main loop), stale read just means we process less this iteration
    size_t defer_queue_end = this->defer_queue_.size();

    while (this->defer_queue_front_ < defer_queue_end) {
      std::unique_ptr<SchedulerItem> item;
      {
        LockGuard lock(this->lock_);
        // SAFETY: Moving out the unique_ptr leaves a nullptr in the vector at defer_queue_front_.
        // This is intentional and safe because:
        // 1. The vector is only cleaned up by cleanup_defer_queue_locked_() at the end of this function
        // 2. Any code iterating defer_queue_ MUST check for nullptr items (see mark_matching_items_removed_locked_
        //    and has_cancelled_timeout_in_container_locked_ in scheduler.h)
        // 3. The lock protects concurrent access, but the nullptr remains until cleanup
        item = std::move(this->defer_queue_[this->defer_queue_front_]);
        this->defer_queue_front_++;
      }

      // Execute callback without holding lock to prevent deadlocks
      // if the callback tries to call defer() again
      if (!this->should_skip_item_(item.get())) {
        now = this->execute_item_(item.get(), now);
      }
      // Recycle the defer item after execution
      {
        LockGuard lock(this->lock_);
        this->recycle_item_main_loop_(std::move(item));
      }
    }

    // If we've consumed all items up to the snapshot point, clean up the dead space
    // Single consumer (main loop), so no lock needed for this check
    if (this->defer_queue_front_ >= defer_queue_end) {
      LockGuard lock(this->lock_);
      this->cleanup_defer_queue_locked_();
    }
  }

  // Helper to cleanup defer_queue_ after processing
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  inline void cleanup_defer_queue_locked_() {
    // Check if new items were added by producers during processing
    if (this->defer_queue_front_ >= this->defer_queue_.size()) {
      // Common case: no new items - clear everything
      this->defer_queue_.clear();
    } else {
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
        this->defer_queue_[i] = std::move(this->defer_queue_[this->defer_queue_front_ + i]);
      }
      this->defer_queue_.resize(remaining);
    }
    this->defer_queue_front_ = 0;
  }
#endif /* not ESPHOME_THREAD_SINGLE */

  // Helper to check if item is marked for removal (platform-specific)
  // Returns true if item should be skipped, handles platform-specific synchronization
  // For ESPHOME_THREAD_MULTI_NO_ATOMICS platforms, the caller must hold the scheduler lock before calling this
  // function.
  bool is_item_removed_(SchedulerItem *item) const {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    // Multi-threaded with atomics: use atomic load for lock-free access
    return item->remove.load(std::memory_order_acquire);
#else
    // Single-threaded (ESPHOME_THREAD_SINGLE) or
    // multi-threaded without atomics (ESPHOME_THREAD_MULTI_NO_ATOMICS): direct read
    // For ESPHOME_THREAD_MULTI_NO_ATOMICS, caller MUST hold lock!
    return item->remove;
#endif
  }

  // Helper to set item removal flag (platform-specific)
  // For ESPHOME_THREAD_MULTI_NO_ATOMICS platforms, the caller must hold the scheduler lock before calling this
  // function. Uses memory_order_release when setting to true (for cancellation synchronization),
  // and memory_order_relaxed when setting to false (for initialization).
  void set_item_removed_(SchedulerItem *item, bool removed) {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    // Multi-threaded with atomics: use atomic store with appropriate ordering
    // Release ordering when setting to true ensures cancellation is visible to other threads
    // Relaxed ordering when setting to false is sufficient for initialization
    item->remove.store(removed, removed ? std::memory_order_release : std::memory_order_relaxed);
#else
    // Single-threaded (ESPHOME_THREAD_SINGLE) or
    // multi-threaded without atomics (ESPHOME_THREAD_MULTI_NO_ATOMICS): direct write
    // For ESPHOME_THREAD_MULTI_NO_ATOMICS, caller MUST hold lock!
    item->remove = removed;
#endif
  }

  // Helper to mark matching items in a container as removed
  // Returns the number of items marked for removal
  // IMPORTANT: Must be called with scheduler lock held
  template<typename Container>
  size_t mark_matching_items_removed_locked_(Container &container, Component *component, const char *name_cstr,
                                             SchedulerItem::Type type, bool match_retry) {
    size_t count = 0;
    for (auto &item : container) {
      // Skip nullptr items (can happen in defer_queue_ when items are being processed)
      // The defer_queue_ uses index-based processing: items are std::moved out but left in the
      // vector as nullptr until cleanup. Even though this function is called with lock held,
      // the vector can still contain nullptr items from the processing loop. This check prevents crashes.
      if (!item)
        continue;
      if (this->matches_item_locked_(item, component, name_cstr, type, match_retry)) {
        // Mark item for removal (platform-specific)
        this->set_item_removed_(item.get(), true);
        count++;
      }
    }
    return count;
  }

  // Template helper to check if any item in a container matches our criteria
  // IMPORTANT: Must be called with scheduler lock held
  template<typename Container>
  bool has_cancelled_timeout_in_container_locked_(const Container &container, Component *component,
                                                  const char *name_cstr, bool match_retry) const {
    for (const auto &item : container) {
      // Skip nullptr items (can happen in defer_queue_ when items are being processed)
      // The defer_queue_ uses index-based processing: items are std::moved out but left in the
      // vector as nullptr until cleanup. If this function is called during defer queue processing,
      // it will iterate over these nullptr items. This check prevents crashes.
      if (!item)
        continue;
      if (is_item_removed_(item.get()) &&
          this->matches_item_locked_(item, component, name_cstr, SchedulerItem::TIMEOUT, match_retry,
                                     /* skip_removed= */ false)) {
        return true;
      }
    }
    return false;
  }

  Mutex lock_;
  std::vector<std::unique_ptr<SchedulerItem>> items_;
  std::vector<std::unique_ptr<SchedulerItem>> to_add_;
#ifndef ESPHOME_THREAD_SINGLE
  // Single-core platforms don't need the defer queue and save ~32 bytes of RAM
  // Using std::vector instead of std::deque avoids 512-byte chunked allocations
  // Index tracking avoids O(n) erase() calls when draining the queue each loop
  std::vector<std::unique_ptr<SchedulerItem>> defer_queue_;  // FIFO queue for defer() calls
  size_t defer_queue_front_{0};  // Index of first valid item in defer_queue_ (tracks consumed items)
#endif                           /* ESPHOME_THREAD_SINGLE */
  uint32_t to_remove_{0};

  // Memory pool for recycling SchedulerItem objects to reduce heap churn.
  // Design decisions:
  // - std::vector is used instead of a fixed array because many systems only need 1-2 scheduler items
  // - The vector grows dynamically up to MAX_POOL_SIZE (5) only when needed, saving memory on simple setups
  // - Pool size of 5 matches typical usage (2-4 timers) while keeping memory overhead low (~250 bytes on ESP32)
  // - The pool significantly reduces heap fragmentation which is critical because heap allocation/deallocation
  //   can stall the entire system, causing timing issues and dropped events for any components that need
  //   to synchronize between tasks (see https://github.com/esphome/backlog/issues/52)
  std::vector<std::unique_ptr<SchedulerItem>> scheduler_item_pool_;

#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  /*
   * Multi-threaded platforms with atomic support: last_millis_ needs atomic for lock-free updates
   *
   * MEMORY-ORDERING NOTE
   * --------------------
   * `last_millis_` and `millis_major_` form a single 64-bit timestamp split in half.
   * Writers publish `last_millis_` with memory_order_release and readers use
   * memory_order_acquire. This ensures that once a reader sees the new low word,
   * it also observes the corresponding increment of `millis_major_`.
   */
  std::atomic<uint32_t> last_millis_{0};
#else  /* not ESPHOME_THREAD_MULTI_ATOMICS */
  // Platforms without atomic support or single-threaded platforms
  uint32_t last_millis_{0};
#endif /* else ESPHOME_THREAD_MULTI_ATOMICS */

  /*
   * Upper 16 bits of the 64-bit millis counter. Incremented only while holding
   * `lock_`; read concurrently. Atomic (relaxed) avoids a formal data race.
   * Ordering relative to `last_millis_` is provided by its release store and the
   * corresponding acquire loads.
   */
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint16_t> millis_major_{0};
#else  /* not ESPHOME_THREAD_MULTI_ATOMICS */
  uint16_t millis_major_{0};
#endif /* else ESPHOME_THREAD_MULTI_ATOMICS */
};

}  // namespace esphome
