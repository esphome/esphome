#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/version.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <ranges>
#ifdef USE_RUNTIME_STATS
#include "esphome/components/runtime_stats/runtime_stats.h"
#endif

#ifdef USE_STATUS_LED
#include "esphome/components/status_led/status_led.h"
#endif

#ifdef USE_SOCKET_SELECT_SUPPORT
#include <cerrno>

#ifdef USE_SOCKET_IMPL_LWIP_SOCKETS
// LWIP sockets implementation
#include <lwip/sockets.h>
#elif defined(USE_SOCKET_IMPL_BSD_SOCKETS)
// BSD sockets implementation
#ifdef USE_ESP32
// ESP32 "BSD sockets" are actually LWIP under the hood
#include <lwip/sockets.h>
#else
// True BSD sockets (e.g., host platform)
#include <sys/select.h>
#endif
#endif
#endif

namespace esphome {

static const char *const TAG = "app";

// Helper function for insertion sort of components by setup priority
// Using insertion sort instead of std::stable_sort saves ~1.3KB of flash
// by avoiding template instantiations (std::rotate, std::stable_sort, lambdas)
// IMPORTANT: This sort is stable (preserves relative order of equal elements),
// which is necessary to maintain user-defined component order for same priority
template<typename Iterator> static void insertion_sort_by_setup_priority(Iterator first, Iterator last) {
  for (auto it = first + 1; it != last; ++it) {
    auto key = *it;
    float key_priority = key->get_actual_setup_priority();
    auto j = it - 1;

    // Using '<' (not '<=') ensures stability - equal priority components keep their order
    while (j >= first && (*j)->get_actual_setup_priority() < key_priority) {
      *(j + 1) = *j;
      j--;
    }
    *(j + 1) = key;
  }
}

// Helper function for insertion sort of components by loop priority
// IMPORTANT: This sort is stable (preserves relative order of equal elements),
// which is required when components are re-sorted during setup() if they block
template<typename Iterator> static void insertion_sort_by_loop_priority(Iterator first, Iterator last) {
  for (auto it = first + 1; it != last; ++it) {
    auto key = *it;
    float key_priority = key->get_loop_priority();
    auto j = it - 1;

    // Using '<' (not '<=') ensures stability - equal priority components keep their order
    while (j >= first && (*j)->get_loop_priority() < key_priority) {
      *(j + 1) = *j;
      j--;
    }
    *(j + 1) = key;
  }
}

void Application::register_component_(Component *comp) {
  if (comp == nullptr) {
    ESP_LOGW(TAG, "Tried to register null component!");
    return;
  }

  for (auto *c : this->components_) {
    if (comp == c) {
      ESP_LOGW(TAG, "Component %s already registered! (%p)", c->get_component_source(), c);
      return;
    }
  }
  this->components_.push_back(comp);
}
void Application::setup() {
  ESP_LOGI(TAG, "Running through setup()");
  ESP_LOGV(TAG, "Sorting components by setup priority");

  // Sort by setup priority using our helper function
  insertion_sort_by_setup_priority(this->components_.begin(), this->components_.end());

  // Initialize looping_components_ early so enable_pending_loops_() works during setup
  this->calculate_looping_components_();

  for (uint32_t i = 0; i < this->components_.size(); i++) {
    Component *component = this->components_[i];

    // Update loop_component_start_time_ before calling each component during setup
    this->loop_component_start_time_ = millis();
    component->call();
    this->scheduler.process_to_add();
    this->feed_wdt();
    if (component->can_proceed())
      continue;

    // Sort components 0 through i by loop priority
    insertion_sort_by_loop_priority(this->components_.begin(), this->components_.begin() + i + 1);

    do {
      uint8_t new_app_state = STATUS_LED_WARNING;
      uint32_t now = millis();

      // Process pending loop enables to handle GPIO interrupts during setup
      this->before_loop_tasks_(now);

      for (uint32_t j = 0; j <= i; j++) {
        // Update loop_component_start_time_ right before calling each component
        this->loop_component_start_time_ = millis();
        this->components_[j]->call();
        new_app_state |= this->components_[j]->get_component_state();
        this->app_state_ |= new_app_state;
        this->feed_wdt();
      }

      this->after_loop_tasks_();
      this->app_state_ = new_app_state;
      yield();
    } while (!component->can_proceed());
  }

  ESP_LOGI(TAG, "setup() finished successfully!");

  // Clear setup priority overrides to free memory
  clear_setup_priority_overrides();

  this->schedule_dump_config();
}
void Application::loop() {
  uint8_t new_app_state = 0;

  // Get the initial loop time at the start
  uint32_t last_op_end_time = millis();

  this->before_loop_tasks_(last_op_end_time);

  for (this->current_loop_index_ = 0; this->current_loop_index_ < this->looping_components_active_end_;
       this->current_loop_index_++) {
    Component *component = this->looping_components_[this->current_loop_index_];

    // Update the cached time before each component runs
    this->loop_component_start_time_ = last_op_end_time;

    {
      this->set_current_component(component);
      WarnIfComponentBlockingGuard guard{component, last_op_end_time};
      component->call();
      // Use the finish method to get the current time as the end time
      last_op_end_time = guard.finish();
    }
    new_app_state |= component->get_component_state();
    this->app_state_ |= new_app_state;
    this->feed_wdt(last_op_end_time);
  }

  this->after_loop_tasks_();
  this->app_state_ = new_app_state;

#ifdef USE_RUNTIME_STATS
  // Process any pending runtime stats printing after all components have run
  // This ensures stats printing doesn't affect component timing measurements
  if (global_runtime_stats != nullptr) {
    global_runtime_stats->process_pending_stats(last_op_end_time);
  }
#endif

  // Use the last component's end time instead of calling millis() again
  auto elapsed = last_op_end_time - this->last_loop_;
  if (elapsed >= this->loop_interval_ || HighFrequencyLoopRequester::is_high_frequency()) {
    // Even if we overran the loop interval, we still need to select()
    // to know if any sockets have data ready
    this->yield_with_select_(0);
  } else {
    uint32_t delay_time = this->loop_interval_ - elapsed;
    uint32_t next_schedule = this->scheduler.next_schedule_in(last_op_end_time).value_or(delay_time);
    // next_schedule is max 0.5*delay_time
    // otherwise interval=0 schedules result in constant looping with almost no sleep
    next_schedule = std::max(next_schedule, delay_time / 2);
    delay_time = std::min(next_schedule, delay_time);

    this->yield_with_select_(delay_time);
  }
  this->last_loop_ = last_op_end_time;

  if (this->dump_config_at_ < this->components_.size()) {
    if (this->dump_config_at_ == 0) {
      ESP_LOGI(TAG, "ESPHome version " ESPHOME_VERSION " compiled on %s", this->compilation_time_);
#ifdef ESPHOME_PROJECT_NAME
      ESP_LOGI(TAG, "Project " ESPHOME_PROJECT_NAME " version " ESPHOME_PROJECT_VERSION);
#endif
    }

    this->components_[this->dump_config_at_]->call_dump_config();
    this->dump_config_at_++;
  }
}

void IRAM_ATTR HOT Application::feed_wdt(uint32_t time) {
  static uint32_t last_feed = 0;
  // Use provided time if available, otherwise get current time
  uint32_t now = time ? time : millis();
  // Compare in milliseconds (3ms threshold)
  if (now - last_feed > 3) {
    arch_feed_wdt();
    last_feed = now;
#ifdef USE_STATUS_LED
    if (status_led::global_status_led != nullptr) {
      status_led::global_status_led->call();
    }
#endif
  }
}
void Application::reboot() {
  ESP_LOGI(TAG, "Forcing a reboot");
  for (auto &component : std::ranges::reverse_view(this->components_)) {
    component->on_shutdown();
  }
  arch_restart();
}
void Application::safe_reboot() {
  ESP_LOGI(TAG, "Rebooting safely");
  run_safe_shutdown_hooks();
  teardown_components(TEARDOWN_TIMEOUT_REBOOT_MS);
  run_powerdown_hooks();
  arch_restart();
}

void Application::run_safe_shutdown_hooks() {
  for (auto &component : std::ranges::reverse_view(this->components_)) {
    component->on_safe_shutdown();
  }
  for (auto &component : std::ranges::reverse_view(this->components_)) {
    component->on_shutdown();
  }
}

void Application::run_powerdown_hooks() {
  for (auto &component : std::ranges::reverse_view(this->components_)) {
    component->on_powerdown();
  }
}

void Application::teardown_components(uint32_t timeout_ms) {
  uint32_t start_time = millis();

  // Copy all components in reverse order using reverse iterators
  // Reverse order matches the behavior of run_safe_shutdown_hooks() above and ensures
  // components are torn down in the opposite order of their setup_priority (which is
  // used to sort components during Application::setup())
  std::vector<Component *> pending_components(this->components_.rbegin(), this->components_.rend());

  uint32_t now = start_time;
  while (!pending_components.empty() && (now - start_time) < timeout_ms) {
    // Feed watchdog during teardown to prevent triggering
    this->feed_wdt(now);

    // Use iterator to safely erase elements
    for (auto it = pending_components.begin(); it != pending_components.end();) {
      if ((*it)->teardown()) {
        // Component finished teardown, erase it
        it = pending_components.erase(it);
      } else {
        // Component still needs time
        ++it;
      }
    }

    // Give some time for I/O operations if components are still pending
    if (!pending_components.empty()) {
      this->yield_with_select_(1);
    }

    // Update time for next iteration
    now = millis();
  }

  if (!pending_components.empty()) {
    // Note: At this point, connections are either disconnected or in a bad state,
    // so this warning will only appear via serial rather than being transmitted to clients
    for (auto *component : pending_components) {
      ESP_LOGW(TAG, "%s did not complete teardown within %" PRIu32 " ms", component->get_component_source(),
               timeout_ms);
    }
  }
}

void Application::calculate_looping_components_() {
  // Count total components that need looping
  size_t total_looping = 0;
  for (auto *obj : this->components_) {
    if (obj->has_overridden_loop()) {
      total_looping++;
    }
  }

  // Pre-reserve vector to avoid reallocations
  this->looping_components_.reserve(total_looping);

  // Add all components with loop override that aren't already LOOP_DONE
  // Some components (like logger) may call disable_loop() during initialization
  // before setup runs, so we need to respect their LOOP_DONE state
  for (auto *obj : this->components_) {
    if (obj->has_overridden_loop() &&
        (obj->get_component_state() & COMPONENT_STATE_MASK) != COMPONENT_STATE_LOOP_DONE) {
      this->looping_components_.push_back(obj);
    }
  }

  this->looping_components_active_end_ = this->looping_components_.size();

  // Then add any components that are already LOOP_DONE to the inactive section
  // This handles components that called disable_loop() during initialization
  for (auto *obj : this->components_) {
    if (obj->has_overridden_loop() &&
        (obj->get_component_state() & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP_DONE) {
      this->looping_components_.push_back(obj);
    }
  }
}

void Application::disable_component_loop_(Component *component) {
  // This method must be reentrant - components can disable themselves during their own loop() call
  // Linear search to find component in active section
  // Most configs have 10-30 looping components (30 is on the high end)
  // O(n) is acceptable here as we optimize for memory, not complexity
  for (uint16_t i = 0; i < this->looping_components_active_end_; i++) {
    if (this->looping_components_[i] == component) {
      // Move last active component to this position
      this->looping_components_active_end_--;
      if (i != this->looping_components_active_end_) {
        std::swap(this->looping_components_[i], this->looping_components_[this->looping_components_active_end_]);

        // If we're currently iterating and just swapped the current position
        if (this->in_loop_ && i == this->current_loop_index_) {
          // Decrement so we'll process the swapped component next
          this->current_loop_index_--;
          // Update the loop start time to current time so the swapped component
          // gets correct timing instead of inheriting stale timing.
          // This prevents integer underflow in timing calculations by ensuring
          // the swapped component starts with a fresh timing reference, avoiding
          // errors caused by stale or wrapped timing values.
          this->loop_component_start_time_ = millis();
        }
      }
      return;
    }
  }
}

void Application::activate_looping_component_(uint16_t index) {
  // Helper to move component from inactive to active section
  if (index != this->looping_components_active_end_) {
    std::swap(this->looping_components_[index], this->looping_components_[this->looping_components_active_end_]);
  }
  this->looping_components_active_end_++;
}

void Application::enable_component_loop_(Component *component) {
  // This method is only called when component state is LOOP_DONE, so we know
  // the component must be in the inactive section (if it exists in looping_components_)
  // Only search the inactive portion for better performance
  // With typical 0-5 inactive components, O(k) is much faster than O(n)
  const uint16_t size = this->looping_components_.size();
  for (uint16_t i = this->looping_components_active_end_; i < size; i++) {
    if (this->looping_components_[i] == component) {
      // Found in inactive section - move to active
      this->activate_looping_component_(i);
      return;
    }
  }
  // Component not found in looping_components_ - this is normal for components
  // that don't have loop() or were not included in the partitioned vector
}

void Application::enable_pending_loops_() {
  // Process components that requested enable_loop from ISR context
  // Only iterate through inactive looping_components_ (typically 0-5) instead of all components
  //
  // Race condition handling:
  // 1. We check if component is already in LOOP state first - if so, just clear the flag
  //    This handles reentrancy where enable_loop() was called between ISR and processing
  // 2. We only clear pending_enable_loop_ after checking state, preventing lost requests
  // 3. If any components aren't in LOOP_DONE state, we set has_pending_enable_loop_requests_
  //    back to true to ensure we check again next iteration
  // 4. ISRs can safely set flags at any time - worst case is we process them next iteration
  // 5. The global flag (has_pending_enable_loop_requests_) is cleared before this method,
  //    so any ISR that fires during processing will be caught in the next loop
  const uint16_t size = this->looping_components_.size();
  bool has_pending = false;

  for (uint16_t i = this->looping_components_active_end_; i < size; i++) {
    Component *component = this->looping_components_[i];
    if (!component->pending_enable_loop_) {
      continue;  // Skip components without pending requests
    }

    // Check current state
    uint8_t state = component->component_state_ & COMPONENT_STATE_MASK;

    // If already in LOOP state, nothing to do - clear flag and continue
    if (state == COMPONENT_STATE_LOOP) {
      component->pending_enable_loop_ = false;
      continue;
    }

    // If not in LOOP_DONE state, can't enable yet - keep flag set
    if (state != COMPONENT_STATE_LOOP_DONE) {
      has_pending = true;  // Keep tracking this component
      continue;            // Keep the flag set - try again next iteration
    }

    // Clear the pending flag and enable the loop
    component->pending_enable_loop_ = false;
    ESP_LOGVV(TAG, "%s loop enabled from ISR", component->get_component_source());
    component->component_state_ &= ~COMPONENT_STATE_MASK;
    component->component_state_ |= COMPONENT_STATE_LOOP;

    // Move to active section
    this->activate_looping_component_(i);
  }

  // If we couldn't process some requests, ensure we check again next iteration
  if (has_pending) {
    this->has_pending_enable_loop_requests_ = true;
  }
}

void Application::before_loop_tasks_(uint32_t loop_start_time) {
  // Process scheduled tasks
  this->scheduler.call(loop_start_time);

  // Feed the watchdog timer
  this->feed_wdt(loop_start_time);

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

void Application::after_loop_tasks_() {
  // Clear the in_loop_ flag to indicate we're done processing components
  this->in_loop_ = false;
}

#ifdef USE_SOCKET_SELECT_SUPPORT
bool Application::register_socket_fd(int fd) {
  // WARNING: This function is NOT thread-safe and must only be called from the main loop
  // It modifies socket_fds_ and related variables without locking
  if (fd < 0)
    return false;

  if (fd >= FD_SETSIZE) {
    ESP_LOGE(TAG, "Cannot monitor socket fd %d: exceeds FD_SETSIZE (%d)", fd, FD_SETSIZE);
    ESP_LOGE(TAG, "Socket will not be monitored for data - may cause performance issues!");
    return false;
  }

  this->socket_fds_.push_back(fd);
  this->socket_fds_changed_ = true;

  if (fd > this->max_fd_) {
    this->max_fd_ = fd;
  }

  return true;
}

void Application::unregister_socket_fd(int fd) {
  // WARNING: This function is NOT thread-safe and must only be called from the main loop
  // It modifies socket_fds_ and related variables without locking
  if (fd < 0)
    return;

  for (size_t i = 0; i < this->socket_fds_.size(); i++) {
    if (this->socket_fds_[i] != fd)
      continue;

    // Swap with last element and pop - O(1) removal since order doesn't matter
    if (i < this->socket_fds_.size() - 1)
      this->socket_fds_[i] = this->socket_fds_.back();
    this->socket_fds_.pop_back();
    this->socket_fds_changed_ = true;

    // Only recalculate max_fd if we removed the current max
    if (fd == this->max_fd_) {
      this->max_fd_ = -1;
      for (int sock_fd : this->socket_fds_) {
        if (sock_fd > this->max_fd_)
          this->max_fd_ = sock_fd;
      }
    }
    return;
  }
}

bool Application::is_socket_ready(int fd) const {
  // This function is thread-safe for reading the result of select()
  // However, it should only be called after select() has been executed in the main loop
  // The read_fds_ is only modified by select() in the main loop
  if (fd < 0 || fd >= FD_SETSIZE)
    return false;

  return FD_ISSET(fd, &this->read_fds_);
}
#endif

void Application::yield_with_select_(uint32_t delay_ms) {
  // Delay while monitoring sockets. When delay_ms is 0, always yield() to ensure other tasks run
  // since select() with 0 timeout only polls without yielding.
#ifdef USE_SOCKET_SELECT_SUPPORT
  if (!this->socket_fds_.empty()) {
    // Update fd_set if socket list has changed
    if (this->socket_fds_changed_) {
      FD_ZERO(&this->base_read_fds_);
      for (int fd : this->socket_fds_) {
        if (fd >= 0 && fd < FD_SETSIZE) {
          FD_SET(fd, &this->base_read_fds_);
        }
      }
      this->socket_fds_changed_ = false;
    }

    // Copy base fd_set before each select
    this->read_fds_ = this->base_read_fds_;

    // Convert delay_ms to timeval
    struct timeval tv;
    tv.tv_sec = delay_ms / 1000;
    tv.tv_usec = (delay_ms - tv.tv_sec * 1000) * 1000;

    // Call select with timeout
#if defined(USE_SOCKET_IMPL_LWIP_SOCKETS) || (defined(USE_ESP32) && defined(USE_SOCKET_IMPL_BSD_SOCKETS))
    int ret = lwip_select(this->max_fd_ + 1, &this->read_fds_, nullptr, nullptr, &tv);
#else
    int ret = ::select(this->max_fd_ + 1, &this->read_fds_, nullptr, nullptr, &tv);
#endif

    // Process select() result:
    // ret < 0: error (except EINTR which is normal)
    // ret > 0: socket(s) have data ready - normal and expected
    // ret == 0: timeout occurred - normal and expected
    if (ret < 0 && errno != EINTR) {
      // Actual error - log and fall back to delay
      ESP_LOGW(TAG, "select() failed with errno %d", errno);
      delay(delay_ms);
    }
    // When delay_ms is 0, we need to yield since select(0) doesn't yield
    if (delay_ms == 0) {
      yield();
    }
  } else {
    // No sockets registered, use regular delay
    delay(delay_ms);
  }
#else
  // No select support, use regular delay
  delay(delay_ms);
#endif
}

Application App;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome
