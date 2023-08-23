#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include "esphome/components/network/ip_address.h"

#include <string>
#include <vector>

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include <esp_wifi.h>
#include <WiFiType.h>
#include <WiFi.h>
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

namespace esphome {
namespace wifi {

struct SavedWifiSettings {
  char ssid[33];
  char password[65];
} PACKED;  // NOLINT

enum WiFiComponentState {
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
  /** WiFi is in STA(+AP) mode and currently connecting to an AP a second time.
   *
   * This is required because for some reason ESPs don't like to connect to WiFi APs directly after
   * a scan.
   * */
  WIFI_COMPONENT_STATE_STA_CONNECTING_2,
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
};
#endif  // USE_WIFI_WPA2_EAP

using bssid_t = std::array<uint8_t, 6>;

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
  void set_priority(float priority) { priority_ = priority; }
  void set_manual_ip(optional<ManualIP> manual_ip);
  void set_hidden(bool hidden);
  const std::string &get_ssid() const;
  const optional<bssid_t> &get_bssid() const;
  const std::string &get_password() const;
#ifdef USE_WIFI_WPA2_EAP
  const optional<EAPAuth> &get_eap() const;
#endif  // USE_WIFI_WPA2_EAP
  const optional<uint8_t> &get_channel() const;
  float get_priority() const { return priority_; }
  const optional<ManualIP> &get_manual_ip() const;
  bool get_hidden() const;

 protected:
  std::string ssid_;
  optional<bssid_t> bssid_;
  std::string password_;
#ifdef USE_WIFI_WPA2_EAP
  optional<EAPAuth> eap_;
#endif  // USE_WIFI_WPA2_EAP
  optional<uint8_t> channel_;
  float priority_{0};
  optional<ManualIP> manual_ip_;
  bool hidden_{false};
};

class WiFiScanResult {
 public:
  WiFiScanResult(const bssid_t &bssid, std::string ssid, uint8_t channel, int8_t rssi, bool with_auth, bool is_hidden);

  bool matches(const WiFiAP &config);

  bool get_matches() const;
  void set_matches(bool matches);
  const bssid_t &get_bssid() const;
  const std::string &get_ssid() const;
  uint8_t get_channel() const;
  int8_t get_rssi() const;
  bool get_with_auth() const;
  bool get_is_hidden() const;
  float get_priority() const { return priority_; }
  void set_priority(float priority) { priority_ = priority; }

  bool operator==(const WiFiScanResult &rhs) const;

 protected:
  bool matches_{false};
  bssid_t bssid_;
  std::string ssid_;
  uint8_t channel_;
  int8_t rssi_;
  bool with_auth_;
  bool is_hidden_;
  float priority_{0.0f};
};

struct WiFiSTAPriority {
  bssid_t bssid;
  float priority;
};

enum WiFiPowerSaveMode {
  WIFI_POWER_SAVE_NONE = 0,
  WIFI_POWER_SAVE_LIGHT,
  WIFI_POWER_SAVE_HIGH,
};

#ifdef USE_ESP_IDF
struct IDFWiFiEvent;
#endif

/// This component is responsible for managing the ESP WiFi interface.
class WiFiComponent : public Component {
 public:
  /// Construct a WiFiComponent.
  WiFiComponent();

  void set_sta(const WiFiAP &ap);
  void add_sta(const WiFiAP &ap);
  void clear_sta();

  /** Setup an Access Point that should be created if no connection to a station can be made.
   *
   * This can also be used without set_sta(). Then the AP will always be active.
   *
   * If both STA and AP are defined, then both will be enabled at startup, but if a connection to a station
   * can be made, the AP will be turned off again.
   */
  void set_ap(const WiFiAP &ap);
  WiFiAP get_ap() { return this->ap_; }

  void enable();
  void disable();
  bool is_disabled();
  void start_scanning();
  void check_scanning_finished();
  void start_connecting(const WiFiAP &ap, bool two);
  void set_fast_connect(bool fast_connect);
  void set_ap_timeout(uint32_t ap_timeout) { ap_timeout_ = ap_timeout; }

  void check_connecting_finished();

  void retry_connect();

  bool can_proceed() override;

  void set_reboot_timeout(uint32_t reboot_timeout);

  bool is_connected();

  void set_power_save_mode(WiFiPowerSaveMode power_save);
  void set_output_power(float output_power) { output_power_ = output_power; }

  void set_passive_scan(bool passive);

  void save_wifi_sta(const std::string &ssid, const std::string &password);
  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup WiFi interface.
  void setup() override;
  void start();
  void dump_config() override;
  /// WIFI setup_priority.
  float get_setup_priority() const override;
  float get_loop_priority() const override;

  /// Reconnect WiFi if required.
  void loop() override;

  bool has_sta() const;
  bool has_ap() const;

#ifdef USE_WIFI_11KV_SUPPORT
  void set_btm(bool btm);
  void set_rrm(bool rrm);
#endif

  network::IPAddress get_dns_address(int num);
  network::IPAddress get_ip_address();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);

  const std::vector<WiFiScanResult> &get_scan_result() const { return scan_result_; }

  network::IPAddress wifi_soft_ap_ip();

  bool has_sta_priority(const bssid_t &bssid) {
    for (auto &it : this->sta_priorities_) {
      if (it.bssid == bssid)
        return true;
    }
    return false;
  }
  float get_sta_priority(const bssid_t bssid) {
    for (auto &it : this->sta_priorities_) {
      if (it.bssid == bssid)
        return it.priority;
    }
    return 0.0f;
  }
  void set_sta_priority(const bssid_t bssid, float priority) {
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

  network::IPAddress wifi_sta_ip();
  std::string wifi_ssid();
  bssid_t wifi_bssid();

  int8_t wifi_rssi();

  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }

 protected:
  static std::string format_mac_addr(const uint8_t mac[6]);
  void setup_ap_config_();
  void print_connect_params_();

  void wifi_loop_();
  bool wifi_mode_(optional<bool> sta, optional<bool> ap);
  bool wifi_sta_pre_setup_();
  bool wifi_apply_output_power_(float output_power);
  bool wifi_apply_power_save_();
  bool wifi_sta_ip_config_(optional<ManualIP> manual_ip);
  bool wifi_apply_hostname_();
  bool wifi_sta_connect_(const WiFiAP &ap);
  void wifi_pre_setup_();
  WiFiSTAConnectStatus wifi_sta_connect_status_();
  bool wifi_scan_start_(bool passive);
  bool wifi_ap_ip_config_(optional<ManualIP> manual_ip);
  bool wifi_start_ap_(const WiFiAP &ap);
  bool wifi_disconnect_();
  int32_t wifi_channel_();
  network::IPAddress wifi_subnet_mask_();
  network::IPAddress wifi_gateway_ip_();
  network::IPAddress wifi_dns_ip_(int num);

  bool is_captive_portal_active_();
  bool is_esp32_improv_active_();

#ifdef USE_ESP8266
  static void wifi_event_callback(System_Event_t *event);
  void wifi_scan_done_callback_(void *arg, STATUS status);
  static void s_wifi_scan_done_callback(void *arg, STATUS status);
#endif

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
  void wifi_event_callback_(arduino_event_id_t event, arduino_event_info_t info);
  void wifi_scan_done_callback_();
#endif
#ifdef USE_ESP_IDF
  void wifi_process_event_(IDFWiFiEvent *data);
#endif

#ifdef USE_RP2040
  static int s_wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result);
  void wifi_scan_result(void *env, const cyw43_ev_scan_result_t *result);
#endif

  std::string use_address_;
  std::vector<WiFiAP> sta_;
  std::vector<WiFiSTAPriority> sta_priorities_;
  WiFiAP selected_ap_;
  bool fast_connect_{false};

  bool has_ap_{false};
  WiFiAP ap_;
  WiFiComponentState state_{WIFI_COMPONENT_STATE_OFF};
  uint32_t action_started_;
  uint8_t num_retried_{0};
  uint32_t last_connected_{0};
  uint32_t reboot_timeout_{};
  uint32_t ap_timeout_{};
  WiFiPowerSaveMode power_save_{WIFI_POWER_SAVE_NONE};
  bool error_from_callback_{false};
  std::vector<WiFiScanResult> scan_result_;
  bool scan_done_{false};
  bool ap_setup_{false};
  optional<float> output_power_;
  bool passive_scan_{false};
  ESPPreferenceObject pref_;
  bool has_saved_wifi_settings_{false};
#ifdef USE_WIFI_11KV_SUPPORT
  bool btm_{false};
  bool rrm_{false};
#endif
  bool enable_on_boot_;
};

extern WiFiComponent *global_wifi_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class WiFiConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return global_wifi_component->is_connected(); }
};

template<typename... Ts> class WiFiEnabledCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override { return !global_wifi_component->is_disabled(); }
};

template<typename... Ts> class WiFiEnableAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_wifi_component->enable(); }
};

template<typename... Ts> class WiFiDisableAction : public Action<Ts...> {
 public:
  void play(Ts... x) override { global_wifi_component->disable(); }
};

}  // namespace wifi
}  // namespace esphome
