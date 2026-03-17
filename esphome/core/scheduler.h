#pragma once

#include "esphome/core/defines.h"
#include <cstring>
#include <string>
#include <vector>
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
#include <atomic>
#endif

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/time_64.h"

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
  // std::string overload - deprecated, use const char* or uint32_t instead
  // Remove before 2026.7.0
  ESPDEPRECATED("Use const char* or uint32_t overload instead. Removed in 2026.7.0", "2026.1.0")
  void set_timeout(Component *component, const std::string &name, uint32_t timeout, std::function<void()> &&func);

  /** Set a timeout with a const char* name.
   *
   * IMPORTANT: The provided name pointer must remain valid for the lifetime of the scheduler item.
   * This means the name should be:
   *   - A string literal (e.g., "update")
   *   - A static const char* variable
   *   - A pointer with lifetime >= the scheduled task
   */
  void set_timeout(Component *component, const char *name, uint32_t timeout, std::function<void()> &&func);
  /// Set a timeout with a numeric ID (zero heap allocation)
  void set_timeout(Component *component, uint32_t id, uint32_t timeout, std::function<void()> &&func);
  /// Set a timeout with an internal scheduler ID (separate namespace from component NUMERIC_ID)
  void set_timeout(Component *component, InternalSchedulerID id, uint32_t timeout, std::function<void()> &&func) {
    this->set_timer_common_(component, SchedulerItem::TIMEOUT, NameType::NUMERIC_ID_INTERNAL, nullptr,
                            static_cast<uint32_t>(id), timeout, std::move(func));
  }

  ESPDEPRECATED("Use const char* or uint32_t overload instead. Removed in 2026.7.0", "2026.1.0")
  bool cancel_timeout(Component *component, const std::string &name);
  bool cancel_timeout(Component *component, const char *name);
  bool cancel_timeout(Component *component, uint32_t id);
  bool cancel_timeout(Component *component, InternalSchedulerID id) {
    return this->cancel_item_(component, NameType::NUMERIC_ID_INTERNAL, nullptr, static_cast<uint32_t>(id),
                              SchedulerItem::TIMEOUT);
  }

  ESPDEPRECATED("Use const char* or uint32_t overload instead. Removed in 2026.7.0", "2026.1.0")
  void set_interval(Component *component, const std::string &name, uint32_t interval, std::function<void()> &&func);

  /** Set an interval with a const char* name.
   *
   * IMPORTANT: The provided name pointer must remain valid for the lifetime of the scheduler item.
   * This means the name should be:
   *   - A string literal (e.g., "update")
   *   - A static const char* variable
   *   - A pointer with lifetime >= the scheduled task
   */
  void set_interval(Component *component, const char *name, uint32_t interval, std::function<void()> &&func);
  /// Set an interval with a numeric ID (zero heap allocation)
  void set_interval(Component *component, uint32_t id, uint32_t interval, std::function<void()> &&func);
  /// Set an interval with an internal scheduler ID (separate namespace from component NUMERIC_ID)
  void set_interval(Component *component, InternalSchedulerID id, uint32_t interval, std::function<void()> &&func) {
    this->set_timer_common_(component, SchedulerItem::INTERVAL, NameType::NUMERIC_ID_INTERNAL, nullptr,
                            static_cast<uint32_t>(id), interval, std::move(func));
  }

  ESPDEPRECATED("Use const char* or uint32_t overload instead. Removed in 2026.7.0", "2026.1.0")
  bool cancel_interval(Component *component, const std::string &name);
  bool cancel_interval(Component *component, const char *name);
  bool cancel_interval(Component *component, uint32_t id);
  bool cancel_interval(Component *component, InternalSchedulerID id) {
    return this->cancel_item_(component, NameType::NUMERIC_ID_INTERNAL, nullptr, static_cast<uint32_t>(id),
                              SchedulerItem::INTERVAL);
  }

  // Remove before 2026.8.0
  ESPDEPRECATED("set_retry is deprecated and will be removed in 2026.8.0. Use set_timeout or set_interval instead.",
                "2026.2.0")
  void set_retry(Component *component, const std::string &name, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);
  // Remove before 2026.8.0
  ESPDEPRECATED("set_retry is deprecated and will be removed in 2026.8.0. Use set_timeout or set_interval instead.",
                "2026.2.0")
  void set_retry(Component *component, const char *name, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);
  // Remove before 2026.8.0
  ESPDEPRECATED("set_retry is deprecated and will be removed in 2026.8.0. Use set_timeout or set_interval instead.",
                "2026.2.0")
  void set_retry(Component *component, uint32_t id, uint32_t initial_wait_time, uint8_t max_attempts,
                 std::function<RetryResult(uint8_t)> func, float backoff_increase_factor = 1.0f);

  // Remove before 2026.8.0
  ESPDEPRECATED("cancel_retry is deprecated and will be removed in 2026.8.0.", "2026.2.0")
  bool cancel_retry(Component *component, const std::string &name);
  // Remove before 2026.8.0
  ESPDEPRECATED("cancel_retry is deprecated and will be removed in 2026.8.0.", "2026.2.0")
  bool cancel_retry(Component *component, const char *name);
  // Remove before 2026.8.0
  ESPDEPRECATED("cancel_retry is deprecated and will be removed in 2026.8.0.", "2026.2.0")
  bool cancel_retry(Component *component, uint32_t id);

  /// Get 64-bit millisecond timestamp (handles 32-bit millis() rollover)
  uint64_t millis_64() { return esphome::millis_64(); }

  // Calculate when the next scheduled item should run.
  // @param now On ESP32, unused for 64-bit extension (native); on other platforms, extended to 64-bit via rollover.
  // Returns the time in milliseconds until the next scheduled item, or nullopt if no items.
  // This method performs cleanup of removed items before checking the schedule.
  // IMPORTANT: This method should only be called from the main thread (loop task).
  optional<uint32_t> next_schedule_in(uint32_t now);

  // Execute all scheduled items that are ready
  // @param now Fresh timestamp from millis() - must not be stale/cached
  void call(uint32_t now);

  void process_to_add();

  // Name storage type discriminator for SchedulerItem
  // Used to distinguish between static strings, hashed strings, numeric IDs, and internal numeric IDs
  enum class NameType : uint8_t {
    STATIC_STRING = 0,       // const char* pointer to static/flash storage
    HASHED_STRING = 1,       // uint32_t FNV-1a hash of a runtime string
    NUMERIC_ID = 2,          // uint32_t numeric identifier (component-level)
    NUMERIC_ID_INTERNAL = 3  // uint32_t numeric identifier (core/internal, separate namespace)
  };

 protected:
  struct SchedulerItem {
    // Ordered by size to minimize padding
    Component *component;
    // Optimized name storage using tagged union - zero heap allocation
    union {
      const char *static_name;  // For STATIC_STRING (string literals, no allocation)
      uint32_t hash_or_id;      // For HASHED_STRING or NUMERIC_ID
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
    // Multi-threaded with atomics: use atomic uint8_t for lock-free access.
    // std::atomic<bool> is not used because GCC on Xtensa generates an indirect
    // function call for std::atomic<bool>::load() instead of inlining it.
    // std::atomic<uint8_t> inlines correctly on all platforms.
    std::atomic<uint8_t> remove{0};

    // Bit-packed fields (4 bits used, 4 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    NameType name_type_ : 2;  // Discriminator for name_ union (0–3, see NameType enum)
    bool is_retry : 1;        // True if this is a retry timeout
                              // 4 bits padding
#else
    // Single-threaded or multi-threaded without atomics: can pack all fields together
    // Bit-packed fields (5 bits used, 3 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    bool remove : 1;
    NameType name_type_ : 2;  // Discriminator for name_ union (0–3, see NameType enum)
    bool is_retry : 1;        // True if this is a retry timeout
                              // 3 bits padding
#endif

    // Constructor
    SchedulerItem()
        : component(nullptr),
          interval(0),
          next_execution_low_(0),
          next_execution_high_(0),
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
          // remove is initialized in the member declaration
          type(TIMEOUT),
          name_type_(NameType::STATIC_STRING),
          is_retry(false) {
#else
          type(TIMEOUT),
          remove(false),
          name_type_(NameType::STATIC_STRING),
          is_retry(false) {
#endif
      name_.static_name = nullptr;
    }

    // Destructor - no dynamic memory to clean up (callback's std::function handles its own)
    ~SchedulerItem() = default;

    // Delete copy operations to prevent accidental copies
    SchedulerItem(const SchedulerItem &) = delete;
    SchedulerItem &operator=(const SchedulerItem &) = delete;

    // Delete move operations: SchedulerItem objects are managed via raw pointers, never moved directly
    SchedulerItem(SchedulerItem &&) = delete;
    SchedulerItem &operator=(SchedulerItem &&) = delete;

    // Helper to get the static name (only valid for STATIC_STRING type)
    const char *get_name() const { return (name_type_ == NameType::STATIC_STRING) ? name_.static_name : nullptr; }

    // Helper to get the hash or numeric ID (only valid for HASHED_STRING or NUMERIC_ID types)
    uint32_t get_name_hash_or_id() const { return (name_type_ != NameType::STATIC_STRING) ? name_.hash_or_id : 0; }

    // Helper to get the name type
    NameType get_name_type() const { return name_type_; }

    // Set name storage: for STATIC_STRING stores the pointer, for all other types stores hash_or_id.
    // Both union members occupy the same offset, so only one store is needed.
    void set_name(NameType type, const char *static_name, uint32_t hash_or_id) {
      if (type == NameType::STATIC_STRING) {
        name_.static_name = static_name;
      } else {
        name_.hash_or_id = hash_or_id;
      }
      name_type_ = type;
    }

    static bool cmp(SchedulerItem *a, SchedulerItem *b);

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
  // name_type determines storage type: STATIC_STRING uses static_name, others use hash_or_id
  void set_timer_common_(Component *component, SchedulerItem::Type type, NameType name_type, const char *static_name,
                         uint32_t hash_or_id, uint32_t delay, std::function<void()> &&func, bool is_retry = false,
                         bool skip_cancel = false);

  // Common implementation for retry - Remove before 2026.8.0
  // name_type determines storage type: STATIC_STRING uses static_name, others use hash_or_id
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  void set_retry_common_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id,
                         uint32_t initial_wait_time, uint8_t max_attempts, std::function<RetryResult(uint8_t)> func,
                         float backoff_increase_factor);
#pragma GCC diagnostic pop
  // Common implementation for cancel_retry
  bool cancel_retry_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id);

  // Extend a 32-bit millis() value to 64-bit. Use when the caller already has a fresh now.
  // On platforms with native 64-bit time, ignores now and uses millis_64() directly.
  // On other platforms, extends now to 64-bit using rollover tracking.
  uint64_t millis_64_from_(uint32_t now) {
#ifdef USE_NATIVE_64BIT_TIME
    (void) now;
    return millis_64();
#else
    return Millis64Impl::compute(now);
#endif
  }
  // Cleanup logically deleted items from the scheduler
  // Returns true if items remain after cleanup
  // IMPORTANT: This method should only be called from the main thread (loop task).
  bool cleanup_();
  // Remove and return the front item from the heap as a raw pointer.
  // Caller takes ownership and must either recycle or delete the item.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  SchedulerItem *pop_raw_locked_();
  // Get or create a scheduler item from the pool
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  SchedulerItem *get_item_from_pool_locked_();

 private:
  // Helper to cancel items - must be called with lock held
  // name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
  bool cancel_item_locked_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id,
                           SchedulerItem::Type type, bool match_retry = false);

  // Common implementation for cancel operations - handles locking
  bool cancel_item_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id,
                    SchedulerItem::Type type, bool match_retry = false);

  // Helper to check if two static string names match
  inline bool HOT names_match_static_(const char *name1, const char *name2) const {
    // Check pointer equality first (common for static strings), then string contents
    // The core ESPHome codebase uses static strings (const char*) for component names,
    // making pointer comparison effective. The std::string overloads exist only for
    // compatibility with external components but are rarely used in practice.
    return (name1 != nullptr && name2 != nullptr) && ((name1 == name2) || (strcmp(name1, name2) == 0));
  }

  // Helper function to check if item matches criteria for cancellation
  // name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
  // IMPORTANT: Must be called with scheduler lock held
  inline bool HOT matches_item_locked_(SchedulerItem *item, Component *component, NameType name_type,
                                       const char *static_name, uint32_t hash_or_id, SchedulerItem::Type type,
                                       bool match_retry, bool skip_removed = true) const {
    // THREAD SAFETY: Check for nullptr first to prevent LoadProhibited crashes. On multi-threaded
    // platforms, items can be nulled in defer_queue_ during processing.
    // Fixes: https://github.com/esphome/esphome/issues/11940
    if (item == nullptr)
      return false;
    if (item->component != component || item->type != type || (skip_removed && this->is_item_removed_locked_(item)) ||
        (match_retry && !item->is_retry)) {
      return false;
    }
    // Name type must match
    if (item->get_name_type() != name_type)
      return false;
    // For static strings, compare the string content; for hash/ID, compare the value
    if (name_type == NameType::STATIC_STRING) {
      return this->names_match_static_(item->get_name(), static_name);
    }
    return item->get_name_hash_or_id() == hash_or_id;
  }

  // Helper to execute a scheduler item
  uint32_t execute_item_(SchedulerItem *item, uint32_t now);

  // Helper to check if item should be skipped
  bool should_skip_item_(SchedulerItem *item) const {
    return is_item_removed_(item) || (item->component != nullptr && item->component->is_failed());
  }

  // Helper to recycle a SchedulerItem back to the pool.
  // Takes a raw pointer — caller transfers ownership. The item is either added to the
  // pool or deleted if the pool is full.
  // IMPORTANT: Only call from main loop context! Recycling clears the callback,
  // so calling from another thread while the callback is executing causes use-after-free.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  void recycle_item_main_loop_(SchedulerItem *item);

  // Helper to perform full cleanup when too many items are cancelled
  void full_cleanup_removed_items_();

  // Helper to calculate random offset for interval timers - extracted to reduce code size of set_timer_common_
  // IMPORTANT: Must not be inlined - called only for intervals, keeping it out of the hot path saves flash.
  uint32_t __attribute__((noinline)) calculate_interval_offset_(uint32_t delay);

  // Helper to check if a retry was already cancelled - extracted to reduce code size of set_timer_common_
  // Remove before 2026.8.0 along with all retry code.
  // IMPORTANT: Must not be inlined - retry path is cold and deprecated.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  bool __attribute__((noinline))
  is_retry_cancelled_locked_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id);

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Helper for debug logging in set_timer_common_ - extracted to reduce code size
  void debug_log_timer_(const SchedulerItem *item, NameType name_type, const char *static_name, uint32_t hash_or_id,
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

    // Fast path: nothing to process, avoid lock entirely.
    // Worst case is a one-loop-iteration delay before newly deferred items are processed.
    if (this->defer_empty_())
      return;

    // Merge lock acquisitions: instead of separate locks for move-out and recycle (2N+1 total),
    // recycle each item after re-acquiring the lock for the next iteration (N+1 total).
    // The lock is held across: recycle → loop condition → move-out, then released for execution.
    SchedulerItem *item;

    this->lock_.lock();
    // Reset counter and snapshot queue end under lock
    this->defer_count_clear_();
    size_t defer_queue_end = this->defer_queue_.size();
    if (this->defer_queue_front_ >= defer_queue_end) {
      this->lock_.unlock();
      return;
    }
    while (this->defer_queue_front_ < defer_queue_end) {
      // Take ownership of the item, leaving nullptr in the vector slot.
      // This is safe because:
      // 1. The vector is only cleaned up by cleanup_defer_queue_locked_() at the end of this function
      // 2. Any code iterating defer_queue_ MUST check for nullptr items (see mark_matching_items_removed_locked_)
      // 3. The lock protects concurrent access, but the nullptr remains until cleanup
      item = this->defer_queue_[this->defer_queue_front_];
      this->defer_queue_[this->defer_queue_front_] = nullptr;
      this->defer_queue_front_++;
      this->lock_.unlock();

      // Execute callback without holding lock to prevent deadlocks
      // if the callback tries to call defer() again
      if (!this->should_skip_item_(item)) {
        now = this->execute_item_(item, now);
      }

      this->lock_.lock();
      this->recycle_item_main_loop_(item);
    }
    // Clean up the queue (lock already held from last recycle or initial acquisition)
    this->cleanup_defer_queue_locked_();
    this->lock_.unlock();
  }

  // Helper to cleanup defer_queue_ after processing.
  // Keeps the common clear() path inline, outlines the rare compaction to keep
  // cold code out of the hot instruction cache lines.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  inline void cleanup_defer_queue_locked_() {
    // Check if new items were added by producers during processing
    if (this->defer_queue_front_ >= this->defer_queue_.size()) {
      // Common case: no new items - clear everything
      this->defer_queue_.clear();
    } else {
      // Rare case: new items were added during processing - outlined to keep cold code
      // out of the hot instruction cache lines
      this->compact_defer_queue_locked_();
    }
    this->defer_queue_front_ = 0;
  }

  // Cold path for compacting defer_queue_ when new items were added during processing.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  // IMPORTANT: Must not be inlined - rare path, outlined to keep it out of the hot instruction cache lines.
  void __attribute__((noinline)) compact_defer_queue_locked_();
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

  // Helper to check if item is marked for removal when lock is already held.
  // Uses relaxed ordering since the mutex provides all necessary synchronization.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  bool is_item_removed_locked_(SchedulerItem *item) const {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    // Lock already held - relaxed is sufficient, mutex provides ordering
    return item->remove.load(std::memory_order_relaxed);
#else
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
    item->remove.store(removed ? 1 : 0, removed ? std::memory_order_release : std::memory_order_relaxed);
#else
    // Single-threaded (ESPHOME_THREAD_SINGLE) or
    // multi-threaded without atomics (ESPHOME_THREAD_MULTI_NO_ATOMICS): direct write
    // For ESPHOME_THREAD_MULTI_NO_ATOMICS, caller MUST hold lock!
    item->remove = removed;
#endif
  }

  // Helper to mark matching items in a container as removed
  // name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
  // Returns the number of items marked for removal
  // IMPORTANT: Must be called with scheduler lock held
  __attribute__((noinline)) size_t mark_matching_items_removed_locked_(std::vector<SchedulerItem *> &container,
                                                                       Component *component, NameType name_type,
                                                                       const char *static_name, uint32_t hash_or_id,
                                                                       SchedulerItem::Type type, bool match_retry) {
    size_t count = 0;
    for (auto *item : container) {
      if (this->matches_item_locked_(item, component, name_type, static_name, hash_or_id, type, match_retry)) {
        this->set_item_removed_(item, true);
        count++;
      }
    }
    return count;
  }

  Mutex lock_;
  std::vector<SchedulerItem *> items_;
  std::vector<SchedulerItem *> to_add_;

#ifndef ESPHOME_THREAD_SINGLE
  // Fast-path counter for process_to_add() to skip taking the lock when there is
  // nothing to add. Uses std::atomic on platforms that support it, plain uint32_t
  // otherwise. On non-atomic platforms, callers must hold the scheduler lock when
  // mutating this counter. Not needed on single-threaded platforms where we can
  // check to_add_.empty() directly.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> to_add_count_{0};
#else
  uint32_t to_add_count_{0};
#endif
#endif /* ESPHOME_THREAD_SINGLE */

  // Fast-path helper for process_to_add() to decide if it can try the lock-free path.
  // - On ESPHOME_THREAD_SINGLE: direct container check is safe (no concurrent writers).
  // - On ESPHOME_THREAD_MULTI_ATOMICS: performs a lock-free check via to_add_count_.
  // - On ESPHOME_THREAD_MULTI_NO_ATOMICS: always returns false to force the caller
  //   down the locked path; this is NOT a lock-free emptiness check on that platform.
  bool to_add_empty_() const {
#ifdef ESPHOME_THREAD_SINGLE
    return this->to_add_.empty();
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    return this->to_add_count_.load(std::memory_order_relaxed) == 0;
#else
  return false;
#endif
  }

  // Increment to_add_count_ (no-op on single-threaded platforms)
  void to_add_count_increment_() {
#ifdef ESPHOME_THREAD_SINGLE
    // No counter needed — to_add_empty_() checks the vector directly
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_add_count_.fetch_add(1, std::memory_order_relaxed);
#else
  this->to_add_count_++;
#endif
  }

  // Reset to_add_count_ (no-op on single-threaded platforms)
  void to_add_count_clear_() {
#ifdef ESPHOME_THREAD_SINGLE
    // No counter needed — to_add_empty_() checks the vector directly
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_add_count_.store(0, std::memory_order_relaxed);
#else
  this->to_add_count_ = 0;
#endif
  }

#ifndef ESPHOME_THREAD_SINGLE
  // Single-core platforms don't need the defer queue and save ~32 bytes of RAM
  // Using std::vector instead of std::deque avoids 512-byte chunked allocations
  // Index tracking avoids O(n) erase() calls when draining the queue each loop
  std::vector<SchedulerItem *> defer_queue_;  // FIFO queue for defer() calls
  size_t defer_queue_front_{0};               // Index of first valid item in defer_queue_ (tracks consumed items)

  // Fast-path counter for process_defer_queue_() to skip lock when nothing to process.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> defer_count_{0};
#else
  uint32_t defer_count_{0};
#endif

  bool defer_empty_() const {
    // defer_queue_ only exists on multi-threaded platforms, so no ESPHOME_THREAD_SINGLE path
    // ESPHOME_THREAD_MULTI_NO_ATOMICS: always take the lock
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    return this->defer_count_.load(std::memory_order_relaxed) == 0;
#else
    return false;
#endif
  }

  void defer_count_increment_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->defer_count_.fetch_add(1, std::memory_order_relaxed);
#else
    this->defer_count_++;
#endif
  }

  void defer_count_clear_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->defer_count_.store(0, std::memory_order_relaxed);
#else
    this->defer_count_ = 0;
#endif
  }

#endif /* ESPHOME_THREAD_SINGLE */

  // Counter for items marked for removal. Incremented cross-thread in cancel_item_locked_().
  // On ESPHOME_THREAD_MULTI_ATOMICS this is read without a lock in the cleanup_() fast path;
  // on ESPHOME_THREAD_MULTI_NO_ATOMICS the fast path is disabled so cleanup_() always takes the lock.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> to_remove_{0};
#else
  uint32_t to_remove_{0};
#endif

  // Lock-free check if there are items to remove (for fast-path in cleanup_)
  bool to_remove_empty_() const {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    return this->to_remove_.load(std::memory_order_relaxed) == 0;
#elif defined(ESPHOME_THREAD_SINGLE)
    return this->to_remove_ == 0;
#else
  return false;  // Always take the lock path
#endif
  }

  void to_remove_add_(uint32_t count) {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->to_remove_.fetch_add(count, std::memory_order_relaxed);
#else
    this->to_remove_ += count;
#endif
  }

  void to_remove_decrement_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->to_remove_.fetch_sub(1, std::memory_order_relaxed);
#else
    this->to_remove_--;
#endif
  }

  void to_remove_clear_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->to_remove_.store(0, std::memory_order_relaxed);
#else
    this->to_remove_ = 0;
#endif
  }

  uint32_t to_remove_count_() const {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    return this->to_remove_.load(std::memory_order_relaxed);
#else
    return this->to_remove_;
#endif
  }

  // Memory pool for recycling SchedulerItem objects to reduce heap churn.
  // Design decisions:
  // - std::vector is used instead of a fixed array because many systems only need 1-2 scheduler items
  // - The vector grows dynamically up to MAX_POOL_SIZE (5) only when needed, saving memory on simple setups
  // - Pool size of 5 matches typical usage (2-4 timers) while keeping memory overhead low (~250 bytes on ESP32)
  // - The pool significantly reduces heap fragmentation which is critical because heap allocation/deallocation
  //   can stall the entire system, causing timing issues and dropped events for any components that need
  //   to synchronize between tasks (see https://github.com/esphome/backlog/issues/52)
  std::vector<SchedulerItem *> scheduler_item_pool_;

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Leak detection: tracks total live SchedulerItem allocations.
  // Invariant: debug_live_items_ == items_.size() + to_add_.size() + defer_queue_.size() + scheduler_item_pool_.size()
  // Verified periodically in call() to catch leaks early.
  size_t debug_live_items_{0};

  // Verify the scheduler memory invariant: all allocated items are accounted for.
  // Returns true if no leak detected. Logs an error and asserts on failure.
  bool debug_verify_no_leak_() const;
#endif
};

}  // namespace esphome
