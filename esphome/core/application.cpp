#include "esphome/core/application.h"
#include "esphome/core/build_info_data.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cstring>

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif
#ifdef USE_ESP32
#include <esp_chip_info.h>
#include <esp_ota_ops.h>
#include <esp_bootloader_desc.h>
#endif
#include "esphome/core/version.h"
#include "esphome/core/hal.h"
#include <algorithm>
#include <ranges>

#ifdef USE_STATUS_LED
#include "esphome/components/status_led/status_led.h"
#endif

namespace esphome {

static const char *const TAG = "app";

// Delay after setup() finishes before trimming the scheduler freelist of its post-boot peak.
// 10 s is well past the bulk of post-setup async work (Wi-Fi/MQTT connects, first-read latency).
static constexpr uint32_t SCHEDULER_FREELIST_TRIM_DELAY_MS = 10000;

// Helper function for insertion sort of components by priority
// Using insertion sort instead of std::stable_sort saves ~1.3KB of flash
// by avoiding template instantiations (std::rotate, std::stable_sort, lambdas)
// IMPORTANT: This sort is stable (preserves relative order of equal elements),
// which is necessary to maintain user-defined component order for same priority
template<typename Iterator, float (Component::*GetPriority)() const>
static void insertion_sort_by_priority(Iterator first, Iterator last) {
  for (auto it = first + 1; it != last; ++it) {
    auto key = *it;
    float key_priority = (key->*GetPriority)();
    auto j = it - 1;

    // Using '<' (not '<=') ensures stability - equal priority components keep their order
    while (j >= first && ((*j)->*GetPriority)() < key_priority) {
      *(j + 1) = *j;
      j--;
    }
    *(j + 1) = key;
  }
}

void Application::register_component_impl_(Component *comp, bool has_loop) {
  if (has_loop) {
    comp->component_state_ |= COMPONENT_HAS_LOOP;
  }
  this->components_.push_back(comp);
}
void Application::setup() {
  ESP_LOGI(TAG, "Running through setup()");
  ESP_LOGV(TAG, "Sorting components by setup priority");

  // Sort by setup priority using our helper function
  insertion_sort_by_priority<decltype(this->components_.begin()), &Component::get_actual_setup_priority>(
      this->components_.begin(), this->components_.end());

  // Initialize looping_components_ early so enable_pending_loops_() works during setup
  this->calculate_looping_components_();

  for (uint32_t i = 0; i < this->components_.size(); i++) {
    Component *component = this->components_[i];

    // Update loop_component_start_time_ before calling each component during setup
    this->loop_component_start_time_ = MillisInternal::get();
    component->call();
    this->scheduler.process_to_add();
    this->feed_wdt();
    if (component->can_proceed())
      continue;

    // Force the status LED to blink WARNING while we wait for a slow
    // component to come up. Cleared after setup() finishes if no real
    // component has warning set.
    this->app_state_ |= STATUS_LED_WARNING;

    do {
      // Service scheduler and process pending loop enables to handle GPIO
      // interrupts during setup. During setup we always run the component
      // phase (no loop_interval_ gate), so call both helpers unconditionally.
      this->scheduler_tick_(MillisInternal::get());
      {
        ComponentPhaseGuard phase_guard{*this};

        for (uint32_t j = 0; j <= i; j++) {
          // Update loop_component_start_time_ right before calling each component
          this->loop_component_start_time_ = MillisInternal::get();
          this->components_[j]->call();
          this->feed_wdt();
        }
      }
      yield();
    } while (!component->can_proceed() && !component->is_failed());
  }

  // Setup is complete. Reconcile STATUS_LED_WARNING: the slow-setup path
  // above may have forced it on, and any status_clear_warning() calls
  // from components during setup were intentional no-ops (gated by
  // APP_STATE_SETUP_COMPLETE). Walk components once here to pick up the
  // real state. STATUS_LED_ERROR is never artificially forced, so its
  // clear path always works and needs no reconciliation. Finally, set
  // APP_STATE_SETUP_COMPLETE so subsequent warning clears go through
  // the normal walk-and-clear path.
  if (!this->any_component_has_status_flag_(STATUS_LED_WARNING))
    this->app_state_ &= ~STATUS_LED_WARNING;
  this->app_state_ |= APP_STATE_SETUP_COMPLETE;

  ESP_LOGI(TAG, "setup() finished successfully!");

  // Trim the scheduler freelist of its post-boot peak once startup churn settles.
  this->scheduler.set_timeout(this, SCHEDULER_FREELIST_TRIM_DELAY_MS, [this]() { this->scheduler.trim_freelist(); });

#ifdef USE_SETUP_PRIORITY_OVERRIDE
  // Clear setup priority overrides to free memory
  clear_setup_priority_overrides();
#endif

#if defined(USE_ESP32) || defined(USE_LIBRETINY)
  // Save main loop task handle for wake_loop_*() / fast select FreeRTOS notifications.
  esphome_main_task_handle = xTaskGetCurrentTaskHandle();
#endif
#ifdef USE_HOST
  // Set up wake socket for waking main loop from tasks (host platform select() loop).
  wake_setup();
#endif

  // Ensure all active looping components are in LOOP state.
  // Components after the last blocking component only got one call() during setup
  // (CONSTRUCTION→SETUP) and never received the second call() (SETUP→LOOP).
  // The main loop calls loop() directly, bypassing call()'s state machine.
  for (uint16_t i = 0; i < this->looping_components_active_end_; i++) {
    this->looping_components_[i]->set_component_state_(COMPONENT_STATE_LOOP);
  }

  this->schedule_dump_config();
}

void Application::process_dump_config_() {
  if (this->dump_config_at_ == 0) {
    char build_time_str[Application::BUILD_TIME_STR_SIZE];
    this->get_build_time_string(build_time_str);
    ESP_LOGI(TAG, "ESPHome version " ESPHOME_VERSION " compiled on %s", build_time_str);
#ifdef ESPHOME_PROJECT_NAME
    ESP_LOGI(TAG, "Project " ESPHOME_PROJECT_NAME " version " ESPHOME_PROJECT_VERSION);
#endif
#ifdef USE_ESP32
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "ESP32 Chip: %s rev%d.%d, %d core(s)", ESPHOME_VARIANT, chip_info.revision / 100,
             chip_info.revision % 100, chip_info.cores);
#if defined(USE_ESP32_VARIANT_ESP32) && (!defined(USE_ESP32_MIN_CHIP_REVISION_SET) || !defined(USE_ESP32_SRAM1_AS_IRAM))
    static const char *const ESP32_ADVANCED_PATH = "under esp32 > framework > advanced";
#endif
#if defined(USE_ESP32_VARIANT_ESP32) && !defined(USE_ESP32_MIN_CHIP_REVISION_SET)
    {
      // Suggest optimization for chips that don't need the PSRAM cache workaround
      if (chip_info.revision >= 300) {
#ifdef USE_PSRAM
        ESP_LOGW(TAG, "Chip rev >= 3.0 detected. Set minimum_chip_revision: \"%d.%d\" %s to save ~10KB IRAM",
                 chip_info.revision / 100, chip_info.revision % 100, ESP32_ADVANCED_PATH);
#else
        ESP_LOGW(TAG, "Chip rev >= 3.0 detected. Set minimum_chip_revision: \"%d.%d\" %s to reduce binary size",
                 chip_info.revision / 100, chip_info.revision % 100, ESP32_ADVANCED_PATH);
#endif
      }
    }
#endif
    {
      // esp_bootloader_desc_t is available in ESP-IDF >= 5.2; if readable the bootloader is modern.
      //
      // Design decision: We intentionally do NOT mention sram1_as_iram when the bootloader is too old.
      // Enabling sram1_as_iram with an old bootloader causes a hard brick (device fails to boot,
      // requires USB reflash to recover). Users don't always read warnings carefully, so we only
      // suggest the option once we've confirmed the bootloader can handle it. In practice this
      // means a user with an old bootloader may need to flash twice: once via USB to update the
      // bootloader (they'll see the suggestion on next boot), then OTA with sram1_as_iram: true.
      // Two flashes is a better outcome than a bricked device.
      esp_bootloader_desc_t boot_desc;
      if (esp_ota_get_bootloader_description(nullptr, &boot_desc) != ESP_OK) {
#ifdef USE_ESP32_VARIANT_ESP32
        ESP_LOGW(TAG, "Bootloader too old for OTA rollback and SRAM1 as IRAM (+40KB). "
                      "Flash via USB once to update the bootloader");
#else
        ESP_LOGW(TAG, "Bootloader too old for OTA rollback. Flash via USB once to update the bootloader");
#endif
      }
#if defined(USE_ESP32_VARIANT_ESP32) && !defined(USE_ESP32_SRAM1_AS_IRAM)
      else {
        ESP_LOGW(TAG, "Bootloader supports SRAM1 as IRAM (+40KB). Set sram1_as_iram: true %s", ESP32_ADVANCED_PATH);
      }
#endif
    }
#endif  // USE_ESP32
  }

  this->components_[this->dump_config_at_]->call_dump_config_();
  this->dump_config_at_++;
}

void Application::feed_wdt() {
  // Cold entry: callers without a millis() timestamp in hand. Fetches the
  // time and defers to the hot path.
  this->feed_wdt_with_time(MillisInternal::get());
}

void HOT Application::feed_wdt_slow_(uint32_t time) {
  // Callers (both feed_wdt() and feed_wdt_with_time()) have already
  // confirmed the WDT_FEED_INTERVAL_MS rate limit was exceeded.
  arch_feed_wdt();
  this->last_wdt_feed_ = time;
}

#ifdef USE_STATUS_LED
void HOT Application::service_status_led_slow_(uint32_t time) {
  // Callers (feed_wdt(), feed_wdt_with_time()) have already confirmed the
  // STATUS_LED_DISPATCH_INTERVAL_MS rate limit was exceeded. Rate-limited
  // separately from arch_feed_wdt() so the LED blink pattern stays readable
  // (status_led error blink period is 250 ms) while HAL watchdog pokes can
  // still run at the much coarser WDT_FEED_INTERVAL_MS cadence.
  this->last_status_led_service_ = time;
  if (status_led::global_status_led == nullptr)
    return;
  auto *sl = status_led::global_status_led;
  uint8_t sl_state = sl->get_component_state() & COMPONENT_STATE_MASK;
  if (sl_state == COMPONENT_STATE_LOOP_DONE) {
    // status_led only transitions to LOOP_DONE from inside its own loop() (after the
    // first idle-path dispatch), so its pin is already initialized by pre_setup() and
    // its setup() has already run. Re-dispatch only if an error or warning bit has been
    // set since; otherwise skip entirely.
    if ((this->app_state_ & STATUS_LED_MASK) == 0)
      return;
    sl->enable_loop();
  } else if (sl_state != COMPONENT_STATE_LOOP) {
    // CONSTRUCTION/SETUP/FAILED: not our job — App::setup() drives the lifecycle.
    return;
  }
  sl->loop();
}
#endif

bool Application::any_component_has_status_flag_(uint8_t flag) const {
  // Walk all components (not just looping ones) so non-looping components'
  // status bits are respected. Only called from the slow-path clear helpers
  // (status_clear_warning_slow_path_ / status_clear_error_slow_path_) on an
  // actual set→clear transition, so walking O(N) here is paid once per
  // transition — not once per loop iteration.
  for (auto *component : this->components_) {
    if ((component->get_component_state() & flag) != 0)
      return true;
  }
  return false;
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
  uint32_t start_time = MillisInternal::get();

  // Use a StaticVector instead of std::vector to avoid heap allocation
  // since we know the actual size at compile time
  StaticVector<Component *, ESPHOME_COMPONENT_COUNT> pending_components;

  // Copy all components in reverse order
  // Reverse order matches the behavior of run_safe_shutdown_hooks() above and ensures
  // components are torn down in the opposite order of their setup_priority (which is
  // used to sort components during Application::setup())
  size_t num_components = this->components_.size();
  for (size_t i = 0; i < num_components; ++i) {
    pending_components[i] = this->components_[num_components - 1 - i];
  }

  uint32_t now = start_time;
  size_t pending_count = num_components;

  // Teardown Algorithm
  // ==================
  // We iterate through pending components, calling teardown() on each.
  // Components that return false (need more time) are copied forward
  // in the array. Components that return true (finished) are skipped.
  //
  // The compaction happens in-place during iteration:
  //   - still_pending tracks the write position (where to put next pending component)
  //   - i tracks the read position (which component we're testing)
  //   - When teardown() returns false, we copy component[i] to component[still_pending]
  //   - When teardown() returns true, we just skip it (don't increment still_pending)
  //
  // Example with 4 components where B can teardown immediately:
  //
  // Start:
  //   pending_components: [A, B, C, D]
  //   pending_count: 4    ^----------^
  //
  // Iteration 1:
  //   i=0: A needs more time → keep at pos 0 (no copy needed)
  //   i=1: B finished → skip
  //   i=2: C needs more time → copy to pos 1
  //   i=3: D needs more time → copy to pos 2
  //
  // After iteration 1:
  //   pending_components: [A, C, D | D]
  //   pending_count: 3    ^--------^
  //
  // Iteration 2:
  //   i=0: A finished → skip
  //   i=1: C needs more time → copy to pos 0
  //   i=2: D finished → skip
  //
  // After iteration 2:
  //   pending_components: [C | C, D, D]  (positions 1-3 have old values)
  //   pending_count: 1    ^--^

  while (pending_count > 0 && (now - start_time) < timeout_ms) {
    // Feed watchdog during teardown to prevent triggering
    this->feed_wdt_with_time(now);

    // Process components and compact the array, keeping only those still pending
    size_t still_pending = 0;
    for (size_t i = 0; i < pending_count; ++i) {
      if (!pending_components[i]->teardown()) {
        // Component still needs time, copy it forward
        if (still_pending != i) {
          pending_components[still_pending] = pending_components[i];
        }
        ++still_pending;
      }
      // Component finished teardown, skip it (don't increment still_pending)
    }
    pending_count = still_pending;

    // Give some time for I/O operations if components are still pending
    if (pending_count > 0) {
      esphome::internal::wakeable_delay(1);
    }

    // Update time for next iteration
    now = MillisInternal::get();
  }

  if (pending_count > 0) {
    // Note: At this point, connections are either disconnected or in a bad state,
    // so this warning will only appear via serial rather than being transmitted to clients
    for (size_t i = 0; i < pending_count; ++i) {
      ESP_LOGW(TAG, "%s did not complete teardown within %" PRIu32 " ms",
               LOG_STR_ARG(pending_components[i]->get_component_log_str()), timeout_ms);
    }
  }
}

void Application::add_looping_components_by_state_(bool match_loop_done) {
  for (auto *obj : this->components_) {
    if (obj->has_overridden_loop() &&
        ((obj->get_component_state() & COMPONENT_STATE_MASK) == COMPONENT_STATE_LOOP_DONE) == match_loop_done) {
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
          this->loop_component_start_time_ = MillisInternal::get();
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
    ESP_LOGVV(TAG, "%s loop enabled from ISR", LOG_STR_ARG(component->get_component_log_str()));
    component->set_component_state_(COMPONENT_STATE_LOOP);

    // Move to active section
    this->activate_looping_component_(i);
  }

  // If we couldn't process some requests, ensure we check again next iteration
  if (has_pending) {
    this->has_pending_enable_loop_requests_ = true;
  }
}

// App storage — asm label shares the linker symbol with "extern Application App".
// char[] is trivially destructible, so no __cxa_atexit or destructor chain is emitted.
// Constructed via placement new in the generated setup().
#ifndef __GXX_ABI_VERSION
#error "Application placement new requires Itanium C++ ABI (GCC/Clang)"
#endif
static_assert(std::is_default_constructible<Application>::value, "Application must be default-constructible");
// __USER_LABEL_PREFIX__ is "_" on Mach-O (macOS) and empty on ELF (embedded targets).
// String literal concatenation produces the correct platform-specific mangled symbol.
// Two-level macro needed: # stringifies before expansion, so the
// indirection forces __USER_LABEL_PREFIX__ to expand first.
#define ESPHOME_STRINGIFY_IMPL_(x) #x
#define ESPHOME_STRINGIFY_(x) ESPHOME_STRINGIFY_IMPL_(x)
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
alignas(Application) char app_storage[sizeof(Application)] asm(
    ESPHOME_STRINGIFY_(__USER_LABEL_PREFIX__) "_ZN7esphome3AppE");
#undef ESPHOME_STRINGIFY_
#undef ESPHOME_STRINGIFY_IMPL_

void Application::get_build_time_string(std::span<char, BUILD_TIME_STR_SIZE> buffer) {
  ESPHOME_strncpy_P(buffer.data(), ESPHOME_BUILD_TIME_STR, buffer.size());
  buffer[buffer.size() - 1] = '\0';
}

void Application::get_comment_string(std::span<char, ESPHOME_COMMENT_SIZE_MAX> buffer) {
  ESPHOME_strncpy_P(buffer.data(), ESPHOME_COMMENT_STR, ESPHOME_COMMENT_SIZE);
  buffer[ESPHOME_COMMENT_SIZE - 1] = '\0';
}

uint32_t Application::get_config_hash() { return ESPHOME_CONFIG_HASH; }

uint32_t Application::get_config_version_hash() { return fnv1a_hash_extend(ESPHOME_CONFIG_HASH, ESPHOME_VERSION); }

time_t Application::get_build_time() { return ESPHOME_BUILD_TIME; }

}  // namespace esphome
