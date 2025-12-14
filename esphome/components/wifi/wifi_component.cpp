#include "wifi_component.h"
#ifdef USE_WIFI
#include <cassert>
#include <cinttypes>

#ifdef USE_ESP32
#if (ESP_IDF_VERSION_MAJOR >= 5 && ESP_IDF_VERSION_MINOR >= 1)
#include <esp_eap_client.h>
#else
#include <esp_wpa2.h>
#endif
#endif

#if defined(USE_ESP32)
#include <esp_wifi.h>
#endif
#ifdef USE_ESP8266
#include <user_interface.h>
#endif

#include <algorithm>
#include <utility>
#include "lwip/dns.h"
#include "lwip/err.h"

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

#ifdef USE_IMPROV
#include "esphome/components/esp32_improv/esp32_improv_component.h"
#endif

namespace esphome::wifi {

static const char *const TAG = "wifi";

/// WiFi Retry Logic - Priority-Based BSSID Selection
///
/// The WiFi component uses a state machine with priority degradation to handle connection failures
/// and automatically cycle through different BSSIDs in mesh networks or multiple configured networks.
///
/// Connection Flow:
/// ┌──────────────────────────────────────────────────────────────────────┐
/// │                      Fast Connect Path (Optional)                    │
/// ├──────────────────────────────────────────────────────────────────────┤
/// │  Entered if: configuration has 'fast_connect: true'                  │
/// │  Optimization to skip scanning when possible:                        │
/// │                                                                      │
/// │  1. INITIAL_CONNECT → Try one of:                                    │
/// │     a) Saved BSSID+channel (from previous boot)                      │
/// │     b) First configured non-hidden network (any BSSID)               │
/// │                          ↓                                           │
/// │     [FAILED] → Check if more configured networks available           │
/// │                          ↓                                           │
/// │  2. FAST_CONNECT_CYCLING_APS → Try remaining configured networks     │
/// │                                (1 attempt each, any BSSID)           │
/// │                          ↓                                           │
/// │     [All Failed] → Fall through to explicit hidden or scanning       │
/// │                                                                      │
/// │  Note: Fast connect data saved from previous successful connection   │
/// └──────────────────────────────────────────────────────────────────────┘
///                          ↓
/// ┌──────────────────────────────────────────────────────────────────────┐
/// │              Explicit Hidden Networks Path (Optional)                │
/// ├──────────────────────────────────────────────────────────────────────┤
/// │  Entered if: first configured network has 'hidden: true'             │
/// │                                                                      │
/// │  1. EXPLICIT_HIDDEN → Try consecutive hidden networks (1 attempt)    │
/// │                       Stop when visible network reached              │
/// │                          ↓                                           │
/// │     Example: Hidden1, Hidden2, Visible1, Hidden3, Visible2           │
/// │              Try: Hidden1, Hidden2 (stop at Visible1)                │
/// │                          ↓                                           │
/// │     [All Failed] → Fall back to scan-based connection                │
/// │                                                                      │
/// │  Note: Fast connect saves BSSID after first successful connection,   │
/// │        so subsequent boots use fast path instead of hidden mode      │
/// └──────────────────────────────────────────────────────────────────────┘
///                          ↓
/// ┌──────────────────────────────────────────────────────────────────────┐
/// │                    Scan-Based Connection Path                        │
/// ├──────────────────────────────────────────────────────────────────────┤
/// │                                                                      │
/// │  1. SCAN → Sort by priority (highest first), then RSSI               │
/// │     ┌─────────────────────────────────────────────────┐              │
/// │     │ scan_result_[0] = Best BSSID (highest priority) │              │
/// │     │ scan_result_[1] = Second best                   │              │
/// │     │ scan_result_[2] = Third best                    │              │
/// │     └─────────────────────────────────────────────────┘              │
/// │                          ↓                                           │
/// │  2. SCAN_CONNECTING → Try scan_result_[0] (2 attempts)               │
/// │                       (Visible1, Visible2 from example above)        │
/// │                          ↓                                           │
/// │  3. FAILED → Decrease priority: 0.0 → -1.0 → -2.0                    │
/// │              (stored in persistent sta_priorities_)                  │
/// │                          ↓                                           │
/// │  4. Check for hidden networks:                                       │
/// │     - If found → RETRY_HIDDEN (try SSIDs not in scan, 1 attempt)     │
/// │       Skip hidden networks before first visible one                  │
/// │       (Skip Hidden1/Hidden2, try Hidden3 from example)               │
/// │     - If none → Skip RETRY_HIDDEN, go to step 5                      │
/// │                          ↓                                           │
/// │  5. FAILED → RESTARTING_ADAPTER (skipped if AP/improv active)        │
/// │                          ↓                                           │
/// │  6. Loop back to start:                                              │
/// │     - If first network is hidden → EXPLICIT_HIDDEN (retry cycle)     │
/// │     - Otherwise → SCAN_CONNECTING (rescan)                           │
/// │                          ↓                                           │
/// │  7. RESCAN → Apply stored priorities, sort again                     │
/// │     ┌─────────────────────────────────────────────────┐              │
/// │     │ scan_result_[0] = BSSID B (priority 0.0)  ← NEW │              │
/// │     │ scan_result_[1] = BSSID C (priority 0.0)        │              │
/// │     │ scan_result_[2] = BSSID A (priority -2.0) ← OLD │              │
/// │     └─────────────────────────────────────────────────┘              │
/// │                          ↓                                           │
/// │  8. SCAN_CONNECTING → Try scan_result_[0] (next best)                │
/// │                                                                      │
/// │  Key: Priority system cycles through BSSIDs ACROSS scan cycles       │
/// │       Full retry cycle: EXPLICIT_HIDDEN → SCAN → RETRY_HIDDEN        │
/// │       Always try best available BSSID (scan_result_[0])              │
/// └──────────────────────────────────────────────────────────────────────┘
///
/// Retry Phases:
/// - INITIAL_CONNECT: Try saved BSSID+channel (fast_connect), or fall back to normal flow
/// - FAST_CONNECT_CYCLING_APS: Cycle through remaining configured networks (1 attempt each, fast_connect only)
/// - EXPLICIT_HIDDEN: Try consecutive networks marked hidden:true before scanning (1 attempt per SSID)
/// - SCAN_CONNECTING: Connect using scan results (2 attempts per BSSID)
/// - RETRY_HIDDEN: Try networks not found in scan (1 attempt per SSID, skipped if none found)
/// - RESTARTING_ADAPTER: Restart WiFi adapter to clear stuck state
///
/// Hidden Network Handling:
/// - Networks marked 'hidden: true' before first non-hidden → Tried in EXPLICIT_HIDDEN phase
/// - Networks marked 'hidden: true' after first non-hidden → Tried in RETRY_HIDDEN phase
/// - After successful connection, fast_connect saves BSSID → subsequent boots use fast path
/// - Networks not in scan results → Tried in RETRY_HIDDEN phase
/// - Networks visible in scan + not marked hidden → Skipped in RETRY_HIDDEN phase
/// - Networks marked 'hidden: true' always use hidden mode, even if broadcasting SSID

static const LogString *retry_phase_to_log_string(WiFiRetryPhase phase) {
  switch (phase) {
    case WiFiRetryPhase::INITIAL_CONNECT:
      return LOG_STR("INITIAL_CONNECT");
#ifdef USE_WIFI_FAST_CONNECT
    case WiFiRetryPhase::FAST_CONNECT_CYCLING_APS:
      return LOG_STR("FAST_CONNECT_CYCLING");
#endif
    case WiFiRetryPhase::EXPLICIT_HIDDEN:
      return LOG_STR("EXPLICIT_HIDDEN");
    case WiFiRetryPhase::SCAN_CONNECTING:
      return LOG_STR("SCAN_CONNECTING");
    case WiFiRetryPhase::RETRY_HIDDEN:
      return LOG_STR("RETRY_HIDDEN");
    case WiFiRetryPhase::RESTARTING_ADAPTER:
      return LOG_STR("RESTARTING");
    default:
      return LOG_STR("UNKNOWN");
  }
}

bool WiFiComponent::went_through_explicit_hidden_phase_() const {
  // If first configured network is marked hidden, we went through EXPLICIT_HIDDEN phase
  // This means those networks were already tried and should be skipped in RETRY_HIDDEN
  return !this->sta_.empty() && this->sta_[0].get_hidden();
}

int8_t WiFiComponent::find_first_non_hidden_index_() const {
  // Find the first network that is NOT marked hidden:true
  // This is where EXPLICIT_HIDDEN phase would have stopped
  for (size_t i = 0; i < this->sta_.size(); i++) {
    if (!this->sta_[i].get_hidden()) {
      return static_cast<int8_t>(i);
    }
  }
  return -1;  // All networks are hidden
}

// 2 attempts per BSSID in SCAN_CONNECTING phase
// Rationale: This is the ONLY phase where we decrease BSSID priority, so we must be very sure.
// Auth failures are common immediately after scan due to WiFi stack state transitions.
// Trying twice filters out false positives and prevents unnecessarily marking a good BSSID as bad.
// After 2 genuine failures, priority degradation ensures we skip this BSSID on subsequent scans.
static constexpr uint8_t WIFI_RETRY_COUNT_PER_BSSID = 2;

// 1 attempt per SSID in RETRY_HIDDEN phase
// Rationale: Try hidden mode once, then rescan to get next best BSSID via priority system
static constexpr uint8_t WIFI_RETRY_COUNT_PER_SSID = 1;

// 1 attempt per AP in fast_connect mode (INITIAL_CONNECT and FAST_CONNECT_CYCLING_APS)
// Rationale: Fast connect prioritizes speed - try each AP once to find a working one quickly
static constexpr uint8_t WIFI_RETRY_COUNT_PER_AP = 1;

/// Cooldown duration in milliseconds after adapter restart or repeated failures
/// Allows WiFi hardware to stabilize before next connection attempt
static constexpr uint32_t WIFI_COOLDOWN_DURATION_MS = 500;

/// Cooldown duration when fallback AP is active and captive portal may be running
/// Longer interval gives users time to configure WiFi without constant connection attempts
/// While connecting, WiFi can't beacon the AP properly, so needs longer cooldown
static constexpr uint32_t WIFI_COOLDOWN_WITH_AP_ACTIVE_MS = 30000;

/// Timeout for WiFi scan operations
/// This is a fallback in case we don't receive a scan done callback from the WiFi driver.
/// Normal scans complete via callback; this only triggers if something goes wrong.
static constexpr uint32_t WIFI_SCAN_TIMEOUT_MS = 31000;

/// Timeout for WiFi connection attempts
/// This is a fallback in case we don't receive connection success/failure callbacks.
/// Some platforms (especially LibreTiny/Beken) can take 30-60 seconds to connect,
/// particularly with fast_connect enabled where no prior scan provides channel info.
/// Do not lower this value - connection failures are detected via callbacks, not timeout.
/// If this timeout fires prematurely while a connection is still in progress, it causes
/// cascading failures: the subsequent scan will also fail because the WiFi driver is
/// still busy with the previous connection attempt.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 46000;

static constexpr uint8_t get_max_retries_for_phase(WiFiRetryPhase phase) {
  switch (phase) {
    case WiFiRetryPhase::INITIAL_CONNECT:
#ifdef USE_WIFI_FAST_CONNECT
    case WiFiRetryPhase::FAST_CONNECT_CYCLING_APS:
#endif
      // INITIAL_CONNECT and FAST_CONNECT_CYCLING_APS both use 1 attempt per AP (fast_connect mode)
      return WIFI_RETRY_COUNT_PER_AP;
    case WiFiRetryPhase::EXPLICIT_HIDDEN:
      // Explicitly hidden network: 1 attempt (user marked as hidden, try once then scan)
      return WIFI_RETRY_COUNT_PER_SSID;
    case WiFiRetryPhase::SCAN_CONNECTING:
      // Scan-based phase: 2 attempts per BSSID (handles transient auth failures after scan)
      return WIFI_RETRY_COUNT_PER_BSSID;
    case WiFiRetryPhase::RETRY_HIDDEN:
      // Hidden network mode: 1 attempt per SSID
      return WIFI_RETRY_COUNT_PER_SSID;
    default:
      return WIFI_RETRY_COUNT_PER_BSSID;
  }
}

static void apply_scan_result_to_params(WiFiAP &params, const WiFiScanResult &scan) {
  params.set_hidden(false);
  params.set_ssid(scan.get_ssid());
  params.set_bssid(scan.get_bssid());
  params.set_channel(scan.get_channel());
}

bool WiFiComponent::needs_scan_results_() const {
  // Only SCAN_CONNECTING phase needs scan results
  if (this->retry_phase_ != WiFiRetryPhase::SCAN_CONNECTING) {
    return false;
  }
  // Need scan if we have no results or no matching networks
  return this->scan_result_.empty() || !this->scan_result_[0].get_matches();
}

bool WiFiComponent::ssid_was_seen_in_scan_(const std::string &ssid) const {
  // Check if this SSID is configured as hidden
  // If explicitly marked hidden, we should always try hidden mode regardless of scan results
  for (const auto &conf : this->sta_) {
    if (conf.get_ssid() == ssid && conf.get_hidden()) {
      return false;  // Treat as not seen - force hidden mode attempt
    }
  }

  // Otherwise, check if we saw it in scan results
  for (const auto &scan : this->scan_result_) {
    if (scan.get_ssid() == ssid) {
      return true;
    }
  }
  return false;
}

int8_t WiFiComponent::find_next_hidden_sta_(int8_t start_index) {
  // Find next SSID that wasn't in scan results (might be hidden)
  bool include_explicit_hidden = !this->went_through_explicit_hidden_phase_();
  // Start searching from start_index + 1
  for (size_t i = start_index + 1; i < this->sta_.size(); i++) {
    const auto &sta = this->sta_[i];

    // Skip networks that were already tried in EXPLICIT_HIDDEN phase
    // Those are: networks marked hidden:true that appear before the first non-hidden network
    // If all networks are hidden (first_non_hidden_idx == -1), skip all of them
    if (!include_explicit_hidden && sta.get_hidden()) {
      int8_t first_non_hidden_idx = this->find_first_non_hidden_index_();
      if (first_non_hidden_idx < 0 || static_cast<int8_t>(i) < first_non_hidden_idx) {
        ESP_LOGD(TAG, "Skipping " LOG_SECRET("'%s'") " (explicit hidden, already tried)", sta.get_ssid().c_str());
        continue;
      }
    }

    // If we didn't scan this cycle, treat all networks as potentially hidden
    // Otherwise, only retry networks that weren't seen in the scan
    if (!this->did_scan_this_cycle_ || !this->ssid_was_seen_in_scan_(sta.get_ssid())) {
      ESP_LOGD(TAG, "Hidden candidate " LOG_SECRET("'%s'") " at index %d", sta.get_ssid().c_str(), static_cast<int>(i));
      return static_cast<int8_t>(i);
    }
    ESP_LOGD(TAG, "Skipping hidden retry for visible network " LOG_SECRET("'%s'"), sta.get_ssid().c_str());
  }
  // No hidden SSIDs found
  return -1;
}

void WiFiComponent::start_initial_connection_() {
  // If first network (highest priority) is explicitly marked hidden, try it first before scanning
  // This respects user's priority order when they explicitly configure hidden networks
  if (!this->sta_.empty() && this->sta_[0].get_hidden()) {
    ESP_LOGI(TAG, "Starting with explicit hidden network (highest priority)");
    this->selected_sta_index_ = 0;
    this->retry_phase_ = WiFiRetryPhase::EXPLICIT_HIDDEN;
    WiFiAP params = this->build_params_for_current_phase_();
    this->start_connecting(params);
  } else {
    ESP_LOGI(TAG, "Starting scan");
    this->start_scanning();
  }
}

#if defined(USE_ESP32) && defined(USE_WIFI_WPA2_EAP) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static const char *eap_phase2_to_str(esp_eap_ttls_phase2_types type) {
  switch (type) {
    case ESP_EAP_TTLS_PHASE2_PAP:
      return "pap";
    case ESP_EAP_TTLS_PHASE2_CHAP:
      return "chap";
    case ESP_EAP_TTLS_PHASE2_MSCHAP:
      return "mschap";
    case ESP_EAP_TTLS_PHASE2_MSCHAPV2:
      return "mschapv2";
    case ESP_EAP_TTLS_PHASE2_EAP:
      return "eap";
    default:
      return "unknown";
  }
}
#endif

float WiFiComponent::get_setup_priority() const { return setup_priority::WIFI; }

void WiFiComponent::setup() {
  this->wifi_pre_setup_();

#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
  // Create semaphore for high-performance mode requests
  // Start at 0, increment on request, decrement on release
  this->high_performance_semaphore_ = xSemaphoreCreateCounting(UINT32_MAX, 0);
  if (this->high_performance_semaphore_ == nullptr) {
    ESP_LOGE(TAG, "Failed semaphore");
  }

  // Store the configured power save mode as baseline
  this->configured_power_save_ = this->power_save_;
#endif

  if (this->enable_on_boot_) {
    this->start();
  } else {
#ifdef USE_ESP32
    esp_netif_init();
#endif
    this->state_ = WIFI_COMPONENT_STATE_DISABLED;
  }
}

void WiFiComponent::start() {
  char mac_s[18];
  ESP_LOGCONFIG(TAG,
                "Starting\n"
                "  Local MAC: %s",
                get_mac_address_pretty_into_buffer(mac_s));
  this->last_connected_ = millis();

  uint32_t hash = this->has_sta() ? fnv1_hash(App.get_compilation_time_ref().c_str()) : 88491487UL;

  this->pref_ = global_preferences->make_preference<wifi::SavedWifiSettings>(hash, true);
#ifdef USE_WIFI_FAST_CONNECT
  this->fast_connect_pref_ = global_preferences->make_preference<wifi::SavedWifiFastConnectSettings>(hash + 1, false);
#endif

  SavedWifiSettings save{};
  if (this->pref_.load(&save)) {
    ESP_LOGD(TAG, "Loaded settings: %s", save.ssid);

    WiFiAP sta{};
    sta.set_ssid(save.ssid);
    sta.set_password(save.password);
    this->set_sta(sta);
  }

  if (this->has_sta()) {
    this->wifi_sta_pre_setup_();
    if (this->output_power_.has_value() && !this->wifi_apply_output_power_(*this->output_power_)) {
      ESP_LOGV(TAG, "Setting Output Power Option failed");
    }

#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
    // Synchronize power_save_ with semaphore state before applying
    if (this->high_performance_semaphore_ != nullptr) {
      UBaseType_t semaphore_count = uxSemaphoreGetCount(this->high_performance_semaphore_);
      if (semaphore_count > 0) {
        this->power_save_ = WIFI_POWER_SAVE_NONE;
        this->is_high_performance_mode_ = true;
      } else {
        this->power_save_ = this->configured_power_save_;
        this->is_high_performance_mode_ = false;
      }
    }
#endif
    if (!this->wifi_apply_power_save_()) {
      ESP_LOGV(TAG, "Setting Power Save Option failed");
    }

    this->transition_to_phase_(WiFiRetryPhase::INITIAL_CONNECT);
#ifdef USE_WIFI_FAST_CONNECT
    WiFiAP params;
    bool loaded_fast_connect = this->load_fast_connect_settings_(params);
    // Fast connect optimization: only use when we have saved BSSID+channel data
    // Without saved data, try first configured network or use normal flow
    if (loaded_fast_connect) {
      ESP_LOGI(TAG, "Starting fast_connect (saved) " LOG_SECRET("'%s'"), params.get_ssid().c_str());
      this->start_connecting(params);
    } else if (!this->sta_.empty() && !this->sta_[0].get_hidden()) {
      // No saved data, but have configured networks - try first non-hidden network
      ESP_LOGI(TAG, "Starting fast_connect (config) " LOG_SECRET("'%s'"), this->sta_[0].get_ssid().c_str());
      this->selected_sta_index_ = 0;
      params = this->build_params_for_current_phase_();
      this->start_connecting(params);
    } else {
      // No saved data and (no networks OR first is hidden) - use normal flow
      this->start_initial_connection_();
    }
#else
    // Without fast_connect: go straight to scanning (or hidden mode if all networks are hidden)
    this->start_initial_connection_();
#endif
#ifdef USE_WIFI_AP
  } else if (this->has_ap()) {
    this->setup_ap_config_();
    if (this->output_power_.has_value() && !this->wifi_apply_output_power_(*this->output_power_)) {
      ESP_LOGV(TAG, "Setting Output Power Option failed");
    }
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr) {
      this->wifi_sta_pre_setup_();
      this->start_scanning();
      captive_portal::global_captive_portal->start();
    }
#endif
#endif  // USE_WIFI_AP
  }
#ifdef USE_IMPROV
  if (!this->has_sta() && esp32_improv::global_improv_component != nullptr) {
    if (this->wifi_mode_(true, {}))
      esp32_improv::global_improv_component->start();
  }
#endif
  this->wifi_apply_hostname_();
}

void WiFiComponent::restart_adapter() {
  ESP_LOGW(TAG, "Restarting adapter");
  this->wifi_mode_(false, {});
  this->error_from_callback_ = false;
}

void WiFiComponent::loop() {
  this->wifi_loop_();
  const uint32_t now = App.get_loop_component_start_time();

  if (this->has_sta()) {
    if (this->is_connected() != this->handled_connected_state_) {
      if (this->handled_connected_state_) {
        this->disconnect_trigger_->trigger();
      } else {
        this->connect_trigger_->trigger();
      }
      this->handled_connected_state_ = this->is_connected();
    }

    switch (this->state_) {
      case WIFI_COMPONENT_STATE_COOLDOWN: {
        this->status_set_warning(LOG_STR("waiting to reconnect"));
        // Skip cooldown if new credentials were provided while connecting
        if (this->skip_cooldown_next_cycle_) {
          this->skip_cooldown_next_cycle_ = false;
          this->check_connecting_finished();
          break;
        }
        // Use longer cooldown when captive portal/improv is active to avoid disrupting user config
        bool portal_active = this->is_captive_portal_active_() || this->is_esp32_improv_active_();
        uint32_t cooldown_duration = portal_active ? WIFI_COOLDOWN_WITH_AP_ACTIVE_MS : WIFI_COOLDOWN_DURATION_MS;
        if (now - this->action_started_ > cooldown_duration) {
          // After cooldown we either restarted the adapter because of
          // a failure, or something tried to connect over and over
          // so we entered cooldown. In both cases we call
          // check_connecting_finished to continue the state machine.
          this->check_connecting_finished();
        }
        break;
      }
      case WIFI_COMPONENT_STATE_STA_SCANNING: {
        this->status_set_warning(LOG_STR("scanning for networks"));
        this->check_scanning_finished();
        break;
      }
      case WIFI_COMPONENT_STATE_STA_CONNECTING: {
        this->status_set_warning(LOG_STR("associating to network"));
        this->check_connecting_finished();
        break;
      }

      case WIFI_COMPONENT_STATE_STA_CONNECTED: {
        if (!this->is_connected()) {
          ESP_LOGW(TAG, "Connection lost; reconnecting");
          this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING;
          // Clear error flag before reconnecting so first attempt is not seen as immediate failure
          this->error_from_callback_ = false;
          this->retry_connect();
        } else {
          this->status_clear_warning();
          this->last_connected_ = now;
        }
        break;
      }
      case WIFI_COMPONENT_STATE_OFF:
      case WIFI_COMPONENT_STATE_AP:
        break;
      case WIFI_COMPONENT_STATE_DISABLED:
        return;
    }

#ifdef USE_WIFI_AP
    if (this->has_ap() && !this->ap_setup_) {
      if (this->ap_timeout_ != 0 && (now - this->last_connected_ > this->ap_timeout_)) {
        ESP_LOGI(TAG, "Starting fallback AP");
        this->setup_ap_config_();
#ifdef USE_CAPTIVE_PORTAL
        if (captive_portal::global_captive_portal != nullptr)
          captive_portal::global_captive_portal->start();
#endif
      }
    }
#endif  // USE_WIFI_AP

#ifdef USE_IMPROV
    if (esp32_improv::global_improv_component != nullptr && !esp32_improv::global_improv_component->is_active() &&
        !esp32_improv::global_improv_component->should_start()) {
      if (now - this->last_connected_ > esp32_improv::global_improv_component->get_wifi_timeout()) {
        if (this->wifi_mode_(true, {}))
          esp32_improv::global_improv_component->start();
      }
    }

#endif

    if (!this->has_ap() && this->reboot_timeout_ != 0) {
      if (now - this->last_connected_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "Can't connect; rebooting");
        App.reboot();
      }
    }
  }

#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
  // Check if power save mode needs to be updated based on high-performance requests
  if (this->high_performance_semaphore_ != nullptr) {
    // Semaphore count directly represents active requests (starts at 0, increments on request)
    UBaseType_t semaphore_count = uxSemaphoreGetCount(this->high_performance_semaphore_);

    if (semaphore_count > 0 && !this->is_high_performance_mode_) {
      // Transition to high-performance mode (no power save)
      ESP_LOGV(TAG, "Switching to high-performance mode (%" PRIu32 " active %s)", (uint32_t) semaphore_count,
               semaphore_count == 1 ? "request" : "requests");
      this->power_save_ = WIFI_POWER_SAVE_NONE;
      if (this->wifi_apply_power_save_()) {
        this->is_high_performance_mode_ = true;
      }
    } else if (semaphore_count == 0 && this->is_high_performance_mode_) {
      // Restore to configured power save mode
      ESP_LOGV(TAG, "Restoring power save mode to configured setting");
      this->power_save_ = this->configured_power_save_;
      if (this->wifi_apply_power_save_()) {
        this->is_high_performance_mode_ = false;
      }
    }
  }
#endif
}

WiFiComponent::WiFiComponent() { global_wifi_component = this; }

bool WiFiComponent::has_ap() const { return this->has_ap_; }
bool WiFiComponent::is_ap_active() const { return this->ap_started_; }
bool WiFiComponent::has_sta() const { return !this->sta_.empty(); }
#ifdef USE_WIFI_11KV_SUPPORT
void WiFiComponent::set_btm(bool btm) { this->btm_ = btm; }
void WiFiComponent::set_rrm(bool rrm) { this->rrm_ = rrm; }
#endif
network::IPAddresses WiFiComponent::get_ip_addresses() {
  if (this->has_sta())
    return this->wifi_sta_ip_addresses();

#ifdef USE_WIFI_AP
  if (this->has_ap())
    return {this->wifi_soft_ap_ip()};
#endif  // USE_WIFI_AP

  return {};
}
network::IPAddress WiFiComponent::get_dns_address(int num) {
  if (this->has_sta())
    return this->wifi_dns_ip_(num);
  return {};
}
// set_use_address() is guaranteed to be called during component setup by Python code generation,
// so use_address_ will always be valid when get_use_address() is called - no fallback needed.
const char *WiFiComponent::get_use_address() const { return this->use_address_; }
void WiFiComponent::set_use_address(const char *use_address) { this->use_address_ = use_address; }

#ifdef USE_WIFI_AP
void WiFiComponent::setup_ap_config_() {
  this->wifi_mode_({}, true);

  if (this->ap_setup_)
    return;

  if (this->ap_.get_ssid().empty()) {
    std::string name = App.get_name();
    if (name.length() > 32) {
      if (App.is_name_add_mac_suffix_enabled()) {
        // Keep first 25 chars and last 7 chars (MAC suffix), remove middle
        name.erase(25, name.length() - 32);
      } else {
        name.resize(32);
      }
    }
    this->ap_.set_ssid(name);
  }
  this->ap_setup_ = this->wifi_start_ap_(this->ap_);

  auto ip_address = this->wifi_soft_ap_ip().str();
  ESP_LOGCONFIG(TAG,
                "Setting up AP:\n"
                "  AP SSID: '%s'\n"
                "  AP Password: '%s'\n"
                "  IP Address: %s",
                this->ap_.get_ssid().c_str(), this->ap_.get_password().c_str(), ip_address.c_str());

#ifdef USE_WIFI_MANUAL_IP
  auto manual_ip = this->ap_.get_manual_ip();
  if (manual_ip.has_value()) {
    ESP_LOGCONFIG(TAG,
                  "  AP Static IP: '%s'\n"
                  "  AP Gateway: '%s'\n"
                  "  AP Subnet: '%s'",
                  manual_ip->static_ip.str().c_str(), manual_ip->gateway.str().c_str(),
                  manual_ip->subnet.str().c_str());
  }
#endif

  if (!this->has_sta()) {
    this->state_ = WIFI_COMPONENT_STATE_AP;
  }
}

void WiFiComponent::set_ap(const WiFiAP &ap) {
  this->ap_ = ap;
  this->has_ap_ = true;
}
#endif  // USE_WIFI_AP

float WiFiComponent::get_loop_priority() const {
  return 10.0f;  // before other loop components
}

void WiFiComponent::init_sta(size_t count) { this->sta_.init(count); }
void WiFiComponent::add_sta(const WiFiAP &ap) { this->sta_.push_back(ap); }
void WiFiComponent::set_sta(const WiFiAP &ap) {
  this->clear_sta();
  this->init_sta(1);
  this->add_sta(ap);
  this->selected_sta_index_ = 0;
  // When new credentials are set (e.g., from improv), skip cooldown to retry immediately
  this->skip_cooldown_next_cycle_ = true;
}

WiFiAP WiFiComponent::build_params_for_current_phase_() {
  const WiFiAP *config = this->get_selected_sta_();
  if (config == nullptr) {
    ESP_LOGE(TAG, "No valid network config (selected_sta_index_=%d, sta_.size()=%zu)",
             static_cast<int>(this->selected_sta_index_), this->sta_.size());
    // Return empty params - caller should handle this gracefully
    return WiFiAP();
  }

  WiFiAP params = *config;

  switch (this->retry_phase_) {
    case WiFiRetryPhase::INITIAL_CONNECT:
#ifdef USE_WIFI_FAST_CONNECT
    case WiFiRetryPhase::FAST_CONNECT_CYCLING_APS:
#endif
      // Fast connect phases: use config-only (no scan results)
      // BSSID/channel from config if user specified them, otherwise empty
      break;

    case WiFiRetryPhase::EXPLICIT_HIDDEN:
    case WiFiRetryPhase::RETRY_HIDDEN:
      // Hidden network mode: clear BSSID/channel to trigger probe request
      // (both explicit hidden and retry hidden use same behavior)
      params.set_bssid(optional<bssid_t>{});
      params.set_channel(optional<uint8_t>{});
      break;

    case WiFiRetryPhase::SCAN_CONNECTING:
      // Scan-based phase: always use best scan result (index 0 - highest priority after sorting)
      if (!this->scan_result_.empty()) {
        apply_scan_result_to_params(params, this->scan_result_[0]);
      }
      break;

    case WiFiRetryPhase::RESTARTING_ADAPTER:
      // Should not be building params during restart
      break;
  }

  return params;
}

WiFiAP WiFiComponent::get_sta() const {
  const WiFiAP *config = this->get_selected_sta_();
  return config ? *config : WiFiAP{};
}
void WiFiComponent::save_wifi_sta(const std::string &ssid, const std::string &password) {
  SavedWifiSettings save{};  // zero-initialized - all bytes set to \0, guaranteeing null termination
  strncpy(save.ssid, ssid.c_str(), sizeof(save.ssid) - 1);              // max 32 chars, byte 32 remains \0
  strncpy(save.password, password.c_str(), sizeof(save.password) - 1);  // max 64 chars, byte 64 remains \0
  this->pref_.save(&save);
  // ensure it's written immediately
  global_preferences->sync();

  WiFiAP sta{};
  sta.set_ssid(ssid);
  sta.set_password(password);
  this->set_sta(sta);

  // Trigger connection attempt (exits cooldown if needed, no-op if already connecting/connected)
  this->connect_soon_();
}

void WiFiComponent::connect_soon_() {
  // Only trigger retry if we're in cooldown - if already connecting/connected, do nothing
  if (this->state_ == WIFI_COMPONENT_STATE_COOLDOWN) {
    ESP_LOGD(TAG, "Exiting cooldown early due to new WiFi credentials");
    this->retry_connect();
  }
}

void WiFiComponent::start_connecting(const WiFiAP &ap) {
  // Log connection attempt at INFO level with priority
  char bssid_s[18];
  int8_t priority = 0;

  if (ap.get_bssid().has_value()) {
    format_mac_addr_upper(ap.get_bssid().value().data(), bssid_s);
    priority = this->get_sta_priority(ap.get_bssid().value());
  }

  ESP_LOGI(TAG,
           "Connecting to " LOG_SECRET("'%s'") " " LOG_SECRET("(%s)") " (priority %d, attempt %u/%u in phase %s)...",
           ap.get_ssid().c_str(), ap.get_bssid().has_value() ? bssid_s : LOG_STR_LITERAL("any"), priority,
           this->num_retried_ + 1, get_max_retries_for_phase(this->retry_phase_),
           LOG_STR_ARG(retry_phase_to_log_string(this->retry_phase_)));

#ifdef ESPHOME_LOG_HAS_VERBOSE
  ESP_LOGV(TAG, "Connection Params:");
  ESP_LOGV(TAG, "  SSID: '%s'", ap.get_ssid().c_str());
  if (ap.get_bssid().has_value()) {
    ESP_LOGV(TAG, "  BSSID: %s", bssid_s);
  } else {
    ESP_LOGV(TAG, "  BSSID: Not Set");
  }

#ifdef USE_WIFI_WPA2_EAP
  if (ap.get_eap().has_value()) {
    ESP_LOGV(TAG, "  WPA2 Enterprise authentication configured:");
    EAPAuth eap_config = ap.get_eap().value();
    ESP_LOGV(TAG, "    Identity: " LOG_SECRET("'%s'"), eap_config.identity.c_str());
    ESP_LOGV(TAG, "    Username: " LOG_SECRET("'%s'"), eap_config.username.c_str());
    ESP_LOGV(TAG, "    Password: " LOG_SECRET("'%s'"), eap_config.password.c_str());
#if defined(USE_ESP32) && defined(USE_WIFI_WPA2_EAP) && ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    ESP_LOGV(TAG, "    TTLS Phase 2: " LOG_SECRET("'%s'"), eap_phase2_to_str(eap_config.ttls_phase_2));
#endif
    bool ca_cert_present = eap_config.ca_cert != nullptr && strlen(eap_config.ca_cert);
    bool client_cert_present = eap_config.client_cert != nullptr && strlen(eap_config.client_cert);
    bool client_key_present = eap_config.client_key != nullptr && strlen(eap_config.client_key);
    ESP_LOGV(TAG, "    CA Cert:     %s", ca_cert_present ? "present" : "not present");
    ESP_LOGV(TAG, "    Client Cert: %s", client_cert_present ? "present" : "not present");
    ESP_LOGV(TAG, "    Client Key:  %s", client_key_present ? "present" : "not present");
  } else {
#endif
    ESP_LOGV(TAG, "  Password: " LOG_SECRET("'%s'"), ap.get_password().c_str());
#ifdef USE_WIFI_WPA2_EAP
  }
#endif
  if (ap.get_channel().has_value()) {
    ESP_LOGV(TAG, "  Channel: %u", *ap.get_channel());
  } else {
    ESP_LOGV(TAG, "  Channel not set");
  }
#ifdef USE_WIFI_MANUAL_IP
  if (ap.get_manual_ip().has_value()) {
    ManualIP m = *ap.get_manual_ip();
    ESP_LOGV(TAG, "  Manual IP: Static IP=%s Gateway=%s Subnet=%s DNS1=%s DNS2=%s", m.static_ip.str().c_str(),
             m.gateway.str().c_str(), m.subnet.str().c_str(), m.dns1.str().c_str(), m.dns2.str().c_str());
  } else
#endif
  {
    ESP_LOGV(TAG, "  Using DHCP IP");
  }
  ESP_LOGV(TAG, "  Hidden: %s", YESNO(ap.get_hidden()));
#endif

  if (!this->wifi_sta_connect_(ap)) {
    ESP_LOGE(TAG, "wifi_sta_connect_ failed");
    // Enter cooldown to allow WiFi hardware to stabilize
    // (immediate failure suggests hardware not ready, different from connection timeout)
    this->state_ = WIFI_COMPONENT_STATE_COOLDOWN;
  } else {
    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING;
  }
  this->action_started_ = millis();
}

const LogString *get_signal_bars(int8_t rssi) {
  // Check for disconnected sentinel value first
  if (rssi == WIFI_RSSI_DISCONNECTED) {
    // MULTIPLICATION SIGN
    // Unicode: U+00D7, UTF-8: C3 97
    return LOG_STR("\033[0;31m"  // red
                   "\xc3\x97\xc3\x97\xc3\x97\xc3\x97"
                   "\033[0m");
  }
  // LOWER ONE QUARTER BLOCK
  // Unicode: U+2582, UTF-8: E2 96 82
  // LOWER HALF BLOCK
  // Unicode: U+2584, UTF-8: E2 96 84
  // LOWER THREE QUARTERS BLOCK
  // Unicode: U+2586, UTF-8: E2 96 86
  // FULL BLOCK
  // Unicode: U+2588, UTF-8: E2 96 88
  if (rssi >= -50) {
    return LOG_STR("\033[0;32m"  // green
                   "\xe2\x96\x82"
                   "\xe2\x96\x84"
                   "\xe2\x96\x86"
                   "\xe2\x96\x88"
                   "\033[0m");
  } else if (rssi >= -65) {
    return LOG_STR("\033[0;33m"  // yellow
                   "\xe2\x96\x82"
                   "\xe2\x96\x84"
                   "\xe2\x96\x86"
                   "\033[0;37m"
                   "\xe2\x96\x88"
                   "\033[0m");
  } else if (rssi >= -85) {
    return LOG_STR("\033[0;33m"  // yellow
                   "\xe2\x96\x82"
                   "\xe2\x96\x84"
                   "\033[0;37m"
                   "\xe2\x96\x86"
                   "\xe2\x96\x88"
                   "\033[0m");
  } else {
    return LOG_STR("\033[0;31m"  // red
                   "\xe2\x96\x82"
                   "\033[0;37m"
                   "\xe2\x96\x84"
                   "\xe2\x96\x86"
                   "\xe2\x96\x88"
                   "\033[0m");
  }
}

void WiFiComponent::print_connect_params_() {
  bssid_t bssid = wifi_bssid();
  char bssid_s[18];
  format_mac_addr_upper(bssid.data(), bssid_s);

  char mac_s[18];
  ESP_LOGCONFIG(TAG, "  Local MAC: %s", get_mac_address_pretty_into_buffer(mac_s));
  if (this->is_disabled()) {
    ESP_LOGCONFIG(TAG, "  Disabled");
    return;
  }
  for (auto &ip : wifi_sta_ip_addresses()) {
    if (ip.is_set()) {
      ESP_LOGCONFIG(TAG, "  IP Address: %s", ip.str().c_str());
    }
  }
  int8_t rssi = wifi_rssi();
  ESP_LOGCONFIG(TAG,
                "  SSID: " LOG_SECRET("'%s'") "\n"
                                              "  BSSID: " LOG_SECRET("%s") "\n"
                                                                           "  Hostname: '%s'\n"
                                                                           "  Signal strength: %d dB %s\n"
                                                                           "  Channel: %" PRId32 "\n"
                                                                           "  Subnet: %s\n"
                                                                           "  Gateway: %s\n"
                                                                           "  DNS1: %s\n"
                                                                           "  DNS2: %s",
                wifi_ssid().c_str(), bssid_s, App.get_name().c_str(), rssi, LOG_STR_ARG(get_signal_bars(rssi)),
                get_wifi_channel(), wifi_subnet_mask_().str().c_str(), wifi_gateway_ip_().str().c_str(),
                wifi_dns_ip_(0).str().c_str(), wifi_dns_ip_(1).str().c_str());
#ifdef ESPHOME_LOG_HAS_VERBOSE
  if (const WiFiAP *config = this->get_selected_sta_(); config && config->get_bssid().has_value()) {
    ESP_LOGV(TAG, "  Priority: %d", this->get_sta_priority(*config->get_bssid()));
  }
#endif
#ifdef USE_WIFI_11KV_SUPPORT
  ESP_LOGCONFIG(TAG,
                "  BTM: %s\n"
                "  RRM: %s",
                this->btm_ ? "enabled" : "disabled", this->rrm_ ? "enabled" : "disabled");
#endif
}

void WiFiComponent::enable() {
  if (this->state_ != WIFI_COMPONENT_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Enabling");
  this->error_from_callback_ = false;
  this->state_ = WIFI_COMPONENT_STATE_OFF;
  this->start();
}

void WiFiComponent::disable() {
  if (this->state_ == WIFI_COMPONENT_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Disabling");
  this->state_ = WIFI_COMPONENT_STATE_DISABLED;
  this->wifi_disconnect_();
  this->wifi_mode_(false, false);
}

bool WiFiComponent::is_disabled() { return this->state_ == WIFI_COMPONENT_STATE_DISABLED; }

void WiFiComponent::start_scanning() {
  this->action_started_ = millis();
  ESP_LOGD(TAG, "Starting scan");
  this->wifi_scan_start_(this->passive_scan_);
  this->state_ = WIFI_COMPONENT_STATE_STA_SCANNING;
}

/// Comparator for WiFi scan result sorting - determines which network should be tried first
/// Returns true if 'a' should be placed before 'b' in the sorted order (a is "better" than b)
///
/// Sorting logic (in priority order):
/// 1. Matching networks always ranked before non-matching networks
/// 2. For matching networks: Priority first (CRITICAL - tracks failure history)
/// 3. RSSI as tiebreaker for equal priority or non-matching networks
///
/// WHY PRIORITY MUST BE CHECKED FIRST:
/// The priority field tracks connection failure history via priority degradation:
/// - Initial priority: 0.0 (from config or default)
/// - Each connection failure: priority -= 1.0 (becomes -1.0, -2.0, -3.0, etc.)
/// - Failed BSSIDs sorted lower → naturally try different BSSID on next scan
///
/// This enables automatic BSSID cycling for various real-world failure scenarios:
/// - Crashed/hung AP (visible but not responding)
/// - Misconfigured mesh node (accepts auth but no DHCP/routing)
/// - Capacity limits (AP refuses new clients)
/// - Rogue AP (same SSID, wrong password or malicious)
/// - Intermittent hardware issues (flaky radio, overheating)
///
/// Example mesh network: 3 APs with same SSID "home", all at priority 0.0 initially
/// - Try strongest BSSID A (sorted by RSSI) → fails → priority A becomes -1.0
/// - Next scan: BSSID B and C (priority 0.0) sorted BEFORE A (priority -1.0)
/// - Try next strongest BSSID B → succeeds or fails and gets deprioritized
/// - System naturally cycles through all BSSIDs via priority degradation
/// - Eventually finds working AP or tries all options before restarting adapter
///
/// If we checked RSSI first (Bug in PR #9963):
/// - Same failed BSSID would keep being selected if it has strongest signal
/// - Device stuck connecting to crashed AP with -30dBm while working AP at -50dBm ignored
/// - Priority degradation would be useless
/// - Mesh networks would never recover from single AP failure
[[nodiscard]] inline static bool wifi_scan_result_is_better(const WiFiScanResult &a, const WiFiScanResult &b) {
  // Matching networks always come before non-matching
  if (a.get_matches() && !b.get_matches())
    return true;
  if (!a.get_matches() && b.get_matches())
    return false;

  // Both matching: check priority first (tracks connection failures via priority degradation)
  // Priority is decreased when a BSSID fails to connect, so lower priority = previously failed
  if (a.get_matches() && b.get_matches() && a.get_priority() != b.get_priority()) {
    return a.get_priority() > b.get_priority();
  }

  // Use RSSI as tiebreaker (for equal-priority matching networks or all non-matching networks)
  return a.get_rssi() > b.get_rssi();
}

// Helper function for insertion sort of WiFi scan results
// Using insertion sort instead of std::stable_sort saves flash memory
// by avoiding template instantiations (std::rotate, std::stable_sort, lambdas)
// IMPORTANT: This sort is stable (preserves relative order of equal elements)
template<typename VectorType> static void insertion_sort_scan_results(VectorType &results) {
  const size_t size = results.size();
  for (size_t i = 1; i < size; i++) {
    // Make a copy to avoid issues with move semantics during comparison
    WiFiScanResult key = results[i];
    int32_t j = i - 1;

    // Move elements that are worse than key to the right
    // For stability, we only move if key is strictly better than results[j]
    while (j >= 0 && wifi_scan_result_is_better(key, results[j])) {
      results[j + 1] = results[j];
      j--;
    }
    results[j + 1] = key;
  }
}

// Helper function to log scan results - marked noinline to prevent re-inlining into loop
__attribute__((noinline)) static void log_scan_result(const WiFiScanResult &res) {
  char bssid_s[18];
  auto bssid = res.get_bssid();
  format_mac_addr_upper(bssid.data(), bssid_s);

  if (res.get_matches()) {
    ESP_LOGI(TAG, "- '%s' %s" LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(),
             res.get_is_hidden() ? LOG_STR_LITERAL("(HIDDEN) ") : LOG_STR_LITERAL(""), bssid_s,
             LOG_STR_ARG(get_signal_bars(res.get_rssi())));
    ESP_LOGD(TAG, "  Channel: %2u, RSSI: %3d dB, Priority: %4d", res.get_channel(), res.get_rssi(), res.get_priority());
  } else {
    ESP_LOGD(TAG, "- " LOG_SECRET("'%s'") " " LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(), bssid_s,
             LOG_STR_ARG(get_signal_bars(res.get_rssi())));
  }
}

void WiFiComponent::check_scanning_finished() {
  if (!this->scan_done_) {
    if (millis() - this->action_started_ > WIFI_SCAN_TIMEOUT_MS) {
      ESP_LOGE(TAG, "Scan timeout");
      this->retry_connect();
    }
    return;
  }
  this->scan_done_ = false;
  this->did_scan_this_cycle_ = true;

  if (this->scan_result_.empty()) {
    ESP_LOGW(TAG, "No networks found");
    this->retry_connect();
    return;
  }

  ESP_LOGD(TAG, "Found networks:");
  for (auto &res : this->scan_result_) {
    for (auto &ap : this->sta_) {
      if (res.matches(ap)) {
        res.set_matches(true);
        // Cache priority lookup - do single search instead of 2 separate searches
        const bssid_t &bssid = res.get_bssid();
        if (!this->has_sta_priority(bssid)) {
          this->set_sta_priority(bssid, ap.get_priority());
        }
        res.set_priority(this->get_sta_priority(bssid));
        break;
      }
    }
  }

  // Sort scan results using insertion sort for better memory efficiency
  insertion_sort_scan_results(this->scan_result_);

  for (auto &res : this->scan_result_) {
    log_scan_result(res);
  }

  // SYNCHRONIZATION POINT: Establish link between scan_result_[0] and selected_sta_index_
  // After sorting, scan_result_[0] contains the best network. Now find which sta_[i] config
  // matches that network and record it in selected_sta_index_. This keeps the two indices
  // synchronized so build_params_for_current_phase_() can safely use both to build connection parameters.
  const WiFiScanResult &scan_res = this->scan_result_[0];
  bool found_match = false;
  if (scan_res.get_matches()) {
    for (size_t i = 0; i < this->sta_.size(); i++) {
      if (scan_res.matches(this->sta_[i])) {
        // Safe cast: sta_.size() limited to MAX_WIFI_NETWORKS (127) in __init__.py validation
        // No overflow check needed - YAML validation prevents >127 networks
        this->selected_sta_index_ = static_cast<int8_t>(i);  // Links scan_result_[0] with sta_[i]
        found_match = true;
        break;
      }
    }
  }

  if (!found_match) {
    ESP_LOGW(TAG, "No matching network found");
    // No scan results matched our configured networks - transition directly to hidden mode
    // Don't call retry_connect() since we never attempted a connection (no BSSID to penalize)
    this->transition_to_phase_(WiFiRetryPhase::RETRY_HIDDEN);
    // If no hidden networks to try, skip connection attempt (will be handled on next loop)
    if (this->selected_sta_index_ == -1) {
      return;
    }
    // Now start connection attempt in hidden mode
  } else if (this->transition_to_phase_(WiFiRetryPhase::SCAN_CONNECTING)) {
    return;  // scan started, wait for next loop iteration
  }

  yield();

  WiFiAP params = this->build_params_for_current_phase_();
  // Ensure we're in SCAN_CONNECTING phase when connecting with scan results
  // (needed when scan was started directly without transition_to_phase_, e.g., initial scan)
  this->start_connecting(params);
}

void WiFiComponent::dump_config() {
  ESP_LOGCONFIG(TAG,
                "WiFi:\n"
                "  Connected: %s",
                YESNO(this->is_connected()));
  this->print_connect_params_();
}

void WiFiComponent::check_connecting_finished() {
  auto status = this->wifi_sta_connect_status_();

  if (status == WiFiSTAConnectStatus::CONNECTED) {
    if (wifi_ssid().empty()) {
      ESP_LOGW(TAG, "Connection incomplete");
      this->retry_connect();
      return;
    }

    ESP_LOGI(TAG, "Connected");
    // Warn if we had to retry with hidden network mode for a network that's not marked hidden
    // Only warn if we actually connected without scan data (SSID only), not if scan succeeded on retry
    if (const WiFiAP *config = this->get_selected_sta_(); this->retry_phase_ == WiFiRetryPhase::RETRY_HIDDEN &&
                                                          config && !config->get_hidden() &&
                                                          this->scan_result_.empty()) {
      ESP_LOGW(TAG, LOG_SECRET("'%s'") " should be marked hidden", config->get_ssid().c_str());
    }
    // Reset to initial phase on successful connection (don't log transition, just reset state)
    this->retry_phase_ = WiFiRetryPhase::INITIAL_CONNECT;
    this->num_retried_ = 0;
    // Ensure next connection attempt does not inherit error state
    // so when WiFi disconnects later we start fresh and don't see
    // the first connection as a failure.
    this->error_from_callback_ = false;

    this->print_connect_params_();

    if (this->has_ap()) {
#ifdef USE_CAPTIVE_PORTAL
      if (this->is_captive_portal_active_()) {
        captive_portal::global_captive_portal->end();
      }
#endif
      ESP_LOGD(TAG, "Disabling AP");
      this->wifi_mode_({}, false);
    }
#ifdef USE_IMPROV
    if (this->is_esp32_improv_active_()) {
      esp32_improv::global_improv_component->stop();
    }
#endif

    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTED;
    this->num_retried_ = 0;

    // Clear priority tracking if all priorities are at minimum
    this->clear_priorities_if_all_min_();

#ifdef USE_WIFI_FAST_CONNECT
    this->save_fast_connect_settings_();
#endif

    // Free scan results memory unless a component needs them
    if (!this->keep_scan_results_) {
      this->scan_result_.clear();
      this->scan_result_.shrink_to_fit();
    }

    return;
  }

  uint32_t now = millis();
  if (now - this->action_started_ > WIFI_CONNECT_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Connection timeout, aborting connection attempt");
    this->wifi_disconnect_();
    this->retry_connect();
    return;
  }

  if (this->error_from_callback_) {
    ESP_LOGW(TAG, "Connecting to network failed (callback)");
    this->retry_connect();
    return;
  }

  if (status == WiFiSTAConnectStatus::CONNECTING) {
    return;
  }

  if (status == WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND) {
    ESP_LOGW(TAG, "Network no longer found");
    this->retry_connect();
    return;
  }

  if (status == WiFiSTAConnectStatus::ERROR_CONNECT_FAILED) {
    ESP_LOGW(TAG, "Connecting to network failed");
    this->retry_connect();
    return;
  }

  ESP_LOGW(TAG, "Unknown connection status %d", (int) status);
  this->retry_connect();
}

/// Determine the next retry phase based on current state and failure conditions
/// This function examines the current retry phase, number of retries, and failure reasons
/// to decide what phase to move to next. It does not modify any state - it only returns
/// the recommended next phase.
///
/// @return The next WiFiRetryPhase to transition to (may be same as current phase if should retry)
WiFiRetryPhase WiFiComponent::determine_next_phase_() {
  switch (this->retry_phase_) {
    case WiFiRetryPhase::INITIAL_CONNECT:
#ifdef USE_WIFI_FAST_CONNECT
    case WiFiRetryPhase::FAST_CONNECT_CYCLING_APS:
      // INITIAL_CONNECT and FAST_CONNECT_CYCLING_APS: no retries, try next AP or fall back to scan
      if (this->selected_sta_index_ < static_cast<int8_t>(this->sta_.size()) - 1) {
        return WiFiRetryPhase::FAST_CONNECT_CYCLING_APS;  // Move to next AP
      }
#endif
      // Check if we should try explicit hidden networks before scanning
      // This handles reconnection after connection loss where first network is hidden
      if (!this->sta_.empty() && this->sta_[0].get_hidden()) {
        return WiFiRetryPhase::EXPLICIT_HIDDEN;
      }
      // No more APs to try, fall back to scan
      return WiFiRetryPhase::SCAN_CONNECTING;

    case WiFiRetryPhase::EXPLICIT_HIDDEN: {
      // Try all explicitly hidden networks before scanning
      if (this->num_retried_ + 1 < WIFI_RETRY_COUNT_PER_SSID) {
        return WiFiRetryPhase::EXPLICIT_HIDDEN;  // Keep retrying same SSID
      }

      // Exhausted retries on current SSID - check for more explicitly hidden networks
      // Stop when we reach a visible network (proceed to scanning)
      size_t next_index = this->selected_sta_index_ + 1;
      if (next_index < this->sta_.size() && this->sta_[next_index].get_hidden()) {
        // Found another explicitly hidden network
        return WiFiRetryPhase::EXPLICIT_HIDDEN;
      }

      // No more consecutive explicitly hidden networks
      // If ALL networks are hidden, skip scanning and go directly to restart
      if (this->find_first_non_hidden_index_() < 0) {
        return WiFiRetryPhase::RESTARTING_ADAPTER;
      }
      // Otherwise proceed to scanning for non-hidden networks
      return WiFiRetryPhase::SCAN_CONNECTING;
    }

    case WiFiRetryPhase::SCAN_CONNECTING:
      // If scan found no networks or no matching networks, skip to hidden network mode
      if (this->scan_result_.empty() || !this->scan_result_[0].get_matches()) {
        return WiFiRetryPhase::RETRY_HIDDEN;
      }

      if (this->num_retried_ + 1 < WIFI_RETRY_COUNT_PER_BSSID) {
        return WiFiRetryPhase::SCAN_CONNECTING;  // Keep retrying same BSSID
      }

      // Exhausted retries on current BSSID (scan_result_[0])
      // Its priority has been decreased, so on next scan it will be sorted lower
      // and we'll try the next best BSSID.
      // Check if there are any potentially hidden networks to try
      if (this->find_next_hidden_sta_(-1) >= 0) {
        return WiFiRetryPhase::RETRY_HIDDEN;  // Found hidden networks to try
      }
      // No hidden networks - always go through RESTARTING_ADAPTER phase
      // This ensures num_retried_ gets reset and a fresh scan is triggered
      // The actual adapter restart will be skipped if captive portal/improv is active
      return WiFiRetryPhase::RESTARTING_ADAPTER;

    case WiFiRetryPhase::RETRY_HIDDEN:
      // If no hidden SSIDs to try (selected_sta_index_ == -1), skip directly to rescan
      if (this->selected_sta_index_ >= 0) {
        if (this->num_retried_ + 1 < WIFI_RETRY_COUNT_PER_SSID) {
          return WiFiRetryPhase::RETRY_HIDDEN;  // Keep retrying same SSID
        }

        // Exhausted retries on current SSID - check if there are more potentially hidden SSIDs to try
        if (this->selected_sta_index_ < static_cast<int8_t>(this->sta_.size()) - 1) {
          // Check if find_next_hidden_sta_() would actually find another hidden SSID
          // as it might have been seen in the scan results and we want to skip those
          // otherwise we will get stuck in RETRY_HIDDEN phase
          if (this->find_next_hidden_sta_(this->selected_sta_index_) != -1) {
            // More hidden SSIDs available - stay in RETRY_HIDDEN, advance will happen in retry_connect()
            return WiFiRetryPhase::RETRY_HIDDEN;
          }
        }
      }
      // Exhausted all potentially hidden SSIDs - always go through RESTARTING_ADAPTER
      // This ensures num_retried_ gets reset and a fresh scan is triggered
      // The actual adapter restart will be skipped if captive portal/improv is active
      return WiFiRetryPhase::RESTARTING_ADAPTER;

    case WiFiRetryPhase::RESTARTING_ADAPTER:
      // After restart, go back to explicit hidden if we went through it initially
      if (this->went_through_explicit_hidden_phase_()) {
        return WiFiRetryPhase::EXPLICIT_HIDDEN;
      }
      // Skip scanning when captive portal/improv is active to avoid disrupting AP
      // Even passive scans can cause brief AP disconnections on ESP32
      if (this->is_captive_portal_active_() || this->is_esp32_improv_active_()) {
        return WiFiRetryPhase::RETRY_HIDDEN;
      }
      return WiFiRetryPhase::SCAN_CONNECTING;
  }

  // Should never reach here
  return WiFiRetryPhase::SCAN_CONNECTING;
}

/// Transition from current retry phase to a new phase with logging and phase-specific setup
/// This function handles the actual state change, including:
/// - Logging the phase transition
/// - Resetting the retry counter
/// - Performing phase-specific initialization (e.g., advancing AP index, starting scans)
///
/// @param new_phase The phase we're transitioning TO
/// @return true if connection attempt should be skipped (scan started or no networks to try)
///         false if caller can proceed with connection attempt
bool WiFiComponent::transition_to_phase_(WiFiRetryPhase new_phase) {
  WiFiRetryPhase old_phase = this->retry_phase_;

  // No-op if staying in same phase
  if (old_phase == new_phase) {
    return false;
  }

  ESP_LOGD(TAG, "Retry phase: %s → %s", LOG_STR_ARG(retry_phase_to_log_string(old_phase)),
           LOG_STR_ARG(retry_phase_to_log_string(new_phase)));

  this->retry_phase_ = new_phase;
  this->num_retried_ = 0;  // Reset retry counter on phase change

  // Phase-specific setup
  switch (new_phase) {
#ifdef USE_WIFI_FAST_CONNECT
    case WiFiRetryPhase::FAST_CONNECT_CYCLING_APS:
      // Move to next configured AP - clear old scan data so new AP is tried with config only
      this->selected_sta_index_++;
      this->scan_result_.clear();
      break;
#endif

    case WiFiRetryPhase::EXPLICIT_HIDDEN:
      // Starting explicit hidden phase - reset to first network
      this->selected_sta_index_ = 0;
      break;

    case WiFiRetryPhase::SCAN_CONNECTING:
      // Transitioning to scan-based connection
#ifdef USE_WIFI_FAST_CONNECT
      if (old_phase == WiFiRetryPhase::FAST_CONNECT_CYCLING_APS) {
        ESP_LOGI(TAG, "Fast connect exhausted, falling back to scan");
      }
#endif
      // Trigger scan if we don't have scan results OR if transitioning from phases that need fresh scan
      if (this->scan_result_.empty() || old_phase == WiFiRetryPhase::EXPLICIT_HIDDEN ||
          old_phase == WiFiRetryPhase::RETRY_HIDDEN || old_phase == WiFiRetryPhase::RESTARTING_ADAPTER) {
        this->selected_sta_index_ = -1;  // Will be set after scan completes
        this->start_scanning();
        return true;  // Started scan, wait for completion
      }
      // Already have scan results - selected_sta_index_ should already be synchronized
      // (set in check_scanning_finished() when scan completed)
      // No need to reset it here
      break;

    case WiFiRetryPhase::RETRY_HIDDEN:
      // Starting hidden mode - find first SSID that wasn't in scan results
      if (old_phase == WiFiRetryPhase::SCAN_CONNECTING) {
        // Keep scan results so we can skip SSIDs that were visible in the scan
        // Don't clear scan_result_ - we need it to know which SSIDs are NOT hidden

        // If first network is marked hidden, we went through EXPLICIT_HIDDEN phase
        // In that case, skip networks marked hidden:true (already tried)
        // Otherwise, include them (they haven't been tried yet)
        this->selected_sta_index_ = this->find_next_hidden_sta_(-1);

        if (this->selected_sta_index_ == -1) {
          ESP_LOGD(TAG, "All SSIDs visible or already tried, skipping hidden mode");
        }
      }
      break;

    case WiFiRetryPhase::RESTARTING_ADAPTER:
      // Skip actual adapter restart if captive portal/improv is active
      // This allows state machine to reset num_retried_ and trigger fresh scan
      // without disrupting the captive portal/improv connection
      if (!this->is_captive_portal_active_() && !this->is_esp32_improv_active_()) {
        this->restart_adapter();
      } else {
        // Even when skipping full restart, disconnect to clear driver state
        // Without this, platforms like LibreTiny may think we're still connecting
        this->wifi_disconnect_();
      }
      // Clear scan flag - we're starting a new retry cycle
      this->did_scan_this_cycle_ = false;
      // Always enter cooldown after restart (or skip-restart) to allow stabilization
      // Use extended cooldown when AP is active to avoid constant scanning that blocks DNS
      this->state_ = WIFI_COMPONENT_STATE_COOLDOWN;
      this->action_started_ = millis();
      // Return true to indicate we should wait (go to COOLDOWN) instead of immediately connecting
      return true;

    default:
      break;
  }

  return false;  // Did not start scan, can proceed with connection
}

/// Clear BSSID priority tracking if all priorities are at minimum (saves memory)
/// At minimum priority, all BSSIDs are equally bad, so priority tracking is useless
/// Called after successful connection or after failed connection attempts
void WiFiComponent::clear_priorities_if_all_min_() {
  if (this->sta_priorities_.empty()) {
    return;
  }

  int8_t first_priority = this->sta_priorities_[0].priority;

  // Only clear if all priorities have been decremented to the minimum value
  // At this point, all BSSIDs have been equally penalized and priority info is useless
  if (first_priority != std::numeric_limits<int8_t>::min()) {
    return;
  }

  for (const auto &pri : this->sta_priorities_) {
    if (pri.priority != first_priority) {
      return;  // Not all same, nothing to do
    }
  }

  // All priorities are at minimum - clear the vector to save memory and reset
  ESP_LOGD(TAG, "Clearing BSSID priorities (all at minimum)");
  this->sta_priorities_.clear();
  this->sta_priorities_.shrink_to_fit();
}

/// Log failed connection attempt and decrease BSSID priority to avoid repeated failures
/// This function identifies which BSSID was attempted (from scan results or config),
/// decreases its priority by 1.0 to discourage future attempts, and logs the change.
///
/// The priority degradation system ensures that failed BSSIDs are automatically sorted
/// lower in subsequent scans, naturally cycling through different APs without explicit
/// BSSID tracking within a scan cycle.
///
/// Priority sources:
/// - SCAN_CONNECTING phase: Uses BSSID from scan_result_[0] (best match after sorting)
/// - Other phases: Uses BSSID from config if explicitly specified by user or fast_connect
///
/// If no BSSID is available (SSID-only connection), priority adjustment is skipped.
///
/// IMPORTANT: Priority is only decreased on the LAST attempt for a BSSID in SCAN_CONNECTING phase.
/// This prevents false positives from transient WiFi stack state issues after scanning.
/// Single failures don't necessarily mean the AP is bad - two genuine failures provide
/// higher confidence before degrading priority and skipping the BSSID in future scans.
void WiFiComponent::log_and_adjust_priority_for_failed_connect_() {
  // Determine which BSSID we tried to connect to
  optional<bssid_t> failed_bssid;

  if (this->retry_phase_ == WiFiRetryPhase::SCAN_CONNECTING && !this->scan_result_.empty()) {
    // Scan-based phase: always use best result (index 0)
    failed_bssid = this->scan_result_[0].get_bssid();
  } else if (const WiFiAP *config = this->get_selected_sta_(); config && config->get_bssid()) {
    // Config has specific BSSID (fast_connect or user-specified)
    failed_bssid = *config->get_bssid();
  }

  if (!failed_bssid.has_value()) {
    return;  // No BSSID to penalize
  }

  // Get SSID for logging
  std::string ssid;
  if (this->retry_phase_ == WiFiRetryPhase::SCAN_CONNECTING && !this->scan_result_.empty()) {
    ssid = this->scan_result_[0].get_ssid();
  } else if (const WiFiAP *config = this->get_selected_sta_()) {
    ssid = config->get_ssid();
  }

  // Only decrease priority on the last attempt for this phase
  // This prevents false positives from transient WiFi stack issues
  uint8_t max_retries = get_max_retries_for_phase(this->retry_phase_);
  bool is_last_attempt = (this->num_retried_ + 1 >= max_retries);

  // Decrease priority only on last attempt to avoid false positives from transient failures
  int8_t old_priority = this->get_sta_priority(failed_bssid.value());
  int8_t new_priority = old_priority;

  if (is_last_attempt) {
    // Decrease priority, but clamp to int8_t::min to prevent overflow
    new_priority =
        (old_priority > std::numeric_limits<int8_t>::min()) ? (old_priority - 1) : std::numeric_limits<int8_t>::min();
    this->set_sta_priority(failed_bssid.value(), new_priority);
  }
  char bssid_s[18];
  format_mac_addr_upper(failed_bssid.value().data(), bssid_s);
  ESP_LOGD(TAG, "Failed " LOG_SECRET("'%s'") " " LOG_SECRET("(%s)") ", priority %d → %d", ssid.c_str(), bssid_s,
           old_priority, new_priority);

  // After adjusting priority, check if all priorities are now at minimum
  // If so, clear the vector to save memory and reset for fresh start
  this->clear_priorities_if_all_min_();
}

/// Handle target advancement or retry counter increment when staying in the same phase
/// This function is called when a connection attempt fails and determine_next_phase_() indicates
/// we should stay in the current phase. It decides whether to:
/// - Advance to the next target (AP in fast_connect, SSID in hidden mode)
/// - Or increment the retry counter to try the same target again
///
/// Phase-specific behavior:
/// - FAST_CONNECT_CYCLING_APS: Always advance to next AP (no retries per AP)
/// - RETRY_HIDDEN: Advance to next SSID after exhausting retries on current SSID
/// - Other phases: Increment retry counter (will retry same target)
void WiFiComponent::advance_to_next_target_or_increment_retry_() {
  WiFiRetryPhase current_phase = this->retry_phase_;

  // Check if we need to advance to next AP/SSID within the same phase
#ifdef USE_WIFI_FAST_CONNECT
  if (current_phase == WiFiRetryPhase::FAST_CONNECT_CYCLING_APS) {
    // Fast connect: always advance to next AP (no retries per AP)
    this->selected_sta_index_++;
    this->num_retried_ = 0;
    ESP_LOGD(TAG, "Next AP in %s", LOG_STR_ARG(retry_phase_to_log_string(this->retry_phase_)));
    return;
  }
#endif

  if (current_phase == WiFiRetryPhase::EXPLICIT_HIDDEN && this->num_retried_ + 1 >= WIFI_RETRY_COUNT_PER_SSID) {
    // Explicit hidden: exhausted retries on current SSID, find next explicitly hidden network
    // Stop when we reach a visible network (proceed to scanning)
    size_t next_index = this->selected_sta_index_ + 1;
    if (next_index < this->sta_.size() && this->sta_[next_index].get_hidden()) {
      this->selected_sta_index_ = static_cast<int8_t>(next_index);
      this->num_retried_ = 0;
      ESP_LOGD(TAG, "Next explicit hidden network at index %d", static_cast<int>(next_index));
      return;
    }
    // No more consecutive explicit hidden networks found - fall through to trigger phase change
  }

  if (current_phase == WiFiRetryPhase::RETRY_HIDDEN && this->num_retried_ + 1 >= WIFI_RETRY_COUNT_PER_SSID) {
    // Hidden mode: exhausted retries on current SSID, find next potentially hidden SSID
    // If first network is marked hidden, we went through EXPLICIT_HIDDEN phase
    // In that case, skip networks marked hidden:true (already tried)
    // Otherwise, include them (they haven't been tried yet)
    int8_t next_index = this->find_next_hidden_sta_(this->selected_sta_index_);
    if (next_index != -1) {
      // Found another potentially hidden SSID
      this->selected_sta_index_ = next_index;
      this->num_retried_ = 0;
      return;
    }
    // No more potentially hidden SSIDs - set selected_sta_index_ to -1 to trigger phase change
    // This ensures determine_next_phase_() will skip the RETRY_HIDDEN logic and transition out
    this->selected_sta_index_ = -1;
    // Return early - phase change will happen on next wifi_loop() iteration
    return;
  }

  // Don't increment retry counter if we're in a scan phase with no valid targets
  if (this->needs_scan_results_()) {
    return;
  }

  // Increment retry counter to try the same target again
  this->num_retried_++;
  ESP_LOGD(TAG, "Retry attempt %u/%u in phase %s", this->num_retried_ + 1,
           get_max_retries_for_phase(this->retry_phase_), LOG_STR_ARG(retry_phase_to_log_string(this->retry_phase_)));
}

void WiFiComponent::retry_connect() {
  this->log_and_adjust_priority_for_failed_connect_();

  // Determine next retry phase based on current state
  WiFiRetryPhase current_phase = this->retry_phase_;
  WiFiRetryPhase next_phase = this->determine_next_phase_();

  // Handle phase transitions (transition_to_phase_ handles same-phase no-op internally)
  if (this->transition_to_phase_(next_phase)) {
    return;  // Scan started or adapter restarted (which sets its own state)
  }

  if (next_phase == current_phase) {
    this->advance_to_next_target_or_increment_retry_();
  }

  this->error_from_callback_ = false;

  yield();
  // Check if we have a valid target before building params
  // After exhausting all networks in a phase, selected_sta_index_ may be -1
  // In that case, skip connection and let next wifi_loop() handle phase transition
  if (this->selected_sta_index_ >= 0) {
    WiFiAP params = this->build_params_for_current_phase_();
    this->start_connecting(params);
  }
}

#ifdef USE_RP2040
// RP2040's mDNS library (LEAmDNS) relies on LwipIntf::stateUpCB() to restart
// mDNS when the network interface reconnects. However, this callback is disabled
// in the arduino-pico framework. As a workaround, we block component setup until
// WiFi is connected, ensuring mDNS.begin() is called with an active connection.

bool WiFiComponent::can_proceed() {
  if (!this->has_sta() || this->state_ == WIFI_COMPONENT_STATE_DISABLED || this->ap_setup_) {
    return true;
  }
  return this->is_connected();
}
#endif

void WiFiComponent::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }
bool WiFiComponent::is_connected() {
  return this->state_ == WIFI_COMPONENT_STATE_STA_CONNECTED &&
         this->wifi_sta_connect_status_() == WiFiSTAConnectStatus::CONNECTED && !this->error_from_callback_;
}
void WiFiComponent::set_power_save_mode(WiFiPowerSaveMode power_save) {
  this->power_save_ = power_save;
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
  this->configured_power_save_ = power_save;
#endif
}

void WiFiComponent::set_passive_scan(bool passive) { this->passive_scan_ = passive; }

bool WiFiComponent::is_captive_portal_active_() {
#ifdef USE_CAPTIVE_PORTAL
  return captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active();
#else
  return false;
#endif
}
bool WiFiComponent::is_esp32_improv_active_() {
#ifdef USE_IMPROV
  return esp32_improv::global_improv_component != nullptr && esp32_improv::global_improv_component->is_active();
#else
  return false;
#endif
}

#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
bool WiFiComponent::request_high_performance() {
  // Already configured for high performance - request satisfied
  if (this->configured_power_save_ == WIFI_POWER_SAVE_NONE) {
    return true;
  }

  // Semaphore initialization failed
  if (this->high_performance_semaphore_ == nullptr) {
    return false;
  }

  // Give the semaphore (non-blocking). This increments the count.
  return xSemaphoreGive(this->high_performance_semaphore_) == pdTRUE;
}

bool WiFiComponent::release_high_performance() {
  // Already configured for high performance - nothing to release
  if (this->configured_power_save_ == WIFI_POWER_SAVE_NONE) {
    return true;
  }

  // Semaphore initialization failed
  if (this->high_performance_semaphore_ == nullptr) {
    return false;
  }

  // Take the semaphore (non-blocking). This decrements the count.
  return xSemaphoreTake(this->high_performance_semaphore_, 0) == pdTRUE;
}
#endif  // USE_ESP32 && USE_WIFI_RUNTIME_POWER_SAVE

#ifdef USE_WIFI_FAST_CONNECT
bool WiFiComponent::load_fast_connect_settings_(WiFiAP &params) {
  SavedWifiFastConnectSettings fast_connect_save{};

  if (this->fast_connect_pref_.load(&fast_connect_save)) {
    // Validate saved AP index
    if (fast_connect_save.ap_index < 0 || static_cast<size_t>(fast_connect_save.ap_index) >= this->sta_.size()) {
      ESP_LOGW(TAG, "AP index out of bounds");
      return false;
    }

    // Set selected index for future operations (save, retry, etc)
    this->selected_sta_index_ = fast_connect_save.ap_index;

    // Copy entire config, then override with fast connect data
    params = this->sta_[fast_connect_save.ap_index];

    // Override with saved BSSID/channel from fast connect (SSID/password/etc already copied from config)
    bssid_t bssid{};
    std::copy(fast_connect_save.bssid, fast_connect_save.bssid + 6, bssid.begin());
    params.set_bssid(bssid);
    params.set_channel(fast_connect_save.channel);
    // Fast connect uses specific BSSID+channel, not hidden network probe (even if config has hidden: true)
    params.set_hidden(false);

    ESP_LOGD(TAG, "Loaded fast_connect settings");
    return true;
  }

  return false;
}

void WiFiComponent::save_fast_connect_settings_() {
  bssid_t bssid = wifi_bssid();
  uint8_t channel = get_wifi_channel();
  // selected_sta_index_ is always valid here (called only after successful connection)
  // Fallback to 0 is defensive programming for robustness
  int8_t ap_index = this->selected_sta_index_ >= 0 ? this->selected_sta_index_ : 0;

  // Skip save if settings haven't changed (compare with previously saved settings to reduce flash wear)
  SavedWifiFastConnectSettings previous_save{};
  if (this->fast_connect_pref_.load(&previous_save) && memcmp(previous_save.bssid, bssid.data(), 6) == 0 &&
      previous_save.channel == channel && previous_save.ap_index == ap_index) {
    return;  // No change, nothing to save
  }

  SavedWifiFastConnectSettings fast_connect_save{};
  memcpy(fast_connect_save.bssid, bssid.data(), 6);
  fast_connect_save.channel = channel;
  fast_connect_save.ap_index = ap_index;

  this->fast_connect_pref_.save(&fast_connect_save);

  ESP_LOGD(TAG, "Saved fast_connect settings");
}
#endif

void WiFiAP::set_ssid(const std::string &ssid) { this->ssid_ = ssid; }
void WiFiAP::set_bssid(bssid_t bssid) { this->bssid_ = bssid; }
void WiFiAP::set_bssid(optional<bssid_t> bssid) { this->bssid_ = bssid; }
void WiFiAP::set_password(const std::string &password) { this->password_ = password; }
#ifdef USE_WIFI_WPA2_EAP
void WiFiAP::set_eap(optional<EAPAuth> eap_auth) { this->eap_ = std::move(eap_auth); }
#endif
void WiFiAP::set_channel(optional<uint8_t> channel) { this->channel_ = channel; }
#ifdef USE_WIFI_MANUAL_IP
void WiFiAP::set_manual_ip(optional<ManualIP> manual_ip) { this->manual_ip_ = manual_ip; }
#endif
void WiFiAP::set_hidden(bool hidden) { this->hidden_ = hidden; }
const std::string &WiFiAP::get_ssid() const { return this->ssid_; }
const optional<bssid_t> &WiFiAP::get_bssid() const { return this->bssid_; }
const std::string &WiFiAP::get_password() const { return this->password_; }
#ifdef USE_WIFI_WPA2_EAP
const optional<EAPAuth> &WiFiAP::get_eap() const { return this->eap_; }
#endif
const optional<uint8_t> &WiFiAP::get_channel() const { return this->channel_; }
#ifdef USE_WIFI_MANUAL_IP
const optional<ManualIP> &WiFiAP::get_manual_ip() const { return this->manual_ip_; }
#endif
bool WiFiAP::get_hidden() const { return this->hidden_; }

WiFiScanResult::WiFiScanResult(const bssid_t &bssid, std::string ssid, uint8_t channel, int8_t rssi, bool with_auth,
                               bool is_hidden)
    : bssid_(bssid),
      channel_(channel),
      rssi_(rssi),
      ssid_(std::move(ssid)),
      with_auth_(with_auth),
      is_hidden_(is_hidden) {}
bool WiFiScanResult::matches(const WiFiAP &config) const {
  if (config.get_hidden()) {
    // User configured a hidden network, only match actually hidden networks
    // don't match SSID
    if (!this->is_hidden_)
      return false;
  } else if (!config.get_ssid().empty()) {
    // check if SSID matches
    if (config.get_ssid() != this->ssid_)
      return false;
  } else {
    // network is configured without SSID - match other settings
  }
  // If BSSID configured, only match for correct BSSIDs
  if (config.get_bssid().has_value() && *config.get_bssid() != this->bssid_)
    return false;

#ifdef USE_WIFI_WPA2_EAP
  // BSSID requires auth but no PSK or EAP credentials given
  if (this->with_auth_ && (config.get_password().empty() && !config.get_eap().has_value()))
    return false;

  // BSSID does not require auth, but PSK or EAP credentials given
  if (!this->with_auth_ && (!config.get_password().empty() || config.get_eap().has_value()))
    return false;
#else
  // If PSK given, only match for networks with auth (and vice versa)
  if (config.get_password().empty() == this->with_auth_)
    return false;
#endif

  // If channel configured, only match networks on that channel.
  if (config.get_channel().has_value() && *config.get_channel() != this->channel_) {
    return false;
  }
  return true;
}
bool WiFiScanResult::get_matches() const { return this->matches_; }
void WiFiScanResult::set_matches(bool matches) { this->matches_ = matches; }
const bssid_t &WiFiScanResult::get_bssid() const { return this->bssid_; }
const std::string &WiFiScanResult::get_ssid() const { return this->ssid_; }
uint8_t WiFiScanResult::get_channel() const { return this->channel_; }
int8_t WiFiScanResult::get_rssi() const { return this->rssi_; }
bool WiFiScanResult::get_with_auth() const { return this->with_auth_; }
bool WiFiScanResult::get_is_hidden() const { return this->is_hidden_; }

bool WiFiScanResult::operator==(const WiFiScanResult &rhs) const { return this->bssid_ == rhs.bssid_; }

WiFiComponent *global_wifi_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi
#endif
