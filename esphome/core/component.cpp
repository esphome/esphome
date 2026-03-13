#include "esphome/core/component.h"

#include <cinttypes>
#include <limits>
#include <memory>
#include <utility>
#include <vector>
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#ifdef USE_RUNTIME_STATS
#include "esphome/components/runtime_stats/runtime_stats.h"
#endif

namespace esphome {

static const char *const TAG = "component";

// Global vectors for component data that doesn't belong in every instance.
// Using vector instead of unordered_map for both because:
// - Much lower memory overhead (8 bytes per entry vs 20+ for unordered_map)
// - Linear search is fine for small n (typically < 5 entries)
// - These are rarely accessed (setup only or error cases only)

// Component error messages - only stores messages for failed components
// Lazy allocated since most configs have zero failures
// Note: We don't clear this vector because:
// 1. Components are never destroyed in ESPHome
// 2. Failed components remain failed (no recovery mechanism)
// 3. Memory usage is minimal (only failures with custom messages are stored)

// Using namespace-scope static to avoid guard variables (saves 16 bytes total)
// This is safe because ESPHome is single-threaded during initialization
namespace {
struct ComponentErrorMessage {
  const Component *component;
  const char *message;
  // Track if message is flash pointer (needs LOG_STR_ARG) or RAM pointer
  // Remove before 2026.6.0 when deprecated const char* API is removed
  bool is_flash_ptr;
};

#ifdef USE_SETUP_PRIORITY_OVERRIDE
struct ComponentPriorityOverride {
  const Component *component;
  float priority;
};

// Setup priority overrides - freed after setup completes
// Using raw pointer instead of unique_ptr to avoid global constructor/destructor overhead
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<ComponentPriorityOverride> *setup_priority_overrides = nullptr;
#endif

// Error messages for failed components
// Using raw pointer instead of unique_ptr to avoid global constructor/destructor overhead
// This is never freed as error messages persist for the lifetime of the device
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::vector<ComponentErrorMessage> *component_error_messages = nullptr;

// Helper to store error messages - reduces duplication between deprecated and new API
// Remove before 2026.6.0 when deprecated const char* API is removed
void store_component_error_message(const Component *component, const char *message, bool is_flash_ptr) {
  // Lazy allocate the error messages vector if needed
  if (!component_error_messages) {
    component_error_messages = new std::vector<ComponentErrorMessage>();
  }
  // Check if this component already has an error message
  for (auto &entry : *component_error_messages) {
    if (entry.component == component) {
      entry.message = message;
      entry.is_flash_ptr = is_flash_ptr;
      return;
    }
  }
  // Add new error message
  component_error_messages->emplace_back(ComponentErrorMessage{component, message, is_flash_ptr});
}
}  // namespace

// setup_priority, component state, and status LED constants are now
// constexpr in component.h

static constexpr uint16_t WARN_IF_BLOCKING_INCREMENT_MS =
    10U;  ///< How long the blocking time must be larger to warn again

#ifdef USE_LOOP_PRIORITY
float Component::get_loop_priority() const { return 0.0f; }
#endif

float Component::get_setup_priority() const { return setup_priority::DATA; }

void Component::setup() {}

void Component::loop() {}

void Component::set_interval(const std::string &name, uint32_t interval, std::function<void()> &&f) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_interval(this, name, interval, std::move(f));
#pragma GCC diagnostic pop
}

void Component::set_interval(const char *name, uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, name, interval, std::move(f));
}

bool Component::cancel_interval(const std::string &name) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_interval(this, name);
#pragma GCC diagnostic pop
}

bool Component::cancel_interval(const char *name) {  // NOLINT
  return App.scheduler.cancel_interval(this, name);
}

void Component::set_retry(const std::string &name, uint32_t initial_wait_time, uint8_t max_attempts,
                          std::function<RetryResult(uint8_t)> &&f, float backoff_increase_factor) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_retry(this, name, initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
#pragma GCC diagnostic pop
}

void Component::set_retry(const char *name, uint32_t initial_wait_time, uint8_t max_attempts,
                          std::function<RetryResult(uint8_t)> &&f, float backoff_increase_factor) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_retry(this, name, initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
#pragma GCC diagnostic pop
}

bool Component::cancel_retry(const std::string &name) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_retry(this, name);
#pragma GCC diagnostic pop
}

bool Component::cancel_retry(const char *name) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_retry(this, name);
#pragma GCC diagnostic pop
}

void Component::set_timeout(const std::string &name, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_timeout(this, name, timeout, std::move(f));
#pragma GCC diagnostic pop
}

void Component::set_timeout(const char *name, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, name, timeout, std::move(f));
}

bool Component::cancel_timeout(const std::string &name) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_timeout(this, name);
#pragma GCC diagnostic pop
}

bool Component::cancel_timeout(const char *name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}

// uint32_t (numeric ID) overloads - zero heap allocation
void Component::set_timeout(uint32_t id, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, id, timeout, std::move(f));
}

bool Component::cancel_timeout(uint32_t id) { return App.scheduler.cancel_timeout(this, id); }

void Component::set_timeout(InternalSchedulerID id, uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, id, timeout, std::move(f));
}

bool Component::cancel_timeout(InternalSchedulerID id) { return App.scheduler.cancel_timeout(this, id); }

void Component::set_interval(uint32_t id, uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, id, interval, std::move(f));
}

bool Component::cancel_interval(uint32_t id) { return App.scheduler.cancel_interval(this, id); }

void Component::set_interval(InternalSchedulerID id, uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, id, interval, std::move(f));
}

bool Component::cancel_interval(InternalSchedulerID id) { return App.scheduler.cancel_interval(this, id); }

void Component::set_retry(uint32_t id, uint32_t initial_wait_time, uint8_t max_attempts,
                          std::function<RetryResult(uint8_t)> &&f, float backoff_increase_factor) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_retry(this, id, initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
#pragma GCC diagnostic pop
}

bool Component::cancel_retry(uint32_t id) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_retry(this, id);
#pragma GCC diagnostic pop
}

void Component::call_loop_() { this->loop(); }
void Component::call_setup() { this->setup(); }
void Component::call_dump_config_() {
  this->dump_config();
  if (this->is_failed()) {
    // Look up error message from global vector
    const char *error_msg = nullptr;
    bool is_flash_ptr = false;
    if (component_error_messages) {
      for (const auto &entry : *component_error_messages) {
        if (entry.component == this) {
          error_msg = entry.message;
          is_flash_ptr = entry.is_flash_ptr;
          break;
        }
      }
    }
    // Log with appropriate format based on pointer type
    ESP_LOGE(TAG, "  %s is marked FAILED: %s", LOG_STR_ARG(this->get_component_log_str()),
             error_msg ? (is_flash_ptr ? LOG_STR_ARG((const LogString *) error_msg) : error_msg)
                       : LOG_STR_LITERAL("unspecified"));
  }
}

void Component::call() {
  uint8_t state = this->component_state_ & COMPONENT_STATE_MASK;
  switch (state) {
    case COMPONENT_STATE_CONSTRUCTION: {
      // State Construction: Call setup and set state to setup
      this->set_component_state_(COMPONENT_STATE_SETUP);
      ESP_LOGV(TAG, "Setup %s", LOG_STR_ARG(this->get_component_log_str()));
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
      uint32_t start_time = millis();
#endif
      this->call_setup();
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
      uint32_t setup_time = millis() - start_time;
      // Only log at CONFIG level if setup took longer than the blocking threshold
      // to avoid spamming the log and blocking the event loop
      if (setup_time >= WARN_IF_BLOCKING_OVER_MS) {
        ESP_LOGCONFIG(TAG, "Setup %s took %ums", LOG_STR_ARG(this->get_component_log_str()), (unsigned) setup_time);
      } else {
        ESP_LOGV(TAG, "Setup %s took %ums", LOG_STR_ARG(this->get_component_log_str()), (unsigned) setup_time);
      }
#endif
      break;
    }
    case COMPONENT_STATE_SETUP:
      // State setup: Call first loop and set state to loop
      this->set_component_state_(COMPONENT_STATE_LOOP);
      this->call_loop_();
      break;
    case COMPONENT_STATE_LOOP:
      // State loop: Call loop
      this->call_loop_();
      break;
    case COMPONENT_STATE_FAILED:
      // State failed: Do nothing
    case COMPONENT_STATE_LOOP_DONE:
      // State loop done: Do nothing, component has finished its work
    default:
      break;
  }
}
const LogString *Component::get_component_log_str() const {
  return this->component_source_ == nullptr ? LOG_STR("<unknown>") : this->component_source_;
}
bool Component::should_warn_of_blocking(uint32_t blocking_time) {
  if (blocking_time > this->warn_if_blocking_over_) {
    // Prevent overflow when adding increment - if we're about to overflow, just max out
    if (blocking_time + WARN_IF_BLOCKING_INCREMENT_MS < blocking_time ||
        blocking_time + WARN_IF_BLOCKING_INCREMENT_MS > std::numeric_limits<uint16_t>::max()) {
      this->warn_if_blocking_over_ = std::numeric_limits<uint16_t>::max();
    } else {
      this->warn_if_blocking_over_ = static_cast<uint16_t>(blocking_time + WARN_IF_BLOCKING_INCREMENT_MS);
    }
    return true;
  }
  return false;
}
void Component::mark_failed() {
  ESP_LOGE(TAG, "%s was marked as failed", LOG_STR_ARG(this->get_component_log_str()));
  this->set_component_state_(COMPONENT_STATE_FAILED);
  this->status_set_error();
  // Also remove from loop since failed components shouldn't loop
  App.disable_component_loop_(this);
}
void Component::disable_loop() {
  if ((this->component_state_ & COMPONENT_STATE_MASK) != COMPONENT_STATE_LOOP_DONE) {
    ESP_LOGVV(TAG, "%s loop disabled", LOG_STR_ARG(this->get_component_log_str()));
    this->set_component_state_(COMPONENT_STATE_LOOP_DONE);
    App.disable_component_loop_(this);
  }
}
void Component::enable_loop() {
  if ((this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP_DONE) {
    ESP_LOGVV(TAG, "%s loop enabled", LOG_STR_ARG(this->get_component_log_str()));
    this->set_component_state_(COMPONENT_STATE_LOOP);
    App.enable_component_loop_(this);
  }
}
void IRAM_ATTR HOT Component::enable_loop_soon_any_context() {
  // This method is thread and ISR-safe because:
  // 1. Only performs simple assignments to volatile variables (atomic on all platforms)
  // 2. No read-modify-write operations that could be interrupted
  // 3. No memory allocation or object construction; on ESP32 the only call (wake_loop_any_context) is ISR-safe
  // 4. IRAM_ATTR ensures code is in IRAM, not flash (required for ISR execution)
  // 5. Components are never destroyed, so no use-after-free concerns
  // 6. App is guaranteed to be initialized before any ISR could fire
  // 7. Multiple ISR/thread calls are safe - just sets the same flags to true
  // 8. Race condition with main loop is handled by clearing flag before processing
  this->pending_enable_loop_ = true;
  App.has_pending_enable_loop_requests_ = true;
#if (defined(USE_LWIP_FAST_SELECT) && defined(USE_ESP32)) || \
    ((defined(USE_ESP8266) || defined(USE_RP2040)) && defined(USE_SOCKET_IMPL_LWIP_TCP))
  // Wake the main loop from sleep. Without this, the main loop would not
  // wake until the select/delay timeout expires (~16ms).
  // ESP32: uses xPortInIsrContext() to choose the correct FreeRTOS notify API.
  // ESP8266: sets socket wake flag and calls esp_schedule() to exit esp_delay() early.
  // RP2040: sets socket wake flag and calls __sev() to exit __wfe() early.
  Application::wake_loop_any_context();
#endif
}
void Component::reset_to_construction_state() {
  if ((this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_FAILED) {
    ESP_LOGI(TAG, "%s is being reset to construction state", LOG_STR_ARG(this->get_component_log_str()));
    this->set_component_state_(COMPONENT_STATE_CONSTRUCTION);
    // Clear error status when resetting
    this->status_clear_error();
  }
}
void Component::defer(std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, static_cast<const char *>(nullptr), 0, std::move(f));
}
bool Component::cancel_defer(const std::string &name) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return App.scheduler.cancel_timeout(this, name);
#pragma GCC diagnostic pop
}
bool Component::cancel_defer(const char *name) {  // NOLINT
  return App.scheduler.cancel_timeout(this, name);
}
void Component::defer(const std::string &name, std::function<void()> &&f) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_timeout(this, name, 0, std::move(f));
#pragma GCC diagnostic pop
}
void Component::defer(const char *name, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, name, 0, std::move(f));
}
void Component::defer(uint32_t id, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, id, 0, std::move(f));
}
bool Component::cancel_defer(uint32_t id) { return App.scheduler.cancel_timeout(this, id); }
void Component::set_timeout(uint32_t timeout, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_timeout(this, static_cast<const char *>(nullptr), timeout, std::move(f));
}
void Component::set_interval(uint32_t interval, std::function<void()> &&f) {  // NOLINT
  App.scheduler.set_interval(this, static_cast<const char *>(nullptr), interval, std::move(f));
}
void Component::set_retry(uint32_t initial_wait_time, uint8_t max_attempts, std::function<RetryResult(uint8_t)> &&f,
                          float backoff_increase_factor) {  // NOLINT
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  App.scheduler.set_retry(this, "", initial_wait_time, max_attempts, std::move(f), backoff_increase_factor);
#pragma GCC diagnostic pop
}
bool Component::is_ready() const {
  return (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP ||
         (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP_DONE ||
         (this->component_state_ & COMPONENT_STATE_MASK) == COMPONENT_STATE_SETUP;
}
bool Component::can_proceed() { return true; }
bool Component::set_status_flag_(uint8_t flag) {
  if ((this->component_state_ & flag) != 0)
    return false;
  this->component_state_ |= flag;
  App.app_state_ |= flag;
  return true;
}

void Component::status_set_warning(const char *message) {
  if (!this->set_status_flag_(STATUS_LED_WARNING))
    return;
  ESP_LOGW(TAG, "%s set Warning flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? message : LOG_STR_LITERAL("unspecified"));
}
void Component::status_set_warning(const LogString *message) {
  if (!this->set_status_flag_(STATUS_LED_WARNING))
    return;
  ESP_LOGW(TAG, "%s set Warning flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? LOG_STR_ARG(message) : LOG_STR_LITERAL("unspecified"));
}
void Component::status_set_error() { this->status_set_error((const LogString *) nullptr); }
void Component::status_set_error(const char *message) {
  if (!this->set_status_flag_(STATUS_LED_ERROR))
    return;
  ESP_LOGE(TAG, "%s set Error flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? message : LOG_STR_LITERAL("unspecified"));
  if (message != nullptr) {
    store_component_error_message(this, message, false);
  }
}
void Component::status_set_error(const LogString *message) {
  if (!this->set_status_flag_(STATUS_LED_ERROR))
    return;
  ESP_LOGE(TAG, "%s set Error flag: %s", LOG_STR_ARG(this->get_component_log_str()),
           message ? LOG_STR_ARG(message) : LOG_STR_LITERAL("unspecified"));
  if (message != nullptr) {
    // Store the LogString pointer directly (safe because LogString is always in flash/static memory)
    store_component_error_message(this, LOG_STR_ARG(message), true);
  }
}
void Component::status_clear_warning_slow_path_() {
  this->component_state_ &= ~STATUS_LED_WARNING;
  ESP_LOGW(TAG, "%s cleared Warning flag", LOG_STR_ARG(this->get_component_log_str()));
}
void Component::status_clear_error_slow_path_() {
  this->component_state_ &= ~STATUS_LED_ERROR;
  ESP_LOGE(TAG, "%s cleared Error flag", LOG_STR_ARG(this->get_component_log_str()));
}
void Component::status_momentary_warning(const char *name, uint32_t length) {
  this->status_set_warning();
  this->set_timeout(name, length, [this]() { this->status_clear_warning(); });
}
void Component::status_momentary_error(const char *name, uint32_t length) {
  this->status_set_error();
  this->set_timeout(name, length, [this]() { this->status_clear_error(); });
}
void Component::dump_config() {}

// Function implementation of LOG_UPDATE_INTERVAL macro to reduce code size
void log_update_interval(const char *tag, PollingComponent *component) {
  uint32_t update_interval = component->get_update_interval();
  if (update_interval == SCHEDULER_DONT_RUN) {
    ESP_LOGCONFIG(tag, "  Update Interval: never");
  } else if (update_interval < 100) {
    ESP_LOGCONFIG(tag, "  Update Interval: %.3fs", update_interval / 1000.0f);
  } else {
    ESP_LOGCONFIG(tag, "  Update Interval: %.1fs", update_interval / 1000.0f);
  }
}
float Component::get_actual_setup_priority() const {
#ifdef USE_SETUP_PRIORITY_OVERRIDE
  // Check if there's an override in the global vector
  if (setup_priority_overrides) {
    // Linear search is fine for small n (typically < 5 overrides)
    for (const auto &entry : *setup_priority_overrides) {
      if (entry.component == this) {
        return entry.priority;
      }
    }
  }
#endif
  return this->get_setup_priority();
}
#ifdef USE_SETUP_PRIORITY_OVERRIDE
void Component::set_setup_priority(float priority) {
  // Lazy allocate the vector if needed
  if (!setup_priority_overrides) {
    setup_priority_overrides = new std::vector<ComponentPriorityOverride>();
  }

  // Check if this component already has an override
  for (auto &entry : *setup_priority_overrides) {
    if (entry.component == this) {
      entry.priority = priority;
      return;
    }
  }

  // Add new override
  setup_priority_overrides->emplace_back(ComponentPriorityOverride{this, priority});
}
#endif

PollingComponent::PollingComponent(uint32_t update_interval) : update_interval_(update_interval) {}

void PollingComponent::call_setup() {
  // init the poller before calling setup, allowing setup to cancel it if desired
  this->start_poller();
  // Let the polling component subclass setup their HW.
  this->setup();
}

void PollingComponent::start_poller() {
  // Register interval.
  this->set_interval(InternalSchedulerID::POLLING_UPDATE, this->get_update_interval(), [this]() { this->update(); });
}

void PollingComponent::stop_poller() {
  // Clear the interval to suspend component
  this->cancel_interval(InternalSchedulerID::POLLING_UPDATE);
}

uint32_t PollingComponent::get_update_interval() const { return this->update_interval_; }
void PollingComponent::set_update_interval(uint32_t update_interval) { this->update_interval_ = update_interval; }

static void __attribute__((noinline, cold)) warn_blocking(Component *component, uint32_t blocking_time) {
  bool should_warn;
  if (component != nullptr) {
    should_warn = component->should_warn_of_blocking(blocking_time);
  } else {
    should_warn = true;  // Already checked > WARN_IF_BLOCKING_OVER_MS in caller
  }
  if (should_warn) {
    ESP_LOGW(TAG, "%s took a long time for an operation (%" PRIu32 " ms), max is 30 ms",
             component == nullptr ? LOG_STR_LITERAL("<null>") : LOG_STR_ARG(component->get_component_log_str()),
             blocking_time);
  }
}

uint32_t WarnIfComponentBlockingGuard::finish() {
  uint32_t curr_time = millis();
  uint32_t blocking_time = curr_time - this->started_;
#ifdef USE_RUNTIME_STATS
  // Use micros() for accurate sub-millisecond timing. millis() has insufficient
  // resolution — most components complete in microseconds but millis() only has
  // 1ms granularity, so results were essentially random noise.
  if (global_runtime_stats != nullptr) {
    uint32_t duration_us = micros() - this->started_us_;
    global_runtime_stats->record_component_time(this->component_, duration_us);
  }
#endif
  if (blocking_time > WARN_IF_BLOCKING_OVER_MS) {
    warn_blocking(this->component_, blocking_time);
  }
  return curr_time;
}

#ifdef USE_SETUP_PRIORITY_OVERRIDE
void clear_setup_priority_overrides() {
  // Free the setup priority map completely
  delete setup_priority_overrides;
  setup_priority_overrides = nullptr;
}
#endif

}  // namespace esphome
