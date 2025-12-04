#pragma once

#include "esphome/core/defines.h"
#ifdef USE_WIFI
#include "esphome/components/network/ip_address.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <string>
#include <vector>

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <WiFi.h>
#include <WiFiType.h>
#include <esp_wifi.h>
#endif

#ifdef USE_LIBRETINY
#include <WiFi.h>
#endif

#if defined(USE_ESP32) && defined(USE_WIFI_WPA2_EAP)
#if (ESP_IDF_VERSION_MAJOR >= 5) && (ESP_IDF_VERSION_MINOR >= 1)
#include <esp_eap_client.h>
#else
#include <esp_wpa2.h>
#endif
#endif

#ifdef USE_ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WiFiType.h>

#if defined(USE_ESP8266) && USE_ARDUINO_VERSION_CODE < VERSION_CODE(2, 4, 0)
extern "C" {
#include <user_interface.h>
};
#endif
#endif

#ifdef USE_RP2040
extern "C" {
#include "cyw43.h"
#include "cyw43_country.h"
#include "pico/cyw43_arch.h"
}

#include <WiFi.h>
#endif

#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#endif

namespace esphome::wifi {

/// Sentinel value for RSSI when WiFi is not connected
static constexpr int8_t WIFI_RSSI_DISCONNECTED = -127;

struct SavedWifiSettings {
  char ssid[33];
  char password[65];
} PACKED;  // NOLINT

struct SavedWifiFastConnectSettings {
  uint8_t bssid[6];
  uint8_t channel;
  int8_t ap_index;
} PACKED;  // NOLINT

enum WiFiComponentState : uint8_t {
  /** Nothing has been initialized yet. Internal AP, if configured, is disabled at this point. */
  WIFI_COMPONENT_STATE_OFF = 0,
  /** WiFi is disabled. */
  WIFI_COMPONENT_STATE_DISABLED,
  /** WiFi is in cooldown mode because something went wrong, scanning will begin after a short period of time. */
  WIFI_COMPONENT_STATE_COOLDOWN,
  /** WiFi is in STA-only mode and currently scanning for APs. */
  WIFI_COMPONENT_STATE_STA_SCANNING,
  /** WiFi is in STA(+AP) mode and currently connecting to an AP. */
  WIFI_COMPONENT_STATE_STA_CONNECTING,
  /** WiFi is in STA(+AP) mode and successfully connected. */
  WIFI_COMPONENT_STATE_STA_CONNECTED,
  /** WiFi is in AP-only mode and internal AP is already enabled. */
  WIFI_COMPONENT_STATE_AP,
};

enum class WiFiSTAConnectStatus : int {
  IDLE,
  CONNECTING,
  CONNECTED,
  ERROR_NETWORK_NOT_FOUND,
  ERROR_CONNECT_FAILED,
};

/// Tracks the current retry strategy/phase for WiFi connection attempts
enum class WiFiRetryPhase : uint8_t {
  /// Initial connection attempt (varies based on fast_connect setting)
  INITIAL_CONNECT,
#ifdef USE_WIFI_FAST_CONNECT
  /// Fast connect mode: cycling through configured APs (config-only, no scan)
  FAST_CONNECT_CYCLING_APS,
#endif
  /// Explicitly hidden networks (user marked as hidden, try before scanning)
  EXPLICIT_HIDDEN,
  /// Scan-based: connecting to best AP from scan results
  SCAN_CONNECTING,
  /// Retry networks not found in scan (might be hidden)
  RETRY_HIDDEN,
  /// Restarting WiFi adapter to clear stuck state
  RESTARTING_ADAPTER,
};

/// Struct for setting static IPs in WiFiComponent.
struct ManualIP {
  network::IPAddress static_ip;
  network::IPAddress gateway;
  network::IPAddress subnet;
  network::IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  network::IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
};

#ifdef USE_WIFI_WPA2_EAP
struct EAPAuth {
  std::string identity;  // required for all auth types
  std::string username;
  std::string password;
  const char *ca_cert;  // optionally verify authentication server
  // used for EAP-TLS
  const char *client_cert;
  const char *client_key;
// used for EAP-TTLS
#ifdef USE_ESP32
  esp_eap_ttls_phase2_types ttls_phase_2;
#endif
};
#endif  // USE_WIFI_WPA2_EAP

using bssid_t = std::array<uint8_t, 6>;

// Use std::vector for RP2040 since scan count is unknown (callback-based)
// Use FixedVector for other platforms where count is queried first
#ifdef USE_RP2040
template<typename T> using wifi_scan_vector_t = std::vector<T>;
#else
template<typename T> using wifi_scan_vector_t = FixedVector<T>;
#endif

class WiFiAP {
 public:
  void set_ssid(const std::string &ssid);
  void set_bssid(bssid_t bssid);
  void set_bssid(optional<bssid_t> bssid);
  void set_password(const std::string &password);
#ifdef USE_WIFI_WPA2_EAP
  void set_eap(optional<EAPAuth> eap_auth);
#endif  // USE_WIFI_WPA2_EAP
  void set_channel(optional<uint8_t> channel);
  void set_priority(int8_t priority) { priority_ = priority; }
#ifdef USE_WIFI_MANUAL_IP
  void set_manual_ip(optional<ManualIP> manual_ip);
#endif
  void set_hidden(bool hidden);
  const std::string &get_ssid() const;
  const optional<bssid_t> &get_bssid() const;
  const std::string &get_password() const;
#ifdef USE_WIFI_WPA2_EAP
  const optional<EAPAuth> &get_eap() const;
#endif  // USE_WIFI_WPA2_EAP
  const optional<uint8_t> &get_channel() const;
  int8_t get_priority() const { return priority_; }
#ifdef USE_WIFI_MANUAL_IP
  const optional<ManualIP> &get_manual_ip() const;
#endif
  bool get_hidden() const;

 protected:
  std::string ssid_;
  std::string password_;
  optional<bssid_t> bssid_;
#ifdef USE_WIFI_WPA2_EAP
  optional<EAPAuth> eap_;
#endif  // USE_WIFI_WPA2_EAP
#ifdef USE_WIFI_MANUAL_IP
  optional<ManualIP> manual_ip_;
#endif
  optional<uint8_t> channel_;
  int8_t priority_{0};
  bool hidden_{false};
};

class WiFiScanResult {
 public:
  WiFiScanResult(const bssid_t &bssid, std::string ssid, uint8_t channel, int8_t rssi, bool with_auth, bool is_hidden);

  bool matches(const WiFiAP &config) const;

  bool get_matches() const;
  void set_matches(bool matches);
  const bssid_t &get_bssid() const;
  const std::string &get_ssid() const;
  uint8_t get_channel() const;
  int8_t get_rssi() const;
  bool get_with_auth() const;
  bool get_is_hidden() const;
  int8_t get_priority() const { return priority_; }
  void set_priority(int8_t priority) { priority_ = priority; }

  bool operator==(const WiFiScanResult &rhs) const;

 protected:
  bssid_t bssid_;
  uint8_t channel_;
  int8_t rssi_;
  std::string ssid_;
  int8_t priority_{0};
  bool matches_{false};
  bool with_auth_;
  bool is_hidden_;
};

struct WiFiSTAPriority {
  bssid_t bssid;
  int8_t priority;
};

enum WiFiPowerSaveMode : uint8_t {
  WIFI_POWER_SAVE_NONE = 0,
  WIFI_POWER_SAVE_LIGHT,
  WIFI_POWER_SAVE_HIGH,
};

enum WifiMinAuthMode : uint8_t {
  WIFI_MIN_AUTH_MODE_WPA = 0,
  WIFI_MIN_AUTH_MODE_WPA2,
  WIFI_MIN_AUTH_MODE_WPA3,
};

#ifdef USE_ESP32
struct IDFWiFiEvent;
#endif

/** Listener interface for WiFi IP state changes.
 *
 * Components can implement this interface to receive IP address updates
 * without the overhead of std::function callbacks.
 */
class WiFiIPStateListener {
 public:
  virtual void on_ip_state(const network::IPAddresses &ips, const network::IPAddress &dns1,
                           const network::IPAddress &dns2) = 0;
};

/** Listener interface for WiFi scan results.
 *
 * Components can implement this interface to receive scan results
 * without the overhead of std::function callbacks.
 */
class WiFiScanResultsListener {
 public:
  virtual void on_wifi_scan_results(const wifi_scan_vector_t<WiFiScanResult> &results) = 0;
};

/** Listener interface for WiFi connection state changes.
 *
 * Components can implement this interface to receive connection updates
 * without the overhead of std::function callbacks.
 */
class WiFiConnectStateListener {
 public:
  virtual void on_wifi_connect_state(const std::string &ssid, const bssid_t &bssid) = 0;
};

/** Listener interface for WiFi power save mode changes.
 *
 * Components can implement this interface to receive power save mode updates
 * without the overhead of std::function callbacks.
 */
class WiFiPowerSaveListener {
 public:
  virtual void on_wifi_power_save(WiFiPowerSaveMode mode) = 0;
};

/// This component is responsible for managing the ESP WiFi interface.
class WiFiComponent : public Component {
 public:
  /// Construct a WiFiComponent.
  WiFiComponent();

  void set_sta(const WiFiAP &ap);
  // Returns a copy of the currently selected AP configuration
  WiFiAP get_sta() const;
  void init_sta(size_t count);
  void add_sta(const WiFiAP &ap);
  void clear_sta() {
    this->sta_.clear();
    this->selected_sta_index_ = -1;
  }

#ifdef USE_WIFI_AP
  /** Setup an Access Point that should be created if no connection to a station can be made.
   *
   * This can also be used without set_sta(). Then the AP will always be active.
   *
   * If both STA and AP are defined, then both will be enabled at startup, but if a connection to a station
   * can be made, the AP will be turned off again.
   */
  void set_ap(const WiFiAP &ap);
  WiFiAP get_ap() { return this->ap_; }
  void set_ap_timeout(uint32_t ap_timeout) { ap_timeout_ = ap_timeout; }
#endif  // USE_WIFI_AP

  void enable();
  void disable();
  bool is_disabled();
  void start_scanning();
  void check_scanning_finished();
  void start_connecting(const WiFiAP &ap);
  // Backward compatibility overload - ignores 'two' parameter
  void start_connecting(const WiFiAP &ap, bool /* two */) { this->start_connecting(ap); }

  void check_connecting_finished();

  void retry_connect();

#ifdef USE_RP2040
  bool can_proceed() override;
#endif

  void set_reboot_timeout(uint32_t reboot_timeout);

  bool is_connected();

  void set_power_save_mode(WiFiPowerSaveMode power_save);
  void set_min_auth_mode(WifiMinAuthMode min_auth_mode) { min_auth_mode_ = min_auth_mode; }
  void set_output_power(float output_power) { output_power_ = output_power; }

  void set_passive_scan(bool passive);

  void save_wifi_sta(const std::string &ssid, const std::string &password);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup WiFi interface.
  void setup() override;
  void start();
  void dump_config() override;
  void restart_adapter();
  /// WIFI setup_priority.
  float get_setup_priority() const override;
  float get_loop_priority() const override;

  /// Reconnect WiFi if required.
  void loop() override;

  bool has_sta() const;
  bool has_ap() const;
  bool is_ap_active() const;

#ifdef USE_WIFI_11KV_SUPPORT
  void set_btm(bool btm);
  void set_rrm(bool rrm);
#endif

  network::IPAddress get_dns_address(int num);
  network::IPAddresses get_ip_addresses();
  const char *get_use_address() const;
  void set_use_address(const char *use_address);

  const wifi_scan_vector_t<WiFiScanResult> &get_scan_result() const { return scan_result_; }

  network::IPAddress wifi_soft_ap_ip();

  bool has_sta_priority(const bssid_t &bssid) {
    for (auto &it : this->sta_priorities_) {
      if (it.bssid == bssid)
        return true;
    }
    return false;
  }
  int8_t get_sta_priority(const bssid_t bssid) {
    for (auto &it : this->sta_priorities_) {
      if (it.bssid == bssid)
        return it.priority;
    }
    return 0;
  }
  void set_sta_priority(const bssid_t bssid, int8_t priority) {
    for (auto &it : this->sta_priorities_) {
      if (it.bssid == bssid) {
        it.priority = priority;
        return;
      }
    }
    this->sta_priorities_.push_back(WiFiSTAPriority{
        .bssid = bssid,
        .priority = priority,
    });
  }

  network::IPAddresses wifi_sta_ip_addresses();
  std::string wifi_ssid();
  bssid_t wifi_bssid();

  int8_t wifi_rssi();

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }
  void set_keep_scan_results(bool keep_scan_results) { this->keep_scan_results_ = keep_scan_results; }

  Trigger<> *get_connect_trigger() const { return this->connect_trigger_; };
  Trigger<> *get_disconnect_trigger() const { return this->disconnect_trigger_; };

  int32_t get_wifi_channel();

#ifdef USE_WIFI_LISTENERS
  /** Add a listener for IP state changes.
   * Listener receives: IP addresses, DNS address 1, DNS address 2
   */
  void add_ip_state_listener(WiFiIPStateListener *listener) { this->ip_state_listeners_.push_back(listener); }
  /// Add a listener for WiFi scan results
  void add_scan_results_listener(WiFiScanResultsListener *listener) {
    this->scan_results_listeners_.push_back(listener);
  }
  /** Add a listener for WiFi connection state changes.
   * Listener receives: SSID, BSSID
   */
  void add_connect_state_listener(WiFiConnectStateListener *listener) {
    this->connect_state_listeners_.push_back(listener);
  }
  /** Add a listener for WiFi power save mode changes.
   * Listener receives: WiFiPowerSaveMode
   */
  void add_power_save_listener(WiFiPowerSaveListener *listener) { this->power_save_listeners_.push_back(listener); }
#endif  // USE_WIFI_LISTENERS

#ifdef USE_WIFI_RUNTIME_POWER_SAVE
  /** Request high-performance mode (no power saving) for improved WiFi latency.
   *
   * Components that need maximum WiFi performance (e.g., audio streaming, large data transfers)
   * can call this method to temporarily disable WiFi power saving. Multiple components can
   * request high performance simultaneously using a counting semaphore.
   *
   * Power saving will be restored to the YAML-configured mode when all components have
   * called release_high_performance().
   *
   * Note: Only supported on ESP32.
   *
   * @return true if request was satisfied (high-performance mode active or already configured),
   *         false if operation failed (semaphore error)
   */
  bool request_high_performance();

  /** Release a high-performance mode request.
   *
   * Should be called when a component no longer needs maximum WiFi latency.
   * When all requests are released (semaphore count reaches zero), WiFi power saving
   * is restored to the YAML-configured mode.
   *
   * Note: Only supported on ESP32.
   *
   * @return true if release was successful (or already in high-performance config),
   *         false if operation failed (semaphore error)
   */
  bool release_high_performance();
#endif  // USE_WIFI_RUNTIME_POWER_SAVE

 protected:
#ifdef USE_WIFI_AP
  void setup_ap_config_();
#endif  // USE_WIFI_AP

  void print_connect_params_();
  WiFiAP build_params_for_current_phase_();

  /// Determine next retry phase based on current state and failure conditions
  WiFiRetryPhase determine_next_phase_();
  /// Transition to a new retry phase with logging
  /// Returns true if a scan was started (caller should wait), false otherwise
  bool transition_to_phase_(WiFiRetryPhase new_phase);
  /// Check if we need valid scan results for the current phase but don't have any
  /// Returns true if the phase requires scan results but they're missing or don't match
  bool needs_scan_results_() const;
  /// Check if we went through EXPLICIT_HIDDEN phase (first network is marked hidden)
  /// Used in RETRY_HIDDEN to determine whether to skip explicitly hidden networks
  bool went_through_explicit_hidden_phase_() const;
  /// Find the index of the first non-hidden network
  /// Returns where EXPLICIT_HIDDEN phase would have stopped, or -1 if all networks are hidden
  int8_t find_first_non_hidden_index_() const;
  /// Check if an SSID was seen in the most recent scan results
  /// Used to skip hidden mode for SSIDs we know are visible
  bool ssid_was_seen_in_scan_(const std::string &ssid) const;
  /// Find next SSID that wasn't in scan results (might be hidden)
  /// Returns index of next potentially hidden SSID, or -1 if none found
  /// @param start_index Start searching from index after this (-1 to start from beginning)
  int8_t find_next_hidden_sta_(int8_t start_index);
  /// Log failed connection and decrease BSSID priority to avoid repeated attempts
  void log_and_adjust_priority_for_failed_connect_();
  /// Clear BSSID priority tracking if all priorities are at minimum (saves memory)
  void clear_priorities_if_all_min_();
  /// Advance to next target (AP/SSID) within current phase, or increment retry counter
  /// Called when staying in the same phase after a failed connection attempt
  void advance_to_next_target_or_increment_retry_();
  /// Start initial connection - either scan or connect directly to hidden networks
  void start_initial_connection_();
  const WiFiAP *get_selected_sta_() const {
    if (this->selected_sta_index_ >= 0 && static_cast<size_t>(this->selected_sta_index_) < this->sta_.size()) {
      return &this->sta_[this->selected_sta_index_];
    }
    return nullptr;
  }

  void reset_selected_ap_to_first_if_invalid_() {
    if (this->selected_sta_index_ < 0 || static_cast<size_t>(this->selected_sta_index_) >= this->sta_.size()) {
      this->selected_sta_index_ = this->sta_.empty() ? -1 : 0;
    }
  }

  bool all_networks_hidden_() const {
    if (this->sta_.empty())
      return false;
    for (const auto &ap : this->sta_) {
      if (!ap.get_hidden())
        return false;
    }
    return true;
  }

  void connect_soon_();

  void wifi_loop_();
  bool wifi_mode_(optional<bool> sta, optional<bool> ap);
  bool wifi_sta_pre_setup_();
  bool wifi_apply_output_power_(float output_power);
  bool wifi_apply_power_save_();
  bool wifi_sta_ip_config_(const optional<ManualIP> &manual_ip);
  bool wifi_apply_hostname_();
  bool wifi_sta_connect_(const WiFiAP &ap);
  void wifi_pre_setup_();
  WiFiSTAConnectStatus wifi_sta_connect_status_();
  bool wifi_scan_start_(bool passive);

#ifdef USE_WIFI_AP
  bool wifi_ap_ip_config_(const optional<ManualIP> &manual_ip);
  bool wifi_start_ap_(const WiFiAP &ap);
#endif  // USE_WIFI_AP

  bool wifi_disconnect_();

  network::IPAddress wifi_subnet_mask_();
  network::IPAddress wifi_gateway_ip_();
  network::IPAddress wifi_dns_ip_(int num);

  bool is_captive_portal_active_();
  bool is_esp32_improv_active_();

#ifdef USE_WIFI_FAST_CONNECT
  bool load_fast_connect_settings_(WiFiAP &params);
  void save_fast_connect_settings_();
#endif

#ifdef USE_ESP8266
  static void wifi_event_callback(System_Event_t *event);
  void wifi_scan_done_callback_(void *arg, STATUS status);
  static void s_wifi_scan_done_callback(void *arg, STATUS status);
#endif

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  void wifi_event_callback_(arduino_event_id_t event, arduino_event_info_t info);
  void wifi_scan_done_callback_();
#endif
#ifdef USE_ESP32
  void wifi_process_event_(IDFWiFiEvent *data);
#endif

#ifdef USE_RP2040
  static int s_wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result);
  void wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result);
#endif

#ifdef USE_LIBRETINY
  void wifi_event_callback_(arduino_event_id_t event, arduino_event_info_t info);
  void wifi_scan_done_callback_();
#endif

  FixedVector<WiFiAP> sta_;
  std::vector<WiFiSTAPriority> sta_priorities_;
  wifi_scan_vector_t<WiFiScanResult> scan_result_;
#ifdef USE_WIFI_AP
  WiFiAP ap_;
#endif
  optional<float> output_power_;
#ifdef USE_WIFI_LISTENERS
  std::vector<WiFiIPStateListener *> ip_state_listeners_;
  std::vector<WiFiScanResultsListener *> scan_results_listeners_;
  std::vector<WiFiConnectStateListener *> connect_state_listeners_;
  std::vector<WiFiPowerSaveListener *> power_save_listeners_;
#endif  // USE_WIFI_LISTENERS
  ESPPreferenceObject pref_;
#ifdef USE_WIFI_FAST_CONNECT
  ESPPreferenceObject fast_connect_pref_;
#endif

  // Group all 32-bit integers together
  uint32_t action_started_;
  uint32_t last_connected_{0};
  uint32_t reboot_timeout_{};
#ifdef USE_WIFI_AP
  uint32_t ap_timeout_{};
#endif

  // Group all 8-bit values together
  WiFiComponentState state_{WIFI_COMPONENT_STATE_OFF};
  WiFiPowerSaveMode power_save_{WIFI_POWER_SAVE_NONE};
  WifiMinAuthMode min_auth_mode_{WIFI_MIN_AUTH_MODE_WPA2};
  WiFiRetryPhase retry_phase_{WiFiRetryPhase::INITIAL_CONNECT};
  uint8_t num_retried_{0};
  // Index into sta_ array for the currently selected AP configuration (-1 = none selected)
  // Used to access password, manual_ip, priority, EAP settings, and hidden flag
  // int8_t limits to 127 APs (enforced in __init__.py via MAX_WIFI_NETWORKS)
  int8_t selected_sta_index_{-1};

#if USE_NETWORK_IPV6
  uint8_t num_ipv6_addresses_{0};
#endif /* USE_NETWORK_IPV6 */

  // Group all boolean values together
  bool has_ap_{false};
  bool handled_connected_state_{false};
  bool error_from_callback_{false};
  bool scan_done_{false};
  bool ap_setup_{false};
  bool ap_started_{false};
  bool passive_scan_{false};
  bool has_saved_wifi_settings_{false};
#ifdef USE_WIFI_11KV_SUPPORT
  bool btm_{false};
  bool rrm_{false};
#endif
  bool enable_on_boot_{true};
  bool got_ipv4_address_{false};
  bool keep_scan_results_{false};
  bool did_scan_this_cycle_{false};
  bool skip_cooldown_next_cycle_{false};
#if defined(USE_ESP32) && defined(USE_WIFI_RUNTIME_POWER_SAVE)
  WiFiPowerSaveMode configured_power_save_{WIFI_POWER_SAVE_NONE};
  bool is_high_performance_mode_{false};

  SemaphoreHandle_t high_performance_semaphore_{nullptr};
#endif

  // Pointers at the end (naturally aligned)
  Trigger<> *connect_trigger_{new Trigger<>()};
  Trigger<> *disconnect_trigger_{new Trigger<>()};

 private:
  // Stores a pointer to a string literal (static storage duration).
  // ONLY set from Python-generated code with string literals - never dynamic strings.
  const char *use_address_{""};
};

extern WiFiComponent *global_wifi_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::wifi
#endif
