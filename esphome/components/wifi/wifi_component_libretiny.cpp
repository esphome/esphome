#include "wifi_component.h"

#ifdef USE_WIFI
#ifdef USE_LIBRETINY

#include <cinttypes>
#include <utility>
#include <algorithm>
#include "lwip/ip_addr.h"
#include "lwip/err.h"
#include "lwip/dns.h"

#include <FreeRTOS.h>
#include <queue.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"

namespace esphome::wifi {

static const char *const TAG = "wifi_lt";

// Thread-safe event handling for LibreTiny WiFi
//
// LibreTiny's WiFi.onEvent() callback runs in the WiFi driver's thread context,
// not the main ESPHome loop. Without synchronization, modifying shared state
// (like connection status flags) from the callback causes race conditions:
// - The main loop may never see state changes (values cached in registers)
// - State changes may be visible in inconsistent order
// - LibreTiny targets (BK7231, RTL8720) lack atomic instructions (no LDREX/STREX)
//
// Solution: Queue events in the callback and process them in the main loop.
// This is the same approach used by ESP32 IDF's wifi_process_event_().
// All state modifications happen in the main loop context, eliminating races.

static constexpr size_t EVENT_QUEUE_SIZE = 16;  // Max pending WiFi events before overflow
static QueueHandle_t s_event_queue = nullptr;   // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
static volatile uint32_t s_event_queue_overflow_count =
    0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Event structure for queued WiFi events - contains a copy of event data
// to avoid lifetime issues with the original event data from the callback
struct LTWiFiEvent {
  arduino_event_id_t event_id;
  union {
    struct {
      uint8_t ssid[33];
      uint8_t ssid_len;
      uint8_t bssid[6];
      uint8_t channel;
      uint8_t authmode;
    } sta_connected;
    struct {
      uint8_t ssid[33];
      uint8_t ssid_len;
      uint8_t bssid[6];
      uint8_t reason;
    } sta_disconnected;
    struct {
      uint8_t old_mode;
      uint8_t new_mode;
    } sta_authmode_change;
    struct {
      uint32_t status;
      uint8_t number;
      uint8_t scan_id;
    } scan_done;
    struct {
      uint8_t mac[6];
      int rssi;
    } ap_probe_req;
  } data;
};

// Connection state machine - only modified from main loop after queue processing
enum class LTWiFiSTAState : uint8_t {
  IDLE,             // Not connecting
  CONNECTING,       // Connection in progress
  CONNECTED,        // Successfully connected with IP
  ERROR_NOT_FOUND,  // AP not found (probe failed)
  ERROR_FAILED,     // Connection failed (auth, timeout, etc.)
};

static LTWiFiSTAState s_sta_state = LTWiFiSTAState::IDLE;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

// Count of ignored disconnect events during connection - too many indicates real failure
static uint8_t s_ignored_disconnect_count = 0;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Threshold for ignored disconnect events before treating as connection failure
// LibreTiny sends spurious "Association Leave" events, but more than this many
// indicates the connection is failing repeatedly. Value of 3 balances fast failure
// detection with tolerance for occasional spurious events on successful connections.
static constexpr uint8_t IGNORED_DISCONNECT_THRESHOLD = 3;

bool WiFiComponent::wifi_mode_(optional<bool> sta, optional<bool> ap) {
  uint8_t current_mode = WiFi.getMode();
  bool current_sta = current_mode & 0b01;
  bool current_ap = current_mode & 0b10;
  bool enable_sta = sta.value_or(current_sta);
  bool enable_ap = ap.value_or(current_ap);
  if (current_sta == enable_sta && current_ap == enable_ap)
    return true;

  if (enable_sta && !current_sta) {
    ESP_LOGV(TAG, "Enabling STA");
  } else if (!enable_sta && current_sta) {
    ESP_LOGV(TAG, "Disabling STA");
  }
  if (enable_ap && !current_ap) {
    ESP_LOGV(TAG, "Enabling AP");
  } else if (!enable_ap && current_ap) {
    ESP_LOGV(TAG, "Disabling AP");
  }

  uint8_t mode = 0;
  if (enable_sta)
    mode |= 0b01;
  if (enable_ap)
    mode |= 0b10;
  bool ret = WiFi.mode(static_cast<wifi_mode_t>(mode));

  if (!ret) {
    ESP_LOGW(TAG, "Setting mode failed");
    return false;
  }

  this->ap_started_ = enable_ap;

  return ret;
}
bool WiFiComponent::wifi_apply_output_power_(float output_power) {
  int8_t val = static_cast<int8_t>(output_power * 4);
  return WiFi.setTxPower(val);
}
bool WiFiComponent::wifi_sta_pre_setup_() {
  if (!this->wifi_mode_(true, {}))
    return false;

  WiFi.setAutoReconnect(false);
  delay(10);
  return true;
}
bool WiFiComponent::wifi_apply_power_save_() {
  bool success = WiFi.setSleep(this->power_save_ != WIFI_POWER_SAVE_NONE);
#ifdef USE_WIFI_POWER_SAVE_LISTENERS
  if (success) {
    for (auto *listener : this->power_save_listeners_) {
      listener->on_wifi_power_save(this->power_save_);
    }
  }
#endif
  return success;
}
bool WiFiComponent::wifi_sta_ip_config_(const optional<ManualIP> &manual_ip) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  if (!manual_ip.has_value()) {
    return true;
  }

  WiFi.config(manual_ip->static_ip, manual_ip->gateway, manual_ip->subnet, manual_ip->dns1, manual_ip->dns2);

  return true;
}

network::IPAddresses WiFiComponent::wifi_sta_ip_addresses() {
  if (!this->has_sta())
    return {};
  network::IPAddresses addresses;
  addresses[0] = WiFi.localIP();
#if USE_NETWORK_IPV6
  int i = 1;
  auto v6_addresses = WiFi.allLocalIPv6();
  for (auto address : v6_addresses) {
    addresses[i++] = network::IPAddress(address.toString().c_str());
  }
#endif /* USE_NETWORK_IPV6 */
  return addresses;
}

bool WiFiComponent::wifi_apply_hostname_() {
  // setting is done in SYSTEM_EVENT_STA_START callback too
  WiFi.setHostname(App.get_name().c_str());
  return true;
}
bool WiFiComponent::wifi_sta_connect_(const WiFiAP &ap) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  String ssid = WiFi.SSID();
  if (ssid && strcmp(ssid.c_str(), ap.get_ssid().c_str()) != 0) {
    WiFi.disconnect();
  }

#ifdef USE_WIFI_MANUAL_IP
  if (!this->wifi_sta_ip_config_(ap.get_manual_ip())) {
    return false;
  }
#else
  if (!this->wifi_sta_ip_config_({})) {
    return false;
  }
#endif

  this->wifi_apply_hostname_();

  // Reset state machine and disconnect counter before connecting
  s_sta_state = LTWiFiSTAState::CONNECTING;
  s_ignored_disconnect_count = 0;

  WiFiStatus status = WiFi.begin(ap.get_ssid().c_str(), ap.get_password().empty() ? NULL : ap.get_password().c_str(),
                                 ap.get_channel(),  // 0 = auto
                                 ap.has_bssid() ? ap.get_bssid().data() : NULL);
  if (status != WL_CONNECTED) {
    ESP_LOGW(TAG, "esp_wifi_connect failed: %d", status);
    return false;
  }

  return true;
}
const char *get_auth_mode_str(uint8_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:
      return "OPEN";
    case WIFI_AUTH_WEP:
      return "WEP";
    case WIFI_AUTH_WPA_PSK:
      return "WPA PSK";
    case WIFI_AUTH_WPA2_PSK:
      return "WPA2 PSK";
    case WIFI_AUTH_WPA_WPA2_PSK:
      return "WPA/WPA2 PSK";
    default:
      return "UNKNOWN";
  }
}

const char *get_op_mode_str(uint8_t mode) {
  switch (mode) {
    case WIFI_OFF:
      return "OFF";
    case WIFI_STA:
      return "STA";
    case WIFI_AP:
      return "AP";
    case WIFI_AP_STA:
      return "AP+STA";
    default:
      return "UNKNOWN";
  }
}
const char *get_disconnect_reason_str(uint8_t reason) {
  switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
      return "Auth Expired";
    case WIFI_REASON_AUTH_LEAVE:
      return "Auth Leave";
    case WIFI_REASON_ASSOC_EXPIRE:
      return "Association Expired";
    case WIFI_REASON_ASSOC_TOOMANY:
      return "Too Many Associations";
    case WIFI_REASON_NOT_AUTHED:
      return "Not Authenticated";
    case WIFI_REASON_NOT_ASSOCED:
      return "Not Associated";
    case WIFI_REASON_ASSOC_LEAVE:
      return "Association Leave";
    case WIFI_REASON_ASSOC_NOT_AUTHED:
      return "Association not Authenticated";
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
      return "Disassociate Power Cap Bad";
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
      return "Disassociate Supported Channel Bad";
    case WIFI_REASON_IE_INVALID:
      return "IE Invalid";
    case WIFI_REASON_MIC_FAILURE:
      return "Mic Failure";
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
      return "4-Way Handshake Timeout";
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
      return "Group Key Update Timeout";
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
      return "IE In 4-Way Handshake Differs";
    case WIFI_REASON_GROUP_CIPHER_INVALID:
      return "Group Cipher Invalid";
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
      return "Pairwise Cipher Invalid";
    case WIFI_REASON_AKMP_INVALID:
      return "AKMP Invalid";
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
      return "Unsupported RSN IE version";
    case WIFI_REASON_INVALID_RSN_IE_CAP:
      return "Invalid RSN IE Cap";
    case WIFI_REASON_802_1X_AUTH_FAILED:
      return "802.1x Authentication Failed";
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
      return "Cipher Suite Rejected";
    case WIFI_REASON_BEACON_TIMEOUT:
      return "Beacon Timeout";
    case WIFI_REASON_NO_AP_FOUND:
      return "AP Not Found";
    case WIFI_REASON_AUTH_FAIL:
      return "Authentication Failed";
    case WIFI_REASON_ASSOC_FAIL:
      return "Association Failed";
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return "Handshake Failed";
    case WIFI_REASON_CONNECTION_FAIL:
      return "Connection Failed";
    case WIFI_REASON_UNSPECIFIED:
    default:
      return "Unspecified";
  }
}

#define ESPHOME_EVENT_ID_WIFI_READY ARDUINO_EVENT_WIFI_READY
#define ESPHOME_EVENT_ID_WIFI_SCAN_DONE ARDUINO_EVENT_WIFI_SCAN_DONE
#define ESPHOME_EVENT_ID_WIFI_STA_START ARDUINO_EVENT_WIFI_STA_START
#define ESPHOME_EVENT_ID_WIFI_STA_STOP ARDUINO_EVENT_WIFI_STA_STOP
#define ESPHOME_EVENT_ID_WIFI_STA_CONNECTED ARDUINO_EVENT_WIFI_STA_CONNECTED
#define ESPHOME_EVENT_ID_WIFI_STA_DISCONNECTED ARDUINO_EVENT_WIFI_STA_DISCONNECTED
#define ESPHOME_EVENT_ID_WIFI_STA_AUTHMODE_CHANGE ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE
#define ESPHOME_EVENT_ID_WIFI_STA_GOT_IP ARDUINO_EVENT_WIFI_STA_GOT_IP
#define ESPHOME_EVENT_ID_WIFI_STA_GOT_IP6 ARDUINO_EVENT_WIFI_STA_GOT_IP6
#define ESPHOME_EVENT_ID_WIFI_STA_LOST_IP ARDUINO_EVENT_WIFI_STA_LOST_IP
#define ESPHOME_EVENT_ID_WIFI_AP_START ARDUINO_EVENT_WIFI_AP_START
#define ESPHOME_EVENT_ID_WIFI_AP_STOP ARDUINO_EVENT_WIFI_AP_STOP
#define ESPHOME_EVENT_ID_WIFI_AP_STACONNECTED ARDUINO_EVENT_WIFI_AP_STACONNECTED
#define ESPHOME_EVENT_ID_WIFI_AP_STADISCONNECTED ARDUINO_EVENT_WIFI_AP_STADISCONNECTED
#define ESPHOME_EVENT_ID_WIFI_AP_STAIPASSIGNED ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED
#define ESPHOME_EVENT_ID_WIFI_AP_PROBEREQRECVED ARDUINO_EVENT_WIFI_AP_PROBEREQRECVED
#define ESPHOME_EVENT_ID_WIFI_AP_GOT_IP6 ARDUINO_EVENT_WIFI_AP_GOT_IP6
using esphome_wifi_event_id_t = arduino_event_id_t;
using esphome_wifi_event_info_t = arduino_event_info_t;

// Event callback - runs in WiFi driver thread context
// Only queues events for processing in main loop, no logging or state changes here
void WiFiComponent::wifi_event_callback_(esphome_wifi_event_id_t event, esphome_wifi_event_info_t info) {
  if (s_event_queue == nullptr) {
    return;
  }

  // Allocate on heap and fill directly to avoid extra memcpy
  auto *to_send = new LTWiFiEvent{};  // NOLINT(cppcoreguidelines-owning-memory)
  to_send->event_id = event;

  // Copy event-specific data
  switch (event) {
    case ESPHOME_EVENT_ID_WIFI_STA_CONNECTED: {
      auto &it = info.wifi_sta_connected;
      to_send->data.sta_connected.ssid_len = it.ssid_len;
      memcpy(to_send->data.sta_connected.ssid, it.ssid,
             std::min(static_cast<size_t>(it.ssid_len), sizeof(to_send->data.sta_connected.ssid) - 1));
      memcpy(to_send->data.sta_connected.bssid, it.bssid, 6);
      to_send->data.sta_connected.channel = it.channel;
      to_send->data.sta_connected.authmode = it.authmode;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_DISCONNECTED: {
      auto &it = info.wifi_sta_disconnected;
      to_send->data.sta_disconnected.ssid_len = it.ssid_len;
      memcpy(to_send->data.sta_disconnected.ssid, it.ssid,
             std::min(static_cast<size_t>(it.ssid_len), sizeof(to_send->data.sta_disconnected.ssid) - 1));
      memcpy(to_send->data.sta_disconnected.bssid, it.bssid, 6);
      to_send->data.sta_disconnected.reason = it.reason;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_AUTHMODE_CHANGE: {
      auto &it = info.wifi_sta_authmode_change;
      to_send->data.sta_authmode_change.old_mode = it.old_mode;
      to_send->data.sta_authmode_change.new_mode = it.new_mode;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_SCAN_DONE: {
      auto &it = info.wifi_scan_done;
      to_send->data.scan_done.status = it.status;
      to_send->data.scan_done.number = it.number;
      to_send->data.scan_done.scan_id = it.scan_id;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_PROBEREQRECVED: {
      auto &it = info.wifi_ap_probereqrecved;
      memcpy(to_send->data.ap_probe_req.mac, it.mac, 6);
      to_send->data.ap_probe_req.rssi = it.rssi;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STACONNECTED: {
      auto &it = info.wifi_sta_connected;
      memcpy(to_send->data.sta_connected.bssid, it.bssid, 6);
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STADISCONNECTED: {
      auto &it = info.wifi_sta_disconnected;
      memcpy(to_send->data.sta_disconnected.bssid, it.bssid, 6);
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_READY:
    case ESPHOME_EVENT_ID_WIFI_STA_START:
    case ESPHOME_EVENT_ID_WIFI_STA_STOP:
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP:
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP6:
    case ESPHOME_EVENT_ID_WIFI_STA_LOST_IP:
    case ESPHOME_EVENT_ID_WIFI_AP_START:
    case ESPHOME_EVENT_ID_WIFI_AP_STOP:
    case ESPHOME_EVENT_ID_WIFI_AP_STAIPASSIGNED:
      // No additional data needed
      break;
    default:
      // Unknown event, don't queue
      delete to_send;  // NOLINT(cppcoreguidelines-owning-memory)
      return;
  }

  // Queue event (don't block if queue is full)
  if (xQueueSend(s_event_queue, &to_send, 0) != pdPASS) {
    delete to_send;  // NOLINT(cppcoreguidelines-owning-memory)
    s_event_queue_overflow_count++;
  }
}

// Process a single event from the queue - runs in main loop context
void WiFiComponent::wifi_process_event_(LTWiFiEvent *event) {
  switch (event->event_id) {
    case ESPHOME_EVENT_ID_WIFI_READY: {
      ESP_LOGV(TAG, "Ready");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_SCAN_DONE: {
      auto &it = event->data.scan_done;
      ESP_LOGV(TAG, "Scan done: status=%" PRIu32 " number=%u scan_id=%u", it.status, it.number, it.scan_id);
      this->wifi_scan_done_callback_();
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_START: {
      ESP_LOGV(TAG, "STA start");
      WiFi.setHostname(App.get_name().c_str());
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_STOP: {
      ESP_LOGV(TAG, "STA stop");
      s_sta_state = LTWiFiSTAState::IDLE;
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_CONNECTED: {
      auto &it = event->data.sta_connected;
      char bssid_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(it.bssid, bssid_buf);
      ESP_LOGV(TAG, "Connected ssid='%.*s' bssid=" LOG_SECRET("%s") " channel=%u, authmode=%s", it.ssid_len,
               (const char *) it.ssid, bssid_buf, it.channel, get_auth_mode_str(it.authmode));
      // Note: We don't set CONNECTED state here yet - wait for GOT_IP
      // This matches ESP32 IDF behavior where s_sta_connected is set but
      // wifi_sta_connect_status_() also checks got_ipv4_address_
#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
      for (auto *listener : this->connect_state_listeners_) {
        listener->on_wifi_connect_state(StringRef(it.ssid, it.ssid_len), it.bssid);
      }
#endif
      // For static IP configurations, GOT_IP event may not fire, so set connected state here
#ifdef USE_WIFI_MANUAL_IP
      if (const WiFiAP *config = this->get_selected_sta_(); config && config->get_manual_ip().has_value()) {
        s_sta_state = LTWiFiSTAState::CONNECTED;
#ifdef USE_WIFI_IP_STATE_LISTENERS
        for (auto *listener : this->ip_state_listeners_) {
          listener->on_ip_state(this->wifi_sta_ip_addresses(), this->get_dns_address(0), this->get_dns_address(1));
        }
#endif
      }
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_DISCONNECTED: {
      auto &it = event->data.sta_disconnected;

      // LibreTiny can send spurious disconnect events with empty ssid/bssid during connection.
      // These are typically "Association Leave" events that don't indicate actual failures:
      //   [W][wifi_lt]: Disconnected ssid='' bssid=00:00:00:00:00:00 reason='Association Leave'
      //   [W][wifi_lt]: Disconnected ssid='' bssid=00:00:00:00:00:00 reason='Association Leave'
      //   [V][wifi_lt]: Connected ssid='WIFI' bssid=... channel=3, authmode=WPA2 PSK
      // Without this check, the spurious events would transition state to ERROR_FAILED,
      // causing wifi_sta_connect_status_() to return an error. The main loop would then
      // call retry_connect(), aborting a connection that may succeed moments later.
      // Only ignore benign reasons - real failures like NO_AP_FOUND should still be processed.
      // However, if we get too many of these events (IGNORED_DISCONNECT_THRESHOLD), treat it
      // as a real connection failure to avoid waiting the full timeout for a failing connection.
      if (it.ssid_len == 0 && s_sta_state == LTWiFiSTAState::CONNECTING && it.reason != WIFI_REASON_NO_AP_FOUND) {
        s_ignored_disconnect_count++;
        if (s_ignored_disconnect_count >= IGNORED_DISCONNECT_THRESHOLD) {
          ESP_LOGW(TAG, "Too many disconnect events (%u) while connecting, treating as failure (reason=%s)",
                   s_ignored_disconnect_count, get_disconnect_reason_str(it.reason));
          s_sta_state = LTWiFiSTAState::ERROR_FAILED;
          WiFi.disconnect();
          this->error_from_callback_ = true;
          // Don't break - fall through to notify listeners
        } else {
          ESP_LOGV(TAG, "Ignoring disconnect event with empty ssid while connecting (reason=%s, count=%u)",
                   get_disconnect_reason_str(it.reason), s_ignored_disconnect_count);
          break;
        }
      }

      if (it.reason == WIFI_REASON_NO_AP_FOUND) {
        ESP_LOGW(TAG, "Disconnected ssid='%.*s' reason='Probe Request Unsuccessful'", it.ssid_len,
                 (const char *) it.ssid);
        s_sta_state = LTWiFiSTAState::ERROR_NOT_FOUND;
      } else {
        char bssid_s[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
        format_mac_addr_upper(it.bssid, bssid_s);
        ESP_LOGW(TAG, "Disconnected ssid='%.*s' bssid=" LOG_SECRET("%s") " reason='%s'", it.ssid_len,
                 (const char *) it.ssid, bssid_s, get_disconnect_reason_str(it.reason));
        s_sta_state = LTWiFiSTAState::ERROR_FAILED;
      }

      uint8_t reason = it.reason;
      if (reason == WIFI_REASON_AUTH_EXPIRE || reason == WIFI_REASON_BEACON_TIMEOUT ||
          reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_ASSOC_FAIL ||
          reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
        WiFi.disconnect();
        this->error_from_callback_ = true;
      }

#ifdef USE_WIFI_CONNECT_STATE_LISTENERS
      static constexpr uint8_t EMPTY_BSSID[6] = {};
      for (auto *listener : this->connect_state_listeners_) {
        listener->on_wifi_connect_state(StringRef(), EMPTY_BSSID);
      }
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_AUTHMODE_CHANGE: {
      auto &it = event->data.sta_authmode_change;
      ESP_LOGV(TAG, "Authmode Change old=%s new=%s", get_auth_mode_str(it.old_mode), get_auth_mode_str(it.new_mode));
      // Mitigate CVE-2020-12638
      // https://lbsfilm.at/blog/wpa2-authenticationmode-downgrade-in-espressif-microprocessors
      if (it.old_mode != WIFI_AUTH_OPEN && it.new_mode == WIFI_AUTH_OPEN) {
        ESP_LOGW(TAG, "Potential Authmode downgrade detected, disconnecting");
        WiFi.disconnect();
        this->error_from_callback_ = true;
        s_sta_state = LTWiFiSTAState::ERROR_FAILED;
      }
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP: {
      char ip_buf[network::IP_ADDRESS_BUFFER_SIZE], gw_buf[network::IP_ADDRESS_BUFFER_SIZE];
      ESP_LOGV(TAG, "static_ip=%s gateway=%s", network::IPAddress(WiFi.localIP()).str_to(ip_buf),
               network::IPAddress(WiFi.gatewayIP()).str_to(gw_buf));
      s_sta_state = LTWiFiSTAState::CONNECTED;
#ifdef USE_WIFI_IP_STATE_LISTENERS
      for (auto *listener : this->ip_state_listeners_) {
        listener->on_ip_state(this->wifi_sta_ip_addresses(), this->get_dns_address(0), this->get_dns_address(1));
      }
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_GOT_IP6: {
      ESP_LOGV(TAG, "Got IPv6");
#ifdef USE_WIFI_IP_STATE_LISTENERS
      for (auto *listener : this->ip_state_listeners_) {
        listener->on_ip_state(this->wifi_sta_ip_addresses(), this->get_dns_address(0), this->get_dns_address(1));
      }
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_STA_LOST_IP: {
      ESP_LOGV(TAG, "Lost IP");
      // Don't change state to IDLE - let the disconnect event handle that
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_START: {
      ESP_LOGV(TAG, "AP start");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STOP: {
      ESP_LOGV(TAG, "AP stop");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STACONNECTED: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      auto &it = event->data.sta_connected;
      char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(it.bssid, mac_buf);
      ESP_LOGV(TAG, "AP client connected MAC=%s", mac_buf);
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STADISCONNECTED: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      auto &it = event->data.sta_disconnected;
      char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(it.bssid, mac_buf);
      ESP_LOGV(TAG, "AP client disconnected MAC=%s", mac_buf);
#endif
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_STAIPASSIGNED: {
      ESP_LOGV(TAG, "AP client assigned IP");
      break;
    }
    case ESPHOME_EVENT_ID_WIFI_AP_PROBEREQRECVED: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
      auto &it = event->data.ap_probe_req;
      char mac_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(it.mac, mac_buf);
      ESP_LOGVV(TAG, "AP receive Probe Request MAC=%s RSSI=%d", mac_buf, it.rssi);
#endif
      break;
    }
    default:
      break;
  }
}
void WiFiComponent::wifi_pre_setup_() {
  // Create event queue for thread-safe event handling
  // Events are pushed from WiFi callback thread and processed in main loop
  s_event_queue = xQueueCreate(EVENT_QUEUE_SIZE, sizeof(LTWiFiEvent *));
  if (s_event_queue == nullptr) {
    ESP_LOGE(TAG, "Failed to create event queue");
    return;
  }

  auto f = std::bind(&WiFiComponent::wifi_event_callback_, this, std::placeholders::_1, std::placeholders::_2);
  WiFi.onEvent(f);
  // Make sure WiFi is in clean state before anything starts
  this->wifi_mode_(false, false);
}
WiFiSTAConnectStatus WiFiComponent::wifi_sta_connect_status_() {
  // Use state machine instead of querying WiFi.status() directly
  // State is updated in main loop from queued events, ensuring thread safety
  switch (s_sta_state) {
    case LTWiFiSTAState::CONNECTED:
      return WiFiSTAConnectStatus::CONNECTED;
    case LTWiFiSTAState::ERROR_NOT_FOUND:
      return WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND;
    case LTWiFiSTAState::ERROR_FAILED:
      return WiFiSTAConnectStatus::ERROR_CONNECT_FAILED;
    case LTWiFiSTAState::CONNECTING:
      return WiFiSTAConnectStatus::CONNECTING;
    case LTWiFiSTAState::IDLE:
    default:
      return WiFiSTAConnectStatus::IDLE;
  }
}
bool WiFiComponent::wifi_scan_start_(bool passive) {
  // enable STA
  if (!this->wifi_mode_(true, {}))
    return false;

  // Reset scan_done_ before starting new scan to prevent stale flag from previous scan
  // (e.g., roaming scan completed just before unexpected disconnect)
  this->scan_done_ = false;

  // need to use WiFi because of WiFiScanClass allocations :(
  int16_t err = WiFi.scanNetworks(true, true, passive, 200);
  if (err != WIFI_SCAN_RUNNING) {
    ESP_LOGV(TAG, "WiFi.scanNetworks failed: %d", err);
    return false;
  }

  return true;
}
void WiFiComponent::wifi_scan_done_callback_() {
  this->scan_result_.clear();
  this->scan_done_ = true;

  int16_t num = WiFi.scanComplete();
  if (num < 0)
    return;

  this->scan_result_.init(static_cast<unsigned int>(num));
  for (int i = 0; i < num; i++) {
    String ssid = WiFi.SSID(i);
    wifi_auth_mode_t authmode = WiFi.encryptionType(i);
    int32_t rssi = WiFi.RSSI(i);
    uint8_t *bssid = WiFi.BSSID(i);
    int32_t channel = WiFi.channel(i);

    this->scan_result_.emplace_back(bssid_t{bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]},
                                    std::string(ssid.c_str()), channel, rssi, authmode != WIFI_AUTH_OPEN,
                                    ssid.length() == 0);
  }
  WiFi.scanDelete();
#ifdef USE_WIFI_SCAN_RESULTS_LISTENERS
  for (auto *listener : this->scan_results_listeners_) {
    listener->on_wifi_scan_results(this->scan_result_);
  }
#endif
}

#ifdef USE_WIFI_AP
bool WiFiComponent::wifi_ap_ip_config_(const optional<ManualIP> &manual_ip) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

  if (manual_ip.has_value()) {
    return WiFi.softAPConfig(manual_ip->static_ip, manual_ip->gateway, manual_ip->subnet);
  } else {
    return WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  }
}

bool WiFiComponent::wifi_start_ap_(const WiFiAP &ap) {
  // enable AP
  if (!this->wifi_mode_({}, true))
    return false;

#ifdef USE_WIFI_MANUAL_IP
  if (!this->wifi_ap_ip_config_(ap.get_manual_ip())) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed");
    return false;
  }
#else
  if (!this->wifi_ap_ip_config_({})) {
    ESP_LOGV(TAG, "wifi_ap_ip_config_ failed");
    return false;
  }
#endif

  yield();

  return WiFi.softAP(ap.get_ssid().c_str(), ap.get_password().empty() ? NULL : ap.get_password().c_str(),
                     ap.has_channel() ? ap.get_channel() : 1, ap.get_hidden());
}

network::IPAddress WiFiComponent::wifi_soft_ap_ip() { return {WiFi.softAPIP()}; }
#endif  // USE_WIFI_AP

bool WiFiComponent::wifi_disconnect_() {
  // Reset state first so disconnect events aren't ignored
  // and wifi_sta_connect_status_() returns IDLE instead of CONNECTING
  s_sta_state = LTWiFiSTAState::IDLE;
  return WiFi.disconnect();
}

bssid_t WiFiComponent::wifi_bssid() {
  bssid_t bssid{};
  uint8_t *raw_bssid = WiFi.BSSID();
  if (raw_bssid != nullptr) {
    for (size_t i = 0; i < bssid.size(); i++)
      bssid[i] = raw_bssid[i];
  }
  return bssid;
}
std::string WiFiComponent::wifi_ssid() { return WiFi.SSID().c_str(); }
const char *WiFiComponent::wifi_ssid_to(std::span<char, SSID_BUFFER_SIZE> buffer) {
  // TODO: Find direct LibreTiny API to avoid Arduino String allocation
  String ssid = WiFi.SSID();
  size_t len = std::min(static_cast<size_t>(ssid.length()), SSID_BUFFER_SIZE - 1);
  memcpy(buffer.data(), ssid.c_str(), len);
  buffer[len] = '\0';
  return buffer.data();
}
int8_t WiFiComponent::wifi_rssi() { return WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : WIFI_RSSI_DISCONNECTED; }
int32_t WiFiComponent::get_wifi_channel() { return WiFi.channel(); }
network::IPAddress WiFiComponent::wifi_subnet_mask_() { return {WiFi.subnetMask()}; }
network::IPAddress WiFiComponent::wifi_gateway_ip_() { return {WiFi.gatewayIP()}; }
network::IPAddress WiFiComponent::wifi_dns_ip_(int num) { return {WiFi.dnsIP(num)}; }
void WiFiComponent::wifi_loop_() {
  // Process all pending events from the queue
  if (s_event_queue == nullptr) {
    return;
  }

  // Check for dropped events due to queue overflow
  if (s_event_queue_overflow_count > 0) {
    ESP_LOGW(TAG, "Event queue overflow, %" PRIu32 " events dropped", s_event_queue_overflow_count);
    s_event_queue_overflow_count = 0;
  }

  while (true) {
    LTWiFiEvent *event;
    if (xQueueReceive(s_event_queue, &event, 0) != pdTRUE) {
      // No more events
      break;
    }

    wifi_process_event_(event);
    delete event;  // NOLINT(cppcoreguidelines-owning-memory)
  }
}

}  // namespace esphome::wifi
#endif  // USE_LIBRETINY
#endif
