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
  // @return Timestamp of the last item that ran, or `now` unchanged if none ran.
  uint32_t call(uint32_t now);

  // Reclaim memory held by the post-boot peak. Frees every SchedulerItem in the
  // recycle freelist and shrinks items_/to_add_/defer_queue_ vector capacity to
  // their current sizes (std::vector grows by doubling and otherwise retains the
  // peak). Live items in those vectors are preserved.
  void trim_freelist();

  // Move items from to_add_ into the main heap.
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // Inlined: the fast path (nothing to add) is just an atomic load / empty check.
  // The lock-free fast path uses to_add_count_ (atomic) or to_add_.empty()
  // (single-threaded). This is safe because the main loop is the only thread
  // that reads to_add_ without holding lock_; other threads may read it only
  // while holding the mutex (e.g. cancel_item_locked_).
  inline void ESPHOME_ALWAYS_INLINE HOT process_to_add() {
    if (this->to_add_empty_())
      return;
    this->process_to_add_slow_path_();
  }

  // Name storage type discriminator for SchedulerItem
  // Used to distinguish between static strings, hashed strings, numeric IDs, internal numeric IDs,
  // and self-keyed pointers (caller-supplied `void *`, typically `this`).
  enum class NameType : uint8_t {
    STATIC_STRING = 0,        // const char* pointer to static/flash storage
    HASHED_STRING = 1,        // uint32_t FNV-1a hash of a runtime string
    NUMERIC_ID = 2,           // uint32_t numeric identifier (component-level)
    NUMERIC_ID_INTERNAL = 3,  // uint32_t numeric identifier (core/internal, separate namespace)
    SELF_POINTER = 4          // void* caller-supplied key (typically `this`); pointer equality
  };

  /** Self-keyed timeout. The cancellation key is `self` (typically the caller's `this`).
   *
   * Use this when the caller schedules at most one timer of a single purpose at a time and
   * does not need a `Component` for `is_failed()` skip or log source attribution. Lets
   * small classes drop `Component` inheritance entirely when their only Component dependency
   * was the per-instance scheduler key.
   *
   * NOT applied for self-keyed items:
   *  - `is_failed()` skip — callbacks always fire (no Component to consult).
   *  - Log source attribution — logs use a generic "self:0x…" label.
   *
   * If you need either of those, use the existing `(Component *, id)` overloads.
   */
  void set_timeout(const void *self, uint32_t timeout, std::function<void()> &&func);
  /// Self-keyed interval. See set_timeout(const void *, ...) for semantics.
  void set_interval(const void *self, uint32_t interval, std::function<void()> &&func);
  bool cancel_timeout(const void *self);
  bool cancel_interval(const void *self);

 protected:
  struct SchedulerItem {
    // Ordered by size to minimize padding.
    // `component` while live; `next_free` while in scheduler_item_pool_head_ (mutually exclusive).
    union {
      Component *component;
      SchedulerItem *next_free;
    };
    // Optimized name storage using tagged union - zero heap allocation
    union {
      const char *static_name;  // For STATIC_STRING (string literals) and SELF_POINTER (caller's `this`)
      uint32_t hash_or_id;      // For HASHED_STRING, NUMERIC_ID, and NUMERIC_ID_INTERNAL
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

    // Bit-packed fields (5 bits used, 3 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    NameType name_type_ : 3;  // Discriminator for name_ union (0–4, see NameType enum)
    bool is_retry : 1;        // True if this is a retry timeout
                              // 3 bits padding
#else
    // Single-threaded or multi-threaded without atomics: can pack all fields together
    // Bit-packed fields (6 bits used, 2 bits padding in 1 byte)
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    bool remove : 1;
    NameType name_type_ : 3;  // Discriminator for name_ union (0–4, see NameType enum)
    bool is_retry : 1;        // True if this is a retry timeout
                              // 2 bits padding
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

    // Helper to get the pointer-slot value (valid for STATIC_STRING and SELF_POINTER types).
    // Both share the same union member, so callers (e.g. log formatters) can read either uniformly.
    const char *get_name() const {
      return (name_type_ == NameType::STATIC_STRING || name_type_ == NameType::SELF_POINTER) ? name_.static_name
                                                                                             : nullptr;
    }

    // Helper to get the hash or numeric ID (only valid for HASHED_STRING / NUMERIC_ID / NUMERIC_ID_INTERNAL types)
    uint32_t get_name_hash_or_id() const {
      return (name_type_ != NameType::STATIC_STRING && name_type_ != NameType::SELF_POINTER) ? name_.hash_or_id : 0;
    }

    // Helper to get the name type
    NameType get_name_type() const { return name_type_; }

    // Set name storage. STATIC_STRING/SELF_POINTER use the static_name pointer slot
    // (both are pointer-width); other types use hash_or_id. Both union members occupy
    // the same offset, so only one store is needed.
    void set_name(NameType type, const char *static_name, uint32_t hash_or_id) {
      if (type == NameType::STATIC_STRING || type == NameType::SELF_POINTER) {
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
  // On platforms with native 64-bit time (ESP32, Host, Zephyr, RP2040 — see
  // USE_NATIVE_64BIT_TIME in defines.h), ignores now and uses millis_64() directly, so the
  // Scheduler always works in 64-bit time regardless of what the caller's 32-bit now came
  // from. On ESP32 specifically, millis() comes from xTaskGetTickCount while millis_64()
  // comes from esp_timer — two different clocks — but that is safe because scheduling
  // compares millis_64 values against millis_64 only, never against millis().
  // On platforms without native 64-bit time (e.g. ESP8266), extends now to 64-bit using
  // rollover tracking, so both millis() and scheduling use the same underlying clock.
  uint64_t ESPHOME_ALWAYS_INLINE millis_64_from_(uint32_t now) {
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
  // Inlined: the fast path (nothing to remove) is just an atomic load + empty check.
  // Reading items_.empty() without the lock is safe here because only the main
  // loop thread structurally modifies items_ (push/pop/erase). Other threads may
  // iterate items_ and mark items removed under lock_, but never change the
  // vector's size or data pointer.
  inline bool ESPHOME_ALWAYS_INLINE HOT cleanup_() {
    if (this->to_remove_empty_())
      return !this->items_.empty();
    return this->cleanup_slow_path_();
  }
  // Slow path for cleanup_() when there are items to remove - defined in scheduler.cpp
  bool cleanup_slow_path_();
  // Slow path for process_to_add() when there are items to merge - defined in scheduler.cpp
  void process_to_add_slow_path_();
  // Remove and return the front item from the heap as a raw pointer.
  // Caller takes ownership and must either recycle or delete the item.
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  SchedulerItem *pop_raw_locked_();
  // Get or create a scheduler item from the pool
  // IMPORTANT: Caller must hold the scheduler lock before calling this function.
  SchedulerItem *get_item_from_pool_locked_();

 private:
  // Out-of-line helper that shrinks a SchedulerItem* vector's capacity to its current
  // size. Centralised so trim_freelist() doesn't pay flash cost per call site.
  void shrink_scheduler_vector_(std::vector<SchedulerItem *> *v);

  // Helper to cancel matching items - must be called with lock held.
  // When find_first=true, stops after the first match (used by set_timer_common_ where
  // the cancel-before-add invariant guarantees at most one match).
  // When find_first=false (default), cancels ALL matches (needed for DelayAction parallel
  // mode where skip_cancel=true allows multiple items with the same key).
  // name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
  bool cancel_item_locked_(Component *component, NameType name_type, const char *static_name, uint32_t hash_or_id,
                           SchedulerItem::Type type, bool match_retry = false, bool find_first = false);

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
    // STATIC_STRING: compare string content. SELF_POINTER: raw pointer equality (no strcmp).
    // Other types: compare hash/ID value.
    if (name_type == NameType::STATIC_STRING) {
      return this->names_match_static_(item->get_name(), static_name);
    }
    if (name_type == NameType::SELF_POINTER) {
      return item->name_.static_name == static_name;
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
  // Process defer queue for FIFO execution of deferred items.
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // Inlined: the fast path (nothing deferred) is just an atomic load check.
  inline void ESPHOME_ALWAYS_INLINE HOT process_defer_queue_(uint32_t &now) {
    // Fast path: nothing to process, avoid lock entirely.
    // Worst case is a one-loop-iteration delay before newly deferred items are processed.
    if (this->defer_empty_())
      return;
    this->process_defer_queue_slow_path_(now);
  }

  // Slow path for process_defer_queue_() - defined in scheduler.cpp
  void process_defer_queue_slow_path_(uint32_t &now);

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

  // Helper to mark matching items in a container as removed.
  // When find_first=true, stops after the first match (used by set_timer_common_ where
  // the cancel-before-add invariant guarantees at most one match).
  // When find_first=false, marks ALL matches (needed for public cancel path where
  // DelayAction parallel mode with skip_cancel=true can create multiple items with the same key).
  // name_type determines matching: STATIC_STRING uses static_name, others use hash_or_id
  // Returns the number of items marked for removal.
  // IMPORTANT: Must be called with scheduler lock held
  // Inlined: the fast path (empty container) avoids calling the out-of-line scan.
  inline size_t HOT mark_matching_items_removed_locked_(std::vector<SchedulerItem *> &container, Component *component,
                                                        NameType name_type, const char *static_name,
                                                        uint32_t hash_or_id, SchedulerItem::Type type, bool match_retry,
                                                        bool find_first = false) {
    if (container.empty())
      return 0;
    return this->mark_matching_items_removed_slow_locked_(container, component, name_type, static_name, hash_or_id,
                                                          type, match_retry, find_first);
  }

  // Out-of-line slow path for mark_matching_items_removed_locked_ when container is non-empty.
  // IMPORTANT: Must be called with scheduler lock held
  __attribute__((noinline)) size_t mark_matching_items_removed_slow_locked_(
      std::vector<SchedulerItem *> &container, Component *component, NameType name_type, const char *static_name,
      uint32_t hash_or_id, SchedulerItem::Type type, bool match_retry, bool find_first);

  Mutex lock_;
  std::vector<SchedulerItem *> items_;
  std::vector<SchedulerItem *> to_add_;

#ifndef ESPHOME_THREAD_SINGLE
  // Fast-path counter for process_to_add() to skip taking the lock when there
  // is nothing to add. std::atomic on ATOMICS; plain uint32_t on NO_ATOMICS
  // (BK72xx — ARMv5TE single-core, lacks LDREX/STREX so std::atomic RMW would
  // require libatomic). Reads use __atomic_load_n(__ATOMIC_RELAXED) on
  // NO_ATOMICS — compiles to a plain LDR (aligned 32-bit load is naturally
  // atomic on ARMv5TE) but expresses the concurrent-access intent in the C++
  // memory model. Writes live behind *_locked_ helpers and must hold lock_.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> to_add_count_{0};
#else
  uint32_t to_add_count_{0};
#endif
#endif /* ESPHOME_THREAD_SINGLE */

  // Fast-path helper for process_to_add() to decide if it can skip the lock.
  bool to_add_empty_() const {
#ifdef ESPHOME_THREAD_SINGLE
    return this->to_add_.empty();
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    return this->to_add_count_.load(std::memory_order_relaxed) == 0;
#else
  return __atomic_load_n(&this->to_add_count_, __ATOMIC_RELAXED) == 0;
#endif
  }

  // Increment to_add_count_ (no-op on single-threaded platforms).
  // On NO_ATOMICS the caller must hold lock_; both load and store go through
  // __atomic_*_n with __ATOMIC_RELAXED to keep every access to the counter
  // explicitly atomic in the C++ memory model (same ARMv5TE codegen as
  // plain LDR+STR).
  void to_add_count_increment_locked_() {
#if defined(ESPHOME_THREAD_SINGLE)
    // No counter needed — to_add_empty_() checks the vector directly
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_add_count_.fetch_add(1, std::memory_order_relaxed);
#else
  uint32_t v = __atomic_load_n(&this->to_add_count_, __ATOMIC_RELAXED);
  __atomic_store_n(&this->to_add_count_, v + 1, __ATOMIC_RELAXED);
#endif
  }

  // Reset to_add_count_ (no-op on single-threaded platforms)
  void to_add_count_clear_locked_() {
#if defined(ESPHOME_THREAD_SINGLE)
    // No counter needed — to_add_empty_() checks the vector directly
#elif defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_add_count_.store(0, std::memory_order_relaxed);
#else
  __atomic_store_n(&this->to_add_count_, 0, __ATOMIC_RELAXED);
#endif
  }

#ifndef ESPHOME_THREAD_SINGLE
  // Single-core platforms don't need the defer queue and save ~32 bytes of RAM
  // Using std::vector instead of std::deque avoids 512-byte chunked allocations
  // Index tracking avoids O(n) erase() calls when draining the queue each loop
  std::vector<SchedulerItem *> defer_queue_;  // FIFO queue for defer() calls
  size_t defer_queue_front_{0};               // Index of first valid item in defer_queue_ (tracks consumed items)

  // Fast-path counter for process_defer_queue_() to skip lock when nothing to
  // process. See to_add_count_ above for the NO_ATOMICS rationale.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> defer_count_{0};
#else
  uint32_t defer_count_{0};
#endif

  bool defer_empty_() const {
    // defer_queue_ only exists on multi-threaded platforms, so no ESPHOME_THREAD_SINGLE path
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    return this->defer_count_.load(std::memory_order_relaxed) == 0;
#else
    return __atomic_load_n(&this->defer_count_, __ATOMIC_RELAXED) == 0;
#endif
  }

  void defer_count_increment_locked_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->defer_count_.fetch_add(1, std::memory_order_relaxed);
#else
    uint32_t v = __atomic_load_n(&this->defer_count_, __ATOMIC_RELAXED);
    __atomic_store_n(&this->defer_count_, v + 1, __ATOMIC_RELAXED);
#endif
  }

  void defer_count_clear_locked_() {
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
    this->defer_count_.store(0, std::memory_order_relaxed);
#else
    __atomic_store_n(&this->defer_count_, 0, __ATOMIC_RELAXED);
#endif
  }

#endif /* ESPHOME_THREAD_SINGLE */

  // Counter for items marked for removal. Incremented cross-thread in
  // cancel_item_locked_(). See to_add_count_ above for the NO_ATOMICS
  // rationale.
#ifdef ESPHOME_THREAD_MULTI_ATOMICS
  std::atomic<uint32_t> to_remove_{0};
#else
  uint32_t to_remove_{0};
#endif

  // Lock-free check if there are items to remove (for fast-path in cleanup_)
  bool to_remove_empty_() const {
#if defined(ESPHOME_THREAD_MULTI_ATOMICS)
    return this->to_remove_.load(std::memory_order_relaxed) == 0;
#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
    return __atomic_load_n(&this->to_remove_, __ATOMIC_RELAXED) == 0;
#else
  return this->to_remove_ == 0;
#endif
  }

  void to_remove_add_locked_(uint32_t count) {
#if defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_remove_.fetch_add(count, std::memory_order_relaxed);
#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
    uint32_t v = __atomic_load_n(&this->to_remove_, __ATOMIC_RELAXED);
    __atomic_store_n(&this->to_remove_, v + count, __ATOMIC_RELAXED);
#else
  this->to_remove_ += count;
#endif
  }

  void to_remove_decrement_locked_() {
#if defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_remove_.fetch_sub(1, std::memory_order_relaxed);
#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
    uint32_t v = __atomic_load_n(&this->to_remove_, __ATOMIC_RELAXED);
    __atomic_store_n(&this->to_remove_, v - 1, __ATOMIC_RELAXED);
#else
  this->to_remove_--;
#endif
  }

  void to_remove_clear_locked_() {
#if defined(ESPHOME_THREAD_MULTI_ATOMICS)
    this->to_remove_.store(0, std::memory_order_relaxed);
#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
    __atomic_store_n(&this->to_remove_, 0, __ATOMIC_RELAXED);
#else
  this->to_remove_ = 0;
#endif
  }

  uint32_t to_remove_count_() const {
#if defined(ESPHOME_THREAD_MULTI_ATOMICS)
    return this->to_remove_.load(std::memory_order_relaxed);
#elif defined(ESPHOME_THREAD_MULTI_NO_ATOMICS)
    return __atomic_load_n(&this->to_remove_, __ATOMIC_RELAXED);
#else
  return this->to_remove_;
#endif
  }

  // Intrusive freelist threaded through SchedulerItem::next_free. Unbounded so it quiesces at the
  // app's concurrent-timer high-water mark; the previous fixed cap caused steady-state new/delete
  // churn on devices with many timers (see https://github.com/esphome/backlog/issues/52).
  SchedulerItem *scheduler_item_pool_head_{nullptr};
  size_t scheduler_item_pool_size_{0};

#ifdef ESPHOME_DEBUG_SCHEDULER
  // Leak detection: tracks total live SchedulerItem allocations.
  // Invariant: debug_live_items_ == items_.size() + to_add_.size() + defer_queue_.size() + scheduler_item_pool_size_
  // Verified periodically in call() to catch leaks early.
  size_t debug_live_items_{0};

  // Verify the scheduler memory invariant: all allocated items are accounted for.
  // Returns true if no leak detected. Logs an error and asserts on failure.
  bool debug_verify_no_leak_() const;
#endif
};

}  // namespace esphome
