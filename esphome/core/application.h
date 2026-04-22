#pragma once

#include <algorithm>
#include <ctime>
#include <limits>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"
#include "esphome/core/progmem.h"
#include "esphome/core/scheduler.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/version.h"

#ifdef USE_DEVICES
#include "esphome/core/device.h"
#endif
#ifdef USE_AREAS
#include "esphome/core/area.h"
#endif

#ifdef USE_LWIP_FAST_SELECT
#include "esphome/core/lwip_fast_select.h"
#endif
#ifdef USE_HOST
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#ifdef USE_RUNTIME_STATS
#include "esphome/components/runtime_stats/runtime_stats.h"
#endif
#include "esphome/core/wake.h"
#include "esphome/core/entity_includes.h"

namespace esphome::socket {
#ifdef USE_HOST
/// Shared ready() helper for fd-based socket implementations.
bool socket_ready_fd(int fd, bool loop_monitored);  // NOLINT(readability-redundant-declaration)
#endif
}  // namespace esphome::socket

#ifdef USE_RUNTIME_STATS
namespace esphome::runtime_stats {
class RuntimeStatsCollector;
}  // namespace esphome::runtime_stats
#endif

// Forward declarations for friend access from codegen-generated setup()
void setup();           // NOLINT(readability-redundant-declaration) - may be declared in Arduino.h
void original_setup();  // NOLINT(readability-redundant-declaration) - used by cpp unit tests

namespace esphome {

/// SFINAE helper: detects whether T overrides Component::loop().
/// When &T::loop is ambiguous (multiple inheritance with separate loop() methods),
/// the ambiguity itself proves an override exists, so the true_type default is correct.
template<typename T, typename = void> struct HasLoopOverride : std::true_type {};
template<typename T>
struct HasLoopOverride<T, std::void_t<decltype(&T::loop)>>
    : std::bool_constant<!std::is_same_v<decltype(&T::loop), decltype(&Component::loop)>> {};

// Teardown timeout constant (in milliseconds)
// For reboots, it's more important to shut down quickly than disconnect cleanly
// since we're not entering deep sleep. The only consequence of not shutting down
// cleanly is a warning in the log.
static constexpr uint32_t TEARDOWN_TIMEOUT_REBOOT_MS = 1000;  // 1 second for quick reboot

class Application {
 public:
#ifdef ESPHOME_NAME_ADD_MAC_SUFFIX
  // Called before Logger::pre_setup() — must not log (global_logger is not yet set).
  /// Pre-setup with MAC suffix: overwrites placeholder in mutable static buffers with actual MAC.
  void pre_setup(char *name, size_t name_len, char *friendly_name, size_t friendly_name_len) {
    arch_init();
    this->name_add_mac_suffix_ = true;
    // MAC address suffix length (last 6 characters of 12-char MAC address string)
    constexpr size_t mac_address_suffix_len = 6;
    char mac_addr[MAC_ADDRESS_BUFFER_SIZE];
    get_mac_address_into_buffer(mac_addr);
    // Overwrite the placeholder suffix in the mutable static buffers with actual MAC
    // name is always non-empty (validated by validate_hostname in Python config)
    memcpy(name + name_len - mac_address_suffix_len, mac_addr + mac_address_suffix_len, mac_address_suffix_len);
    if (friendly_name_len > 0) {
      memcpy(friendly_name + friendly_name_len - mac_address_suffix_len, mac_addr + mac_address_suffix_len,
             mac_address_suffix_len);
    }
    this->name_ = StringRef(name, name_len);
    this->friendly_name_ = StringRef(friendly_name, friendly_name_len);
  }
#else
  // Called before Logger::pre_setup() — must not log (global_logger is not yet set).
  /// Pre-setup without MAC suffix: StringRef points directly at const string literals in flash.
  void pre_setup(const char *name, size_t name_len, const char *friendly_name, size_t friendly_name_len) {
    arch_init();
    this->name_add_mac_suffix_ = false;
    this->name_ = StringRef(name, name_len);
    this->friendly_name_ = StringRef(friendly_name, friendly_name_len);
  }
#endif

#ifdef USE_DEVICES
  void register_device(Device *device) { this->devices_.push_back(device); }
#endif
#ifdef USE_AREAS
  void register_area(Area *area) { this->areas_.push_back(area); }
#endif

  void set_current_component(Component *component) { this->current_component_ = component; }
  Component *get_current_component() { return this->current_component_; }

// Entity register methods (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  void register_##singular(type *obj) { this->plural##_.push_back(obj); }
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

#ifdef USE_SERIAL_PROXY
  void register_serial_proxy(serial_proxy::SerialProxy *proxy) {
    proxy->set_instance_index(this->serial_proxies_.size());
    this->serial_proxies_.push_back(proxy);
  }
#endif

  /// Reserve space for components to avoid memory fragmentation

  /// Set up all the registered components. Call this at the end of your setup() function.
  void setup();

  /// Make a loop iteration. Call this in your loop() function.
  inline void ESPHOME_ALWAYS_INLINE loop();

  /// Get the name of this Application set by pre_setup().
  const StringRef &get_name() const { return this->name_; }

  /// Get the friendly name of this Application set by pre_setup().
  const StringRef &get_friendly_name() const { return this->friendly_name_; }

  /// Get the area of this Application set by pre_setup().
  const char *get_area() const {
#ifdef USE_AREAS
    // If we have areas registered, return the name of the first one (which is the top-level area)
    if (!this->areas_.empty() && this->areas_[0] != nullptr) {
      return this->areas_[0]->get_name();
    }
#endif
    return "";
  }

  /// Maximum size of the comment buffer (including null terminator)
  static constexpr size_t ESPHOME_COMMENT_SIZE_MAX = 256;

  /// Copy the comment string into the provided buffer
  void get_comment_string(std::span<char, ESPHOME_COMMENT_SIZE_MAX> buffer);

  /// Get the comment of this Application as a string
  std::string get_comment() {
    char buffer[ESPHOME_COMMENT_SIZE_MAX];
    this->get_comment_string(buffer);
    return std::string(buffer);
  }

  bool is_name_add_mac_suffix_enabled() const { return this->name_add_mac_suffix_; }

  /// Size of buffer required for build time string (including null terminator)
  static constexpr size_t BUILD_TIME_STR_SIZE = 26;

  /// Get the config hash as a 32-bit integer
  uint32_t get_config_hash();

  /// Get the config hash extended with ESPHome version
  uint32_t get_config_version_hash();

  /// Get the build time as a Unix timestamp
  time_t get_build_time();

  /// Copy the build time string into the provided buffer
  /// Buffer must be BUILD_TIME_STR_SIZE bytes (compile-time enforced)
  void get_build_time_string(std::span<char, BUILD_TIME_STR_SIZE> buffer);

  /// Get the build time as a string (deprecated, use get_build_time_string() instead)
  // Remove before 2026.7.0
  ESPDEPRECATED("Use get_build_time_string() instead. Removed in 2026.7.0", "2026.1.0")
  std::string get_compilation_time() {
    char buf[BUILD_TIME_STR_SIZE];
    this->get_build_time_string(buf);
    return std::string(buf);
  }

  /// Get the cached time in milliseconds from when the current component started its loop execution
  inline uint32_t IRAM_ATTR HOT get_loop_component_start_time() const { return this->loop_component_start_time_; }

  /** Set the target interval with which to run the loop() calls.
   * If the loop() method takes longer than the target interval, ESPHome won't
   * sleep in loop(), but if the time spent in loop() is small than the target, ESPHome
   * will delay at the end of the App.loop() method.
   *
   * This is done to conserve power: In most use-cases, high-speed loop() calls are not required
   * and degrade power consumption.
   *
   * Each component can request a high frequency loop execution by using the HighFrequencyLoopRequester
   * helper in helpers.h
   *
   * Note: This method is not called by ESPHome core code. It is only used by lambda functions
   * in YAML configurations or by external components.
   *
   * @param loop_interval The interval in milliseconds to run the core loop at. Defaults to 16 milliseconds.
   */
  void set_loop_interval(uint32_t loop_interval) {
    this->loop_interval_ = std::min(loop_interval, static_cast<uint32_t>(std::numeric_limits<uint16_t>::max()));
  }

  uint32_t get_loop_interval() const { return static_cast<uint32_t>(this->loop_interval_); }

  void schedule_dump_config() { this->dump_config_at_ = 0; }

  /// Minimum interval between real arch_feed_wdt() calls. Sized so the outer
  /// feed in Application::loop() is effectively rate-limited across both the
  /// normal ~62 Hz cadence and worst-case wake-storm scenarios (e.g. external
  /// stacks like OpenThread posting frequent wake notifications). Component
  /// loops and scheduler items still feed after every op, so any op exceeding
  /// this threshold triggers a real feed naturally.
  /// Safety margins vs. platform watchdog timeouts:
  ///   - ESP32 task WDT default (5 s): ~16x
  ///   - ESP8266 soft WDT (~1.6 s):    ~5x  <-- floor case; any future change
  ///                                             must keep comfortable margin here
  ///   - ESP8266 HW WDT (~6 s):        ~20x
  static constexpr uint32_t WDT_FEED_INTERVAL_MS = 300;

  /// Feed the task watchdog. Cold entry — callers without a millis()
  /// timestamp in hand. Out of line to keep call sites tiny.
  void feed_wdt();

#ifdef USE_STATUS_LED
  /// Dispatch interval for the status LED update. Deliberately shorter than
  /// WDT_FEED_INTERVAL_MS because the status LED error blink has a 250 ms
  /// period (status_led.cpp:ERROR_PERIOD_MS) and a 150 ms on-window; the
  /// dispatch cadence must be short enough to render that blink without
  /// aliasing. Sampling every 100 ms yields an on/off observation inside
  /// every error period with headroom for the 250 ms warning on-window.
  static constexpr uint32_t STATUS_LED_DISPATCH_INTERVAL_MS = 100;
#endif

  /// Feed the task watchdog, hot entry. Callers that already have a
  /// millis() timestamp pay only a load + sub + branch on the common
  /// (no-op) path. The actual arch feed lives in feed_wdt_slow_.
  /// When USE_STATUS_LED is compiled in, also gates a separate (shorter)
  /// interval for dispatching status_led so the LED blink pattern stays
  /// readable even though arch_feed_wdt pokes are now rate-limited at
  /// WDT_FEED_INTERVAL_MS. The two rate limits are independent so raising
  /// WDT_FEED_INTERVAL_MS does not distort the LED cadence.
  void ESPHOME_ALWAYS_INLINE feed_wdt_with_time(uint32_t time) {
    if (static_cast<uint32_t>(time - this->last_wdt_feed_) > WDT_FEED_INTERVAL_MS) [[unlikely]] {
      this->feed_wdt_slow_(time);
    }
#ifdef USE_STATUS_LED
    if (static_cast<uint32_t>(time - this->last_status_led_service_) > STATUS_LED_DISPATCH_INTERVAL_MS) [[unlikely]] {
      this->service_status_led_slow_(time);
    }
#endif
  }

  void reboot();

  void safe_reboot();

  void run_safe_shutdown_hooks();

  void run_powerdown_hooks();

  /** Teardown all components with a timeout.
   *
   * @param timeout_ms Maximum time to wait for teardown in milliseconds
   */
  void teardown_components(uint32_t timeout_ms);

  /// Return the public app state status bits (STATUS_LED_* only).
  /// Internal bookkeeping bits like APP_STATE_SETUP_COMPLETE are masked
  /// out so external readers (status_led components, etc.) never see them.
  uint8_t get_app_state() const { return this->app_state_ & ~APP_STATE_SETUP_COMPLETE; }

  /// True once Application::setup() has finished walking all components
  /// and finalized the initial status flags. Before this point, the
  /// slow-setup busy-wait may be forcing STATUS_LED_WARNING on, and
  /// status_clear_* intentionally skips its walk-and-clear step so the
  /// forced bit doesn't get wiped. Stored as a free bit on app_state_
  /// (bit 6) to avoid costing additional RAM.
  bool is_setup_complete() const { return (this->app_state_ & APP_STATE_SETUP_COMPLETE) != 0; }

// Helper macro for entity getter method declarations
#ifdef USE_DEVICES
#define GET_ENTITY_METHOD(entity_type, entity_name, entities_member) \
  entity_type *get_##entity_name##_by_key(uint32_t key, uint32_t device_id, bool include_internal = false) { \
    for (auto *obj : this->entities_member##_) { \
      if (obj->get_object_id_hash() == key && obj->get_device_id() == device_id && \
          (include_internal || !obj->is_internal())) \
        return obj; \
    } \
    return nullptr; \
  }
  const auto &get_devices() { return this->devices_; }
#else
#define GET_ENTITY_METHOD(entity_type, entity_name, entities_member) \
  entity_type *get_##entity_name##_by_key(uint32_t key, bool include_internal = false) { \
    for (auto *obj : this->entities_member##_) { \
      if (obj->get_object_id_hash() == key && (include_internal || !obj->is_internal())) \
        return obj; \
    } \
    return nullptr; \
  }
#endif  // USE_DEVICES
#ifdef USE_AREAS
  const auto &get_areas() { return this->areas_; }
#endif
// Entity getter methods (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) \
  auto &get_##plural() const { return this->plural##_; } \
  GET_ENTITY_METHOD(type, singular, plural)
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

#ifdef USE_SERIAL_PROXY
  auto &get_serial_proxies() const { return this->serial_proxies_; }
#endif

  Scheduler scheduler;

#ifdef USE_HOST
  /// Register/unregister a socket file descriptor with the host select() fallback loop.
  /// USE_LWIP_FAST_SELECT builds do not use this API — sockets hook the lwIP netconn
  /// event_callback directly (see socket.h hook_fd_for_fast_select) and rely on FreeRTOS
  /// task notifications for wake-up.
  /// NOTE: File descriptors >= FD_SETSIZE (typically 10 on ESP) will be rejected with an error.
  /// WARNING: These functions are NOT thread-safe. They must only be called from the main loop.
  /// @return true if registration was successful, false if fd exceeds limits
  bool register_socket_fd(int fd);
  void unregister_socket_fd(int fd);
#endif

  /// Wake the main event loop from another thread or callback.
  /// @see esphome::wake_loop_threadsafe() in wake.h for platform details.
  void wake_loop_threadsafe() { esphome::wake_loop_threadsafe(); }

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  /// Wake from ISR (ESP32 and LibreTiny).
  static void IRAM_ATTR wake_loop_isrsafe(BaseType_t *px) { esphome::wake_loop_isrsafe(px); }
#elif defined(USE_ESP8266)
  /// Wake from ISR (ESP8266). No task_woken arg — no FreeRTOS. Caller must be IRAM_ATTR.
  static void IRAM_ATTR ESPHOME_ALWAYS_INLINE wake_loop_isrsafe() { esphome::wake_loop_isrsafe(); }
#endif

  /// Wake from any context (ISR, thread, callback).
  static void IRAM_ATTR wake_loop_any_context() { esphome::wake_loop_any_context(); }

 protected:
  friend Component;
#ifdef USE_HOST
  friend bool socket::socket_ready_fd(int fd, bool loop_monitored);
#endif
#ifdef USE_RUNTIME_STATS
  friend class runtime_stats::RuntimeStatsCollector;
#endif
  friend void ::setup();
  friend void ::original_setup();
#ifdef USE_HOST
  friend void wake_loop_threadsafe();  // Host platform accesses wake_socket_fd_
#endif

#ifdef USE_HOST
  bool is_socket_ready_(int fd) const { return FD_ISSET(fd, &this->read_fds_); }
#endif

  /// Walk all registered components looking for any whose component_state_
  /// has the given flag set. Used by Component::status_clear_*_slow_path_()
  /// (which is a friend) to decide whether to clear the corresponding bit on
  /// this->app_state_ (the app-wide "any component has this status" indicator).
  bool any_component_has_status_flag_(uint8_t flag) const;

  /// Register a component, detecting loop() override at compile time.
  /// Uses HasLoopOverride<T> which handles ambiguous &T::loop from multiple inheritance.
  template<typename T> void register_component_(T *comp) {
    this->register_component_impl_(comp, HasLoopOverride<T>::value);
  }

  void register_component_impl_(Component *comp, bool has_loop);

  void calculate_looping_components_() {
    // FixedVector capacity was pre-initialized by codegen with the exact count
    // of components that override loop(), computed at C++ compile time.

    // Add all components with loop override that aren't already LOOP_DONE
    // Some components (like logger) may call disable_loop() during initialization
    // before setup runs, so we need to respect their LOOP_DONE state
    this->add_looping_components_by_state_(false);
    this->looping_components_active_end_ = this->looping_components_.size();
    // Then add any components that are already LOOP_DONE to the inactive section
    // This handles components that called disable_loop() during initialization
    this->add_looping_components_by_state_(true);
  }
  void add_looping_components_by_state_(bool match_loop_done);

  // These methods are called by Component::disable_loop() and Component::enable_loop()
  // Components should not call these directly - use this->disable_loop() or this->enable_loop()
  // to ensure component state is properly updated along with the loop partition
  void disable_component_loop_(Component *component);
  void enable_component_loop_(Component *component);
  void enable_pending_loops_();
  void activate_looping_component_(uint16_t index);
  inline uint32_t ESPHOME_ALWAYS_INLINE scheduler_tick_(uint32_t now);
  inline void ESPHOME_ALWAYS_INLINE before_component_phase_();
  inline void ESPHOME_ALWAYS_INLINE after_component_phase_() { this->in_loop_ = false; }

  /// Process dump_config output one component per loop iteration.
  /// Extracted from loop() to keep cold startup/reconnect logging out of the hot path.
  /// Caller must ensure dump_config_at_ < components_.size().
  void __attribute__((noinline)) process_dump_config_();

  /// Slow path for feed_wdt(): actually calls arch_feed_wdt() and updates
  /// last_wdt_feed_. Out of line so the inline wrapper stays tiny. Does NOT
  /// touch status_led — that's gated separately via service_status_led_slow_
  /// because the two rate limits have very different safe ranges (~ seconds
  /// for WDT, < 250 ms for LED blink rendering).
  void feed_wdt_slow_(uint32_t time);

#ifdef USE_STATUS_LED
  /// Slow path for the status_led dispatch rate limit. Runs the status_led
  /// component's loop() based on its state (LOOP / LOOP_DONE with status
  /// bits set), and updates last_status_led_service_. Out of line to keep
  /// the feed_wdt_with_time hot path a couple of load+branch sequences.
  void service_status_led_slow_(uint32_t time);
#endif

  /// Perform a delay while also monitoring socket file descriptors for readiness
#ifdef USE_HOST
  // select() fallback path is too complex to inline (host platform)
  void yield_with_select_(uint32_t delay_ms);
#else
  inline void ESPHOME_ALWAYS_INLINE yield_with_select_(uint32_t delay_ms);
#endif

#ifdef USE_HOST
  void setup_wake_loop_threadsafe_();       // Create wake notification socket
  inline void drain_wake_notifications_();  // Read pending wake notifications in main loop (hot path - inlined)
#endif

  // === Member variables ordered by size to minimize padding ===

  // Pointer-sized members first
  Component *current_component_{nullptr};

  // std::vector (3 pointers each: begin, end, capacity)
  // Partitioned vector design for looping components
  // =================================================
  // Components are partitioned into [active | inactive] sections:
  //
  // looping_components_: [A, B, C, D | E, F]
  //                                  ^
  //                      looping_components_active_end_ (4)
  //
  // - Components A,B,C,D are active and will be called in loop()
  // - Components E,F are inactive (disabled/failed) and won't be called
  // - No flag checking needed during iteration - just loop 0 to active_end_
  // - When a component is disabled, it's swapped with the last active component
  //   and active_end_ is decremented
  // - When a component is enabled, it's swapped with the first inactive component
  //   and active_end_ is incremented
  // - This eliminates branch mispredictions from flag checking in the hot loop
  FixedVector<Component *> looping_components_{};
#ifdef USE_HOST
  std::vector<int> socket_fds_;  // Vector of all monitored socket file descriptors
#endif
#ifdef USE_HOST
  int wake_socket_fd_{-1};  // Shared wake notification socket for waking main loop from tasks
#endif

  // StringRef members (8 bytes each: pointer + size)
  StringRef name_;
  StringRef friendly_name_;

  // 4-byte members
  uint32_t last_loop_{0};
  uint32_t loop_component_start_time_{0};
  uint32_t last_wdt_feed_{0};  // millis() of most recent arch_feed_wdt(); rate-limits feed_wdt() hot path
#ifdef USE_STATUS_LED
  // millis() of most recent status_led dispatch; rate-limits independently of last_wdt_feed_
  uint32_t last_status_led_service_{0};
#endif

#ifdef USE_HOST
  int max_fd_{-1};  // Highest file descriptor number for select()
#endif

  // 2-byte members (grouped together for alignment)
  uint16_t dump_config_at_{std::numeric_limits<uint16_t>::max()};  // Index into components_ for dump_config progress
  uint16_t loop_interval_{16};                                     // Loop interval in ms (max 65535ms = 65.5 seconds)
  uint16_t looping_components_active_end_{0};  // Index marking end of active components in looping_components_
  uint16_t current_loop_index_{0};             // For safe reentrant modifications during iteration

  // 1-byte members (grouped together to minimize padding)
  uint8_t app_state_{0};
  bool name_add_mac_suffix_;
  bool in_loop_{false};
  volatile bool has_pending_enable_loop_requests_{false};

#ifdef USE_HOST
  bool socket_fds_changed_{false};  // Flag to rebuild base_read_fds_ when socket_fds_ changes
#endif

#ifdef USE_HOST
  // Variable-sized members (not needed with fast select — is_socket_ready_ reads rcvevent directly)
  fd_set read_fds_{};       // Working fd_set: populated by select()
  fd_set base_read_fds_{};  // Cached fd_set rebuilt only when socket_fds_ changes
#endif

  // StaticVectors (largest members - contain actual array data inline)
  StaticVector<Component *, ESPHOME_COMPONENT_COUNT> components_{};

#ifdef USE_DEVICES
  StaticVector<Device *, ESPHOME_DEVICE_COUNT> devices_{};
#endif
#ifdef USE_AREAS
  StaticVector<Area *, ESPHOME_AREA_COUNT> areas_{};
#endif
// Entity StaticVector fields (generated from entity_types.h)
// NOLINTBEGIN(bugprone-macro-parentheses)
#define ENTITY_TYPE_(type, singular, plural, count, upper) StaticVector<type *, count> plural##_{};
#define ENTITY_CONTROLLER_TYPE_(type, singular, plural, count, upper, callback) \
  ENTITY_TYPE_(type, singular, plural, count, upper)
#include "esphome/core/entity_types.h"
#undef ENTITY_TYPE_
#undef ENTITY_CONTROLLER_TYPE_
  // NOLINTEND(bugprone-macro-parentheses)

#ifdef USE_SERIAL_PROXY
  StaticVector<serial_proxy::SerialProxy *, SERIAL_PROXY_COUNT> serial_proxies_{};
#endif
};

/// Global storage of Application pointer - only one Application can exist.
extern Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#ifdef USE_HOST
// Inline implementations for hot-path functions
// drain_wake_notifications_() is called on every loop iteration

// Small buffer for draining wake notification bytes (1 byte sent per wake)
// Size allows draining multiple notifications per recvfrom() without wasting stack
static constexpr size_t WAKE_NOTIFY_DRAIN_BUFFER_SIZE = 16;

inline void Application::drain_wake_notifications_() {
  // Called from main loop to drain any pending wake notifications
  // Must check is_socket_ready_() to avoid blocking on empty socket
  if (this->wake_socket_fd_ >= 0 && this->is_socket_ready_(this->wake_socket_fd_)) {
    char buffer[WAKE_NOTIFY_DRAIN_BUFFER_SIZE];
    // Drain all pending notifications with non-blocking reads
    // Multiple wake events may have triggered multiple writes, so drain until EWOULDBLOCK
    // We control both ends of this loopback socket (always write 1 byte per wake),
    // so no error checking needed - any errors indicate catastrophic system failure
    while (::recvfrom(this->wake_socket_fd_, buffer, sizeof(buffer), 0, nullptr, nullptr) > 0) {
      // Just draining, no action needed - wake has already occurred
    }
  }
}
#endif  // USE_HOST

// Phase A: drain wake notifications and run the scheduler. Invoked on every
// Application::loop() tick regardless of whether a component phase runs, so
// scheduler items fire at their requested cadence even when the caller has
// raised loop_interval_ for power savings (see Application::loop()).
// Returns the timestamp of the last scheduler item that ran (or `now`
// unchanged if none ran), so the caller's WDT feed stays monotonic with the
// per-item feeds inside scheduler.call() without an extra millis().
inline uint32_t ESPHOME_ALWAYS_INLINE Application::scheduler_tick_(uint32_t now) {
#ifdef USE_HOST
  // Drain wake notifications first to clear socket for next wake
  this->drain_wake_notifications_();
#endif
  return this->scheduler.call(now);
}

// Phase B entry: only invoked when a component loop phase is about to run.
// Processes pending enable_loop requests from ISRs and marks in_loop_ so
// reentrant modifications during component.loop() are safe.
inline void ESPHOME_ALWAYS_INLINE Application::before_component_phase_() {
  // Process any pending enable_loop requests from ISRs
  // This must be done before marking in_loop_ = true to avoid race conditions
  if (this->has_pending_enable_loop_requests_) {
    // Clear flag BEFORE processing to avoid race condition
    // If ISR sets it during processing, we'll catch it next loop iteration
    // This is safe because:
    // 1. Each component has its own pending_enable_loop_ flag that we check
    // 2. If we can't process a component (wrong state), enable_pending_loops_()
    //    will set this flag back to true
    // 3. Any new ISR requests during processing will set the flag again
    this->has_pending_enable_loop_requests_ = false;
    this->enable_pending_loops_();
  }

  // Mark that we're in the loop for safe reentrant modifications
  this->in_loop_ = true;
}

inline void ESPHOME_ALWAYS_INLINE Application::loop() {
#ifdef USE_RUNTIME_STATS
  // Capture the start of the active (non-sleeping) portion of this iteration.
  // Used to derive main-loop overhead = active time − Σ(component time) −
  // before/tail splits recorded below.
  uint32_t loop_active_start_us = micros();
  // Snapshot the cumulative component-recorded time so we can subtract the
  // slice that the scheduler spends inside its own WarnIfComponentBlockingGuard
  // (scheduler.cpp) — that time is already counted in per-component stats,
  // so charging it again to "before" would double-count.
  uint64_t loop_recorded_snap = ComponentRuntimeStats::global_recorded_us;
#endif
  // Phase A: always service the scheduler. Decouples scheduler cadence from
  // loop_interval_ so raised intervals (for power savings) don't drag scheduled
  // items forward. A tick that only runs the scheduler is cheap.
  // scheduler_tick_ returns the timestamp of the last scheduler item that ran
  // (advanced by its per-item feeds) or `now` unchanged. We adopt it as `now`
  // so the gate check and WDT feed both reflect actual elapsed time after
  // scheduler dispatch, without an extra millis() call.
  uint32_t now = this->scheduler_tick_(MillisInternal::get());
  // Guarantee one WDT feed per tick even when the scheduler had nothing to
  // dispatch and the component phase is gated out — covers configs with no
  // looping components and no scheduler work (setup() has its own
  // per-component feed_wdt calls, so only do this here, not in scheduler_tick_).
  this->feed_wdt_with_time(now);

#ifdef USE_RUNTIME_STATS
  uint32_t loop_before_end_us = micros();
  uint64_t loop_before_scheduled_us = ComponentRuntimeStats::global_recorded_us - loop_recorded_snap;
  // Only meaningful when do_component_phase is true; initialized to 0 so the
  // tail bucket receives 0 on Phase A-only ticks (no component tail happened,
  // the gate-check / stats-prefix overhead belongs to "residual", not "tail").
  uint32_t loop_tail_start_us = 0;
#endif

  // Gate the component phase on loop_interval_, an active high-frequency
  // request, or an explicit wake from a background producer. A scheduler-only
  // wake (e.g. set_interval firing under a raised loop_interval_) leaves the
  // component phase gated; an external producer that called wake_loop_*
  // (MQTT RX, USB RX, BLE event, etc.) needs the component phase to actually
  // run so its component's loop() can drain the queued work — that is the
  // long-standing semantic of wake_loop_threadsafe(), and the wake_request
  // flag preserves it. wake_request_take() exchange-clears the flag; wakes
  // that arrive during Phase B re-set it and run Phase B again on the next
  // iteration.
  const bool high_frequency = HighFrequencyLoopRequester::is_high_frequency();
  const uint32_t elapsed = now - this->last_loop_;
  const bool woke = esphome::wake_request_take();
  const bool do_component_phase = high_frequency || woke || (elapsed >= this->loop_interval_);

  if (do_component_phase) {
    this->before_component_phase_();

    uint32_t last_op_end_time = now;
    for (this->current_loop_index_ = 0; this->current_loop_index_ < this->looping_components_active_end_;
         this->current_loop_index_++) {
      Component *component = this->looping_components_[this->current_loop_index_];

      // Update the cached time before each component runs
      this->loop_component_start_time_ = last_op_end_time;

      {
        this->set_current_component(component);
        WarnIfComponentBlockingGuard guard{component, last_op_end_time};
        component->loop();
        // Use the finish method to get the current time as the end time
        last_op_end_time = guard.finish();
      }
      this->feed_wdt_with_time(last_op_end_time);
    }

#ifdef USE_RUNTIME_STATS
    loop_tail_start_us = micros();
#endif
    this->last_loop_ = last_op_end_time;
    now = last_op_end_time;
    this->after_component_phase_();
  }

#ifdef USE_RUNTIME_STATS
  // Record per-tick timing on every loop, not just component-phase ticks.
  // record_loop_active is a small accumulator; process_pending_stats is an
  // inline gate check that early-outs unless now >= next_log_time_.
  if (global_runtime_stats != nullptr) {
    uint32_t loop_now_us = micros();
    // Subtract scheduled-component time from the "before" bucket so it is
    // not double-counted (it is already attributed to per-component stats).
    uint32_t loop_before_wall_us = loop_before_end_us - loop_active_start_us;
    uint32_t loop_before_overhead_us = loop_before_wall_us > loop_before_scheduled_us
                                           ? loop_before_wall_us - static_cast<uint32_t>(loop_before_scheduled_us)
                                           : 0;
    // tail_us is only defined when Phase B ran; 0 on Phase A-only ticks so the
    // stats bucket keeps its "component-phase trailing overhead" meaning.
    uint32_t loop_tail_us = do_component_phase ? (loop_now_us - loop_tail_start_us) : 0;
    global_runtime_stats->record_loop_active(loop_now_us - loop_active_start_us, loop_before_overhead_us, loop_tail_us);
    global_runtime_stats->process_pending_stats(now);
  }
#endif

  // Compute sleep: bounded by time-until-next-component-phase and the
  // scheduler's next deadline. When a scheduler timer fires it re-enters
  // loop(), Phase A services it, and the component phase stays gated by
  // loop_interval_. When a background producer calls wake_loop_threadsafe()
  // it sets the wake_request flag and wakes select() / the task notification;
  // the gate above sees the flag and runs Phase B too so the producer's
  // component can drain its queued work without waiting up to loop_interval_.
  //
  // Re-read HighFrequencyLoopRequester::is_high_frequency() here instead of
  // reusing the cached `high_frequency` captured above: a component calling
  // HighFrequencyLoopRequester::start() from within its loop() would
  // otherwise sit under the stale value and sleep for up to loop_interval_
  // before the request took effect. That was fine pre-decoupling (the old
  // main loop also called the function fresh at the sleep point) but now
  // matters much more — loop_interval_ is a power-saving knob documented
  // to accept multi-second values, so the stale path could add seconds of
  // latency on an HF request. The call is a trivial atomic read.
  uint32_t delay_time = 0;
  if (!HighFrequencyLoopRequester::is_high_frequency()) {
    const uint32_t elapsed_since_phase = now - this->last_loop_;
    const uint32_t until_phase =
        (elapsed_since_phase >= this->loop_interval_) ? 0 : (this->loop_interval_ - elapsed_since_phase);
    const uint32_t until_sched = this->scheduler.next_schedule_in(now).value_or(until_phase);
    delay_time = std::min(until_phase, until_sched);
  }
  this->yield_with_select_(delay_time);

  if (this->dump_config_at_ < this->components_.size()) {
    this->process_dump_config_();
  }
}

// Inline yield_with_select_ for all paths except the select() fallback
#ifndef USE_HOST
inline void ESPHOME_ALWAYS_INLINE Application::yield_with_select_(uint32_t delay_ms) {
#ifdef USE_LWIP_FAST_SELECT
  // Fast path (ESP32/LibreTiny): FreeRTOS task notifications posted by the lwip
  // event_callback wrapper (see lwip_fast_select.c) are the single source of truth for
  // socket wake-ups. Every NETCONN_EVT_RCVPLUS posts an xTaskNotifyGive, so any notification
  // that lands between wakes keeps the counter non-zero (next ulTaskNotifyTake returns
  // immediately) or wakes a blocked Take directly. Additional wake sources:
  // wake_loop_threadsafe() from background tasks, and the delay_ms timeout.
  if (delay_ms == 0) [[unlikely]] {
    yield();
    return;
  }
#endif
  esphome::internal::wakeable_delay(delay_ms);
}
#endif  // !USE_HOST

}  // namespace esphome
