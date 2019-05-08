#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"
#include <string>
#include <IPAddress.h>

#ifdef ARDUINO_ARCH_ESP32
#include <esp_wifi.h>
#include <WiFiType.h>
#include <WiFi.h>
#endif

#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFiType.h>
#include <ESP8266WiFi.h>

#ifdef ARDUINO_ESP8266_RELEASE_2_3_0
extern "C" {
#include <user_interface.h>
};
#endif
#endif

namespace esphome {
namespace wifi {

enum WiFiComponentState {
  /** Nothing has been initialized yet. Internal AP, if configured, is disabled at this point. */
  WIFI_COMPONENT_STATE_OFF = 0,
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

/// Struct for setting static IPs in WiFiComponent.
struct ManualIP {
  IPAddress static_ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;  ///< The first DNS server. 0.0.0.0 for default.
  IPAddress dns2;  ///< The second DNS server. 0.0.0.0 for default.
};

using bssid_t = std::array<uint8_t, 6>;

class WiFiAP {
 public:
  void set_ssid(const std::string &ssid);
  void set_bssid(bssid_t bssid);
  void set_bssid(optional<bssid_t> bssid);
  void set_password(const std::string &password);
  void set_channel(optional<uint8_t> channel);
  void set_manual_ip(optional<ManualIP> manual_ip);
  void set_hidden(bool hidden);
  const std::string &get_ssid() const;
  const optional<bssid_t> &get_bssid() const;
  const std::string &get_password() const;
  const optional<uint8_t> &get_channel() const;
  const optional<ManualIP> &get_manual_ip() const;
  bool get_hidden() const;

 protected:
  std::string ssid_;
  optional<bssid_t> bssid_;
  std::string password_;
  optional<uint8_t> channel_;
  optional<ManualIP> manual_ip_;
  bool hidden_{false};
};

class WiFiScanResult {
 public:
  WiFiScanResult(const bssid_t &bssid, const std::string &ssid, uint8_t channel, int8_t rssi, bool with_auth,
                 bool is_hidden);

  bool matches(const WiFiAP &config);

  bool get_matches() const;
  void set_matches(bool matches);
  const bssid_t &get_bssid() const;
  const std::string &get_ssid() const;
  uint8_t get_channel() const;
  int8_t get_rssi() const;
  bool get_with_auth() const;
  bool get_is_hidden() const;

 protected:
  bool matches_{false};
  bssid_t bssid_;
  std::string ssid_;
  uint8_t channel_;
  int8_t rssi_;
  bool with_auth_;
  bool is_hidden_;
};

enum WiFiPowerSaveMode {
  WIFI_POWER_SAVE_NONE = 0,
  WIFI_POWER_SAVE_LIGHT,
  WIFI_POWER_SAVE_HIGH,
};

/// This component is responsible for managing the ESP WiFi interface.
class WiFiComponent : public Component {
 public:
  /// Construct a WiFiComponent.
  WiFiComponent();

  void add_sta(const WiFiAP &ap);

  /** Setup an Access Point that should be created if no connection to a station can be made.
   *
   * This can also be used without set_sta(). Then the AP will always be active.
   *
   * If both STA and AP are defined, then both will be enabled at startup, but if a connection to a station
   * can be made, the AP will be turned off again.
   */
  void set_ap(const WiFiAP &ap);

  void start_scanning();
  void check_scanning_finished();
  void start_connecting(const WiFiAP &ap, bool two);
  void set_fast_connect(bool fast_connect);

  void check_connecting_finished();

  void retry_connect();

  bool can_proceed() override;

  bool ready_for_ota();

  void set_reboot_timeout(uint32_t reboot_timeout);

  bool is_connected();

  void set_power_save_mode(WiFiPowerSaveMode power_save);

  // ========== INTERNAL METHODS ==========
  // (In most use cases you won't need these)
  /// Setup WiFi interface.
  void setup() override;
  void dump_config() override;
  /// WIFI setup_priority.
  float get_setup_priority() const override;
  float get_loop_priority() const override;

  /// Reconnect WiFi if required.
  void loop() override;

  bool has_sta() const;
  bool has_ap() const;

  IPAddress get_ip_address();
  std::string get_use_address() const;
  void set_use_address(const std::string &use_address);

 protected:
  static std::string format_mac_addr(const uint8_t mac[6]);
  void setup_ap_config_();
  void print_connect_params_();

  bool wifi_mode_(optional<bool> sta, optional<bool> ap);
  bool wifi_disable_auto_connect_();
  bool wifi_apply_power_save_();
  bool wifi_sta_ip_config_(optional<ManualIP> manual_ip);
  IPAddress wifi_sta_ip_();
  bool wifi_apply_hostname_();
  bool wifi_sta_connect_(WiFiAP ap);
  void wifi_register_callbacks_();
  wl_status_t wifi_sta_status_();
  bool wifi_scan_start_();
  bool wifi_ap_ip_config_(optional<ManualIP> manual_ip);
  bool wifi_start_ap_(const WiFiAP &ap);
  IPAddress wifi_soft_ap_ip_();

#ifdef ARDUINO_ARCH_ESP8266
  static void wifi_event_callback(System_Event_t *event);
  void wifi_scan_done_callback_(void *arg, STATUS status);
  static void s_wifi_scan_done_callback(void *arg, STATUS status);
#endif

#ifdef ARDUINO_ARCH_ESP32
  void wifi_event_callback_(system_event_id_t event, system_event_info_t info);
  void wifi_scan_done_callback_();
#endif

  std::string use_address_;
  std::vector<WiFiAP> sta_;
  WiFiAP selected_ap_;
  bool fast_connect_{false};

  WiFiAP ap_;
  WiFiComponentState state_{WIFI_COMPONENT_STATE_OFF};
  uint32_t action_started_;
  uint8_t num_retried_{0};
  uint32_t last_connected_{0};
  uint32_t reboot_timeout_{300000};
  WiFiPowerSaveMode power_save_{WIFI_POWER_SAVE_NONE};
  bool error_from_callback_{false};
  std::vector<WiFiScanResult> scan_result_;
  bool scan_done_{false};
  bool ap_setup_{false};
};

extern WiFiComponent *global_wifi_component;

template<typename... Ts> class WiFiConnectedCondition : public Condition<Ts...> {
 public:
  bool check(Ts... x) override;
};

template<typename... Ts> bool WiFiConnectedCondition<Ts...>::check(Ts... x) {
  return global_wifi_component->is_connected();
}

}  // namespace wifi
}  // namespace esphome
