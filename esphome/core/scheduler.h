#pragma once

#include "esphome/core/defines.h"
#include <vector>
#include <memory>
#include <cstring>
#include <deque>
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
  void pop_raw_();

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
  inline bool HOT matches_item_(const std::unique_ptr<SchedulerItem> &item, Component *component, const char *name_cstr,
                                SchedulerItem::Type type, bool match_retry, bool skip_removed = true) const {
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

  // Helper to recycle a SchedulerItem
  void recycle_item_(std::unique_ptr<SchedulerItem> item);

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

  // Helper to mark item for removal (platform-specific)
  // For ESPHOME_THREAD_MULTI_NO_ATOMICS platforms, the caller must hold the scheduler lock before calling this
  // function.
  void mark_item_removed_(SchedulerItem *item) {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    // Multi-threaded with atomics: use atomic store
    item->remove.store(true, std::memory_order_release);
#else
    // Single-threaded (ESPHOME_THREAD_SINGLE) or
    // multi-threaded without atomics (ESPHOME_THREAD_MULTI_NO_ATOMICS): direct write
    // For ESPHOME_THREAD_MULTI_NO_ATOMICS, caller MUST hold lock!
    item->remove = true;
#endif
  }

  // Template helper to check if any item in a container matches our criteria
  template<typename Container>
  bool has_cancelled_timeout_in_container_(const Container &container, Component *component, const char *name_cstr,
                                           bool match_retry) const {
    for (const auto &item : container) {
      if (is_item_removed_(item.get()) &&
          this->matches_item_(item, component, name_cstr, SchedulerItem::TIMEOUT, match_retry,
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
  // Single-core platforms don't need the defer queue and save 40 bytes of RAM
  std::deque<std::unique_ptr<SchedulerItem>> defer_queue_;  // FIFO queue for defer() calls
#endif                                                      /* ESPHOME_THREAD_SINGLE */
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
