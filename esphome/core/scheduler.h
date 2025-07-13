#pragma once

#include <vector>
#include <memory>
#include <cstring>
#include <deque>

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

namespace esphome {

class Component;

class Scheduler {
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
  bool cancel_retry(Component *component, const std::string &name);

  optional<uint32_t> next_schedule_in();

  void call();

  void process_to_add();

 protected:
  struct SchedulerItem {
    // Ordered by size to minimize padding
    Component *component;
    uint32_t interval;
    // 64-bit time to handle millis() rollover. The scheduler combines the 32-bit millis()
    // with a 16-bit rollover counter to create a 64-bit time that won't roll over for
    // billions of years. This ensures correct scheduling even when devices run for months.
    uint64_t next_execution_;

    // Optimized name storage using tagged union
    union {
      const char *static_name;  // For string literals (no allocation)
      char *dynamic_name;       // For allocated strings
    } name_;

    std::function<void()> callback;

    // Bit-packed fields to minimize padding
    enum Type : uint8_t { TIMEOUT, INTERVAL } type : 1;
    bool remove : 1;
    bool name_is_dynamic : 1;  // True if name was dynamically allocated (needs delete[])
    // 5 bits padding

    // Constructor
    SchedulerItem()
        : component(nullptr), interval(0), next_execution_(0), type(TIMEOUT), remove(false), name_is_dynamic(false) {
      name_.static_name = nullptr;
    }

    // Destructor to clean up dynamic names
    ~SchedulerItem() {
      if (name_is_dynamic) {
        delete[] name_.dynamic_name;
      }
    }

    // Delete copy operations to prevent accidental copies
    SchedulerItem(const SchedulerItem &) = delete;
    SchedulerItem &operator=(const SchedulerItem &) = delete;

    // Delete move operations: SchedulerItem objects are only managed via unique_ptr, never moved directly
    SchedulerItem(SchedulerItem &&) = delete;
    SchedulerItem &operator=(SchedulerItem &&) = delete;

    // Helper to get the name regardless of storage type
    const char *get_name() const { return name_is_dynamic ? name_.dynamic_name : name_.static_name; }

    // Helper to set name with proper ownership
    void set_name(const char *name, bool make_copy = false) {
      // Clean up old dynamic name if any
      if (name_is_dynamic && name_.dynamic_name) {
        delete[] name_.dynamic_name;
        name_is_dynamic = false;
      }

      if (!name || !name[0]) {
        name_.static_name = nullptr;
      } else if (make_copy) {
        // Make a copy for dynamic strings
        size_t len = strlen(name);
        name_.dynamic_name = new char[len + 1];
        memcpy(name_.dynamic_name, name, len + 1);
        name_is_dynamic = true;
      } else {
        // Use static string directly
        name_.static_name = name;
      }
    }

    static bool cmp(const std::unique_ptr<SchedulerItem> &a, const std::unique_ptr<SchedulerItem> &b);
    const char *get_type_str() const { return (type == TIMEOUT) ? "timeout" : "interval"; }
    const char *get_source() const { return component ? component->get_component_source() : "unknown"; }
  };

  // Common implementation for both timeout and interval
  void set_timer_common_(Component *component, SchedulerItem::Type type, bool is_static_string, const void *name_ptr,
                         uint32_t delay, std::function<void()> func);

  uint64_t millis_();
  void cleanup_();
  void pop_raw_();

 private:
  // Helper to cancel items by name - must be called with lock held
  bool cancel_item_locked_(Component *component, const char *name, SchedulerItem::Type type);

  // Helper to extract name as const char* from either static string or std::string
  inline const char *get_name_cstr_(bool is_static_string, const void *name_ptr) {
    return is_static_string ? static_cast<const char *>(name_ptr) : static_cast<const std::string *>(name_ptr)->c_str();
  }

  // Common implementation for cancel operations
  bool cancel_item_(Component *component, bool is_static_string, const void *name_ptr, SchedulerItem::Type type);

  // Helper function to check if item matches criteria for cancellation
  inline bool HOT matches_item_(const std::unique_ptr<SchedulerItem> &item, Component *component, const char *name_cstr,
                                SchedulerItem::Type type) {
    if (item->component != component || item->type != type || item->remove) {
      return false;
    }
    const char *item_name = item->get_name();
    if (item_name == nullptr) {
      return false;
    }
    // Fast path: if pointers are equal
    // This is effective because the core ESPHome codebase uses static strings (const char*)
    // for component names. The std::string overloads exist only for compatibility with
    // external components, but are rarely used in practice.
    if (item_name == name_cstr) {
      return true;
    }
    // Slow path: compare string contents
    return strcmp(name_cstr, item_name) == 0;
  }

  // Helper to execute a scheduler item
  void execute_item_(SchedulerItem *item);

  // Helper to check if item should be skipped
  bool should_skip_item_(const SchedulerItem *item) const {
    return item->remove || (item->component != nullptr && item->component->is_failed());
  }

  // Check if the scheduler has no items.
  // IMPORTANT: This method should only be called from the main thread (loop task).
  // It performs cleanup of removed items and checks if the queue is empty.
  // The items_.empty() check at the end is done without a lock for performance,
  // which is safe because this is only called from the main thread while other
  // threads only add items (never remove them).
  bool empty_() {
    this->cleanup_();
    return this->items_.empty();
  }

  Mutex lock_;
  std::vector<std::unique_ptr<SchedulerItem>> items_;
  std::vector<std::unique_ptr<SchedulerItem>> to_add_;
#if !defined(USE_ESP8266) && !defined(USE_RP2040)
  // ESP8266 and RP2040 don't need the defer queue because:
  // ESP8266: Single-core with no preemptive multitasking
  // RP2040: Currently has empty mutex implementation in ESPHome
  // Both platforms save 40 bytes of RAM by excluding this
  std::deque<std::unique_ptr<SchedulerItem>> defer_queue_;  // FIFO queue for defer() calls
#endif
  uint32_t last_millis_{0};
  uint16_t millis_major_{0};
  uint32_t to_remove_{0};
};

}  // namespace esphome
