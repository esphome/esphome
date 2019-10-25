#include "wifi_component.h"

#ifdef ARDUINO_ARCH_ESP32
#include <esp_wifi.h>
#endif
#ifdef ARDUINO_ARCH_ESP8266
#include <user_interface.h>
#endif

#include <utility>
#include <algorithm>
#include "lwip/err.h"
#include "lwip/dns.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/esphal.h"
#include "esphome/core/util.h"
#include "esphome/core/application.h"

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome {
namespace wifi {

static const char *TAG = "wifi";

float WiFiComponent::get_setup_priority() const { return setup_priority::WIFI; }

void WiFiComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up WiFi...");
  this->last_connected_ = millis();
  this->wifi_pre_setup_();

  if (this->has_sta()) {
    this->wifi_sta_pre_setup_();

    if (!this->wifi_apply_power_save_()) {
      ESP_LOGV(TAG, "Setting Power Save Option failed!");
    }

    if (this->fast_connect_) {
      this->selected_ap_ = this->sta_[0];
      this->start_connecting(this->selected_ap_, false);
    } else {
      this->start_scanning();
    }
  } else if (this->has_ap()) {
    this->setup_ap_config_();
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr)
      captive_portal::global_captive_portal->start();
#endif
  }

  this->wifi_apply_hostname_();
#ifdef ARDUINO_ARCH_ESP32
  network_setup_mdns();
#endif
}

void WiFiComponent::loop() {
  const uint32_t now = millis();

  if (this->has_sta()) {
    switch (this->state_) {
      case WIFI_COMPONENT_STATE_COOLDOWN: {
        this->status_set_warning();
        if (millis() - this->action_started_ > 5000) {
          if (this->fast_connect_) {
            this->start_connecting(this->sta_[0], false);
          } else {
            this->start_scanning();
          }
        }
        break;
      }
      case WIFI_COMPONENT_STATE_STA_SCANNING: {
        this->status_set_warning();
        this->check_scanning_finished();
        break;
      }
      case WIFI_COMPONENT_STATE_STA_CONNECTING:
      case WIFI_COMPONENT_STATE_STA_CONNECTING_2: {
        this->status_set_warning();
        this->check_connecting_finished();
        break;
      }

      case WIFI_COMPONENT_STATE_STA_CONNECTED: {
        if (!this->is_connected()) {
          ESP_LOGW(TAG, "WiFi Connection lost... Reconnecting...");
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
    }

    if (this->has_ap() && !this->ap_setup_) {
      if (now - this->last_connected_ > this->ap_timeout_) {
        ESP_LOGI(TAG, "Starting fallback AP!");
        this->setup_ap_config_();
#ifdef USE_CAPTIVE_PORTAL
        if (captive_portal::global_captive_portal != nullptr)
          captive_portal::global_captive_portal->start();
#endif
      }
    }

    if (!this->has_ap() && this->reboot_timeout_ != 0) {
      if (now - this->last_connected_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "Can't connect to WiFi, rebooting...");
        App.reboot();
      }
    }
  }

  network_tick_mdns();
}

WiFiComponent::WiFiComponent() { global_wifi_component = this; }

bool WiFiComponent::has_ap() const { return !this->ap_.get_ssid().empty(); }
bool WiFiComponent::has_sta() const { return !this->sta_.empty(); }
void WiFiComponent::set_fast_connect(bool fast_connect) { this->fast_connect_ = fast_connect; }
IPAddress WiFiComponent::get_ip_address() {
  if (this->has_sta())
    return this->wifi_sta_ip_();
  if (this->has_ap())
    return this->wifi_soft_ap_ip();
  return {};
}
std::string WiFiComponent::get_use_address() const {
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}
void WiFiComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }
void WiFiComponent::setup_ap_config_() {
  this->wifi_mode_({}, true);

  if (this->ap_setup_)
    return;

  ESP_LOGCONFIG(TAG, "Setting up AP...");

  ESP_LOGCONFIG(TAG, "  AP SSID: '%s'", this->ap_.get_ssid().c_str());
  ESP_LOGCONFIG(TAG, "  AP Password: '%s'", this->ap_.get_password().c_str());
  if (this->ap_.get_manual_ip().has_value()) {
    auto manual = *this->ap_.get_manual_ip();
    ESP_LOGCONFIG(TAG, "  AP Static IP: '%s'", manual.static_ip.toString().c_str());
    ESP_LOGCONFIG(TAG, "  AP Gateway: '%s'", manual.gateway.toString().c_str());
    ESP_LOGCONFIG(TAG, "  AP Subnet: '%s'", manual.subnet.toString().c_str());
  }

  this->ap_setup_ = this->wifi_start_ap_(this->ap_);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", this->wifi_soft_ap_ip().toString().c_str());
#ifdef ARDUINO_ARCH_ESP8266
  network_setup_mdns(this->wifi_soft_ap_ip(), 1);
#endif

  if (!this->has_sta()) {
    this->state_ = WIFI_COMPONENT_STATE_AP;
  }
}

float WiFiComponent::get_loop_priority() const {
  return 10.0f;  // before other loop components
}
void WiFiComponent::set_ap(const WiFiAP &ap) { this->ap_ = ap; }
void WiFiComponent::add_sta(const WiFiAP &ap) { this->sta_.push_back(ap); }
void WiFiComponent::set_sta(const WiFiAP &ap) {
  this->sta_.clear();
  this->add_sta(ap);
}

void WiFiComponent::start_connecting(const WiFiAP &ap, bool two) {
  ESP_LOGI(TAG, "WiFi Connecting to '%s'...", ap.get_ssid().c_str());
#ifdef ESPHOME_LOG_HAS_VERBOSE
  ESP_LOGV(TAG, "Connection Params:");
  ESP_LOGV(TAG, "  SSID: '%s'", ap.get_ssid().c_str());
  if (ap.get_bssid().has_value()) {
    bssid_t b = *ap.get_bssid();
    ESP_LOGV(TAG, "  BSSID: %02X:%02X:%02X:%02X:%02X:%02X", b[0], b[1], b[2], b[3], b[4], b[5]);
  } else {
    ESP_LOGV(TAG, "  BSSID: Not Set");
  }
  ESP_LOGV(TAG, "  Password: " LOG_SECRET("'%s'"), ap.get_password().c_str());
  if (ap.get_channel().has_value()) {
    ESP_LOGV(TAG, "  Channel: %u", *ap.get_channel());
  } else {
    ESP_LOGV(TAG, "  Channel: Not Set");
  }
  if (ap.get_manual_ip().has_value()) {
    ManualIP m = *ap.get_manual_ip();
    ESP_LOGV(TAG, "  Manual IP: Static IP=%s Gateway=%s Subnet=%s DNS1=%s DNS2=%s", m.static_ip.toString().c_str(),
             m.gateway.toString().c_str(), m.subnet.toString().c_str(), m.dns1.toString().c_str(),
             m.dns2.toString().c_str());
  } else {
    ESP_LOGV(TAG, "  Using DHCP IP");
  }
  ESP_LOGV(TAG, "  Hidden: %s", YESNO(ap.get_hidden()));
#endif

  if (!this->wifi_sta_connect_(ap)) {
    ESP_LOGE(TAG, "wifi_sta_connect_ failed!");
    this->retry_connect();
    return;
  }

  if (!two) {
    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING;
  } else {
    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING_2;
  }
  this->action_started_ = millis();
}

void print_signal_bars(int8_t rssi, char *buf) {
  // LOWER ONE QUARTER BLOCK
  // Unicode: U+2582, UTF-8: E2 96 82
  // LOWER HALF BLOCK
  // Unicode: U+2584, UTF-8: E2 96 84
  // LOWER THREE QUARTERS BLOCK
  // Unicode: U+2586, UTF-8: E2 96 86
  // FULL BLOCK
  // Unicode: U+2588, UTF-8: E2 96 88
  if (rssi >= -50) {
    sprintf(buf, "\033[0;32m"  // green
                 "\xe2\x96\x82"
                 "\xe2\x96\x84"
                 "\xe2\x96\x86"
                 "\xe2\x96\x88"
                 "\033[0m");
  } else if (rssi >= -65) {
    sprintf(buf, "\033[0;33m"  // yellow
                 "\xe2\x96\x82"
                 "\xe2\x96\x84"
                 "\xe2\x96\x86"
                 "\033[0;37m"
                 "\xe2\x96\x88"
                 "\033[0m");
  } else if (rssi >= -85) {
    sprintf(buf, "\033[0;33m"  // yellow
                 "\xe2\x96\x82"
                 "\xe2\x96\x84"
                 "\033[0;37m"
                 "\xe2\x96\x86"
                 "\xe2\x96\x88"
                 "\033[0m");
  } else {
    sprintf(buf, "\033[0;31m"  // red
                 "\xe2\x96\x82"
                 "\033[0;37m"
                 "\xe2\x96\x84"
                 "\xe2\x96\x86"
                 "\xe2\x96\x88"
                 "\033[0m");
  }
}

void WiFiComponent::print_connect_params_() {
  uint8_t bssid[6] = {};
  uint8_t *raw_bssid = WiFi.BSSID();
  if (raw_bssid != nullptr)
    memcpy(bssid, raw_bssid, sizeof(bssid));

  ESP_LOGCONFIG(TAG, "  SSID: " LOG_SECRET("'%s'"), WiFi.SSID().c_str());
  ESP_LOGCONFIG(TAG, "  IP Address: %s", WiFi.localIP().toString().c_str());
  ESP_LOGCONFIG(TAG, "  BSSID: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X"), bssid[0], bssid[1], bssid[2], bssid[3],
                bssid[4], bssid[5]);
  ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
  char signal_bars[50];
  int8_t rssi = WiFi.RSSI();
  print_signal_bars(rssi, signal_bars);
  ESP_LOGCONFIG(TAG, "  Signal strength: %d dB %s", rssi, signal_bars);
  if (this->selected_ap_.get_bssid().has_value()) {
    ESP_LOGV(TAG, "  Priority: %.1f", this->get_sta_priority(*this->selected_ap_.get_bssid()));
  }
  ESP_LOGCONFIG(TAG, "  Channel: %d", WiFi.channel());
  ESP_LOGCONFIG(TAG, "  Subnet: %s", WiFi.subnetMask().toString().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", WiFi.gatewayIP().toString().c_str());
  ESP_LOGCONFIG(TAG, "  DNS1: %s", WiFi.dnsIP(0).toString().c_str());
  ESP_LOGCONFIG(TAG, "  DNS2: %s", WiFi.dnsIP(1).toString().c_str());
}

void WiFiComponent::start_scanning() {
  this->action_started_ = millis();
  ESP_LOGD(TAG, "Starting scan...");
  this->wifi_scan_start_();
  this->state_ = WIFI_COMPONENT_STATE_STA_SCANNING;
}

void WiFiComponent::check_scanning_finished() {
  if (!this->scan_done_) {
    if (millis() - this->action_started_ > 30000) {
      ESP_LOGE(TAG, "Scan timeout!");
      this->retry_connect();
    }
    return;
  }
  this->scan_done_ = false;

  ESP_LOGD(TAG, "Found networks:");
  if (this->scan_result_.empty()) {
    ESP_LOGD(TAG, "  No network found!");
    this->retry_connect();
    return;
  }

  for (auto &res : this->scan_result_) {
    for (auto &ap : this->sta_) {
      if (res.matches(ap)) {
        res.set_matches(true);
        if (!this->has_sta_priority(res.get_bssid())) {
          this->set_sta_priority(res.get_bssid(), ap.get_priority());
        }
        res.set_priority(this->get_sta_priority(res.get_bssid()));
        break;
      }
    }
  }

  std::stable_sort(this->scan_result_.begin(), this->scan_result_.end(),
                   [](const WiFiScanResult &a, const WiFiScanResult &b) {
                     // return true if a is better than b
                     if (a.get_matches() && !b.get_matches())
                       return true;
                     if (!a.get_matches() && b.get_matches())
                       return false;

                     if (a.get_matches() && b.get_matches()) {
                       // if both match, check priority
                       if (a.get_priority() != b.get_priority())
                         return a.get_priority() > b.get_priority();
                     }

                     return a.get_rssi() > b.get_rssi();
                   });

  for (auto &res : this->scan_result_) {
    char bssid_s[18];
    auto bssid = res.get_bssid();
    sprintf(bssid_s, "%02X:%02X:%02X:%02X:%02X:%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
    char signal_bars[50];
    print_signal_bars(res.get_rssi(), signal_bars);

    if (res.get_matches()) {
      ESP_LOGI(TAG, "- '%s' %s" LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(),
               res.get_is_hidden() ? "(HIDDEN) " : "", bssid_s, signal_bars);
      ESP_LOGD(TAG, "    Channel: %u", res.get_channel());
      ESP_LOGD(TAG, "    RSSI: %d dB", res.get_rssi());
    } else {
      ESP_LOGD(TAG, "- " LOG_SECRET("'%s'") " " LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(), bssid_s, signal_bars);
    }
  }

  if (!this->scan_result_[0].get_matches()) {
    ESP_LOGW(TAG, "No matching network found!");
    this->retry_connect();
    return;
  }

  WiFiAP connect_params;
  WiFiScanResult scan_res = this->scan_result_[0];
  for (auto &config : this->sta_) {
    // search for matching STA config, at least one will match (from checks before)
    if (!scan_res.matches(config)) {
      continue;
    }

    if (config.get_hidden()) {
      // selected network is hidden, we use the data from the config
      connect_params.set_hidden(true);
      connect_params.set_ssid(config.get_ssid());
      // don't set BSSID and channel, there might be multiple hidden networks
      // but we can't know which one is the correct one. Rely on probe-req with just SSID.
    } else {
      // selected network is visible, we use the data from the scan
      // limit the connect params to only connect to exactly this network
      // (network selection is done during scan phase).
      connect_params.set_hidden(false);
      connect_params.set_ssid(scan_res.get_ssid());
      connect_params.set_channel(scan_res.get_channel());
      connect_params.set_bssid(scan_res.get_bssid());
    }
    // set manual IP+password (if any)
    connect_params.set_manual_ip(config.get_manual_ip());
    connect_params.set_password(config.get_password());
    break;
  }

  yield();

  this->selected_ap_ = connect_params;
  this->start_connecting(connect_params, false);
}

void WiFiComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WiFi:");
  this->print_connect_params_();
}

void WiFiComponent::check_connecting_finished() {
  wl_status_t status = this->wifi_sta_status_();

  if (status == WL_CONNECTED) {
    ESP_LOGI(TAG, "WiFi Connected!");
    this->print_connect_params_();

    if (this->has_ap()) {
#ifdef USE_CAPTIVE_PORTAL
      if (this->is_captive_portal_active_()) {
        captive_portal::global_captive_portal->end();
      }
#endif
      ESP_LOGD(TAG, "Disabling AP...");
      this->wifi_mode_({}, false);
    }
#ifdef ARDUINO_ARCH_ESP8266
    network_setup_mdns(this->wifi_sta_ip_(), 0);
#endif
    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTED;
    this->num_retried_ = 0;
    return;
  }

  uint32_t now = millis();
  if (now - this->action_started_ > 30000) {
    ESP_LOGW(TAG, "Timeout while connecting to WiFi.");
    this->retry_connect();
    return;
  }

  if (this->error_from_callback_) {
    ESP_LOGW(TAG, "Error while connecting to network.");
    this->retry_connect();
    return;
  }

  if (status == WL_IDLE_STATUS || status == WL_DISCONNECTED || status == WL_CONNECTION_LOST) {
    // WL_DISCONNECTED is set while not connected yet.
    // WL_IDLE_STATUS is set while we're waiting for the IP address.
    // WL_CONNECTION_LOST happens on the ESP32
    return;
  }

  if (status == WL_NO_SSID_AVAIL) {
    ESP_LOGW(TAG, "WiFi network can not be found anymore.");
    this->retry_connect();
    return;
  }

  if (status == WL_CONNECT_FAILED) {
    ESP_LOGW(TAG, "Connecting to WiFi network failed. Are the credentials wrong?");
    this->retry_connect();
    return;
  }

  ESP_LOGW(TAG, "WiFi Unknown connection status %d", status);
}

void WiFiComponent::retry_connect() {
  if (this->selected_ap_.get_bssid()) {
    auto bssid = *this->selected_ap_.get_bssid();
    float priority = this->get_sta_priority(bssid);
    this->set_sta_priority(bssid, priority - 1.0f);
  }

  delay(10);
  if (!this->is_captive_portal_active_() && (this->num_retried_ > 5 || this->error_from_callback_)) {
    // If retry failed for more than 5 times, let's restart STA
    ESP_LOGW(TAG, "Restarting WiFi adapter...");
    this->wifi_mode_(false, {});
    delay(100);  // NOLINT
    this->num_retried_ = 0;
  } else {
    this->num_retried_++;
  }
  this->error_from_callback_ = false;
  if (this->state_ == WIFI_COMPONENT_STATE_STA_CONNECTING) {
    yield();
    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING_2;
    this->start_connecting(this->selected_ap_, true);
    return;
  }

  this->state_ = WIFI_COMPONENT_STATE_COOLDOWN;
  this->action_started_ = millis();
}

bool WiFiComponent::can_proceed() {
  if (this->has_ap() && !this->has_sta()) {
    return true;
  }
  return this->is_connected();
}
void WiFiComponent::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }
bool WiFiComponent::is_connected() {
  return this->state_ == WIFI_COMPONENT_STATE_STA_CONNECTED && this->wifi_sta_status_() == WL_CONNECTED &&
         !this->error_from_callback_;
}
void WiFiComponent::set_power_save_mode(WiFiPowerSaveMode power_save) { this->power_save_ = power_save; }

std::string WiFiComponent::format_mac_addr(const uint8_t *mac) {
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buf;
}
bool WiFiComponent::is_captive_portal_active_() {
#ifdef USE_CAPTIVE_PORTAL
  return captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active();
#else
  return false;
#endif
}

void WiFiAP::set_ssid(const std::string &ssid) { this->ssid_ = ssid; }
void WiFiAP::set_bssid(bssid_t bssid) { this->bssid_ = bssid; }
void WiFiAP::set_bssid(optional<bssid_t> bssid) { this->bssid_ = bssid; }
void WiFiAP::set_password(const std::string &password) { this->password_ = password; }
void WiFiAP::set_channel(optional<uint8_t> channel) { this->channel_ = channel; }
void WiFiAP::set_manual_ip(optional<ManualIP> manual_ip) { this->manual_ip_ = manual_ip; }
void WiFiAP::set_hidden(bool hidden) { this->hidden_ = hidden; }
const std::string &WiFiAP::get_ssid() const { return this->ssid_; }
const optional<bssid_t> &WiFiAP::get_bssid() const { return this->bssid_; }
const std::string &WiFiAP::get_password() const { return this->password_; }
const optional<uint8_t> &WiFiAP::get_channel() const { return this->channel_; }
const optional<ManualIP> &WiFiAP::get_manual_ip() const { return this->manual_ip_; }
bool WiFiAP::get_hidden() const { return this->hidden_; }

WiFiScanResult::WiFiScanResult(const bssid_t &bssid, const std::string &ssid, uint8_t channel, int8_t rssi,
                               bool with_auth, bool is_hidden)
    : bssid_(bssid), ssid_(ssid), channel_(channel), rssi_(rssi), with_auth_(with_auth), is_hidden_(is_hidden) {}
bool WiFiScanResult::matches(const WiFiAP &config) {
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
  // If PW given, only match for networks with auth (and vice versa)
  if (config.get_password().empty() == this->with_auth_)
    return false;
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

WiFiComponent *global_wifi_component;

}  // namespace wifi
}  // namespace esphome
