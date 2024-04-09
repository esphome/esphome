#include "wifi_component.h"
#include <cinttypes>

#if defined(USE_ESP32) || defined(USE_ESP_IDF)
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

namespace esphome {
namespace wifi {

static const char *const TAG = "wifi";

float WiFiComponent::get_setup_priority() const { return setup_priority::WIFI; }

void WiFiComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up WiFi...");
  this->wifi_pre_setup_();
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
  ESP_LOGCONFIG(TAG, "Starting WiFi...");
  ESP_LOGCONFIG(TAG, "  Local MAC: %s", get_mac_address_pretty().c_str());
  this->last_connected_ = millis();

  uint32_t hash = this->has_sta() ? fnv1_hash(App.get_compilation_time()) : 88491487UL;

  this->pref_ = global_preferences->make_preference<wifi::SavedWifiSettings>(hash, true);
  if (this->fast_connect_) {
    this->fast_connect_pref_ = global_preferences->make_preference<wifi::SavedWifiFastConnectSettings>(hash, false);
  }

  SavedWifiSettings save{};
  if (this->pref_.load(&save)) {
    ESP_LOGD(TAG, "Loaded saved wifi settings: %s", save.ssid);

    WiFiAP sta{};
    sta.set_ssid(save.ssid);
    sta.set_password(save.password);
    this->set_sta(sta);
  }

  if (this->has_sta()) {
    this->wifi_sta_pre_setup_();
    if (this->output_power_.has_value() && !this->wifi_apply_output_power_(*this->output_power_)) {
      ESP_LOGV(TAG, "Setting Output Power Option failed!");
    }

    if (!this->wifi_apply_power_save_()) {
      ESP_LOGV(TAG, "Setting Power Save Option failed!");
    }

    if (this->fast_connect_) {
      this->selected_ap_ = this->sta_[0];
      this->load_fast_connect_settings_();
      this->start_connecting(this->selected_ap_, false);
    } else {
      this->start_scanning();
    }
#ifdef USE_WIFI_AP
  } else if (this->has_ap()) {
    this->setup_ap_config_();
    if (this->output_power_.has_value() && !this->wifi_apply_output_power_(*this->output_power_)) {
      ESP_LOGV(TAG, "Setting Output Power Option failed!");
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

void WiFiComponent::loop() {
  this->wifi_loop_();
  const uint32_t now = millis();

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
          this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTING;
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
        ESP_LOGI(TAG, "Starting fallback AP!");
        this->setup_ap_config_();
#ifdef USE_CAPTIVE_PORTAL
        if (captive_portal::global_captive_portal != nullptr)
          captive_portal::global_captive_portal->start();
#endif
      }
    }
#endif  // USE_WIFI_AP

#ifdef USE_IMPROV
    if (esp32_improv::global_improv_component != nullptr && !esp32_improv::global_improv_component->is_active()) {
      if (now - this->last_connected_ > esp32_improv::global_improv_component->get_wifi_timeout()) {
        if (this->wifi_mode_(true, {}))
          esp32_improv::global_improv_component->start();
      }
    }

#endif

    if (!this->has_ap() && this->reboot_timeout_ != 0) {
      if (now - this->last_connected_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "Can't connect to WiFi, rebooting...");
        App.reboot();
      }
    }
  }
}

WiFiComponent::WiFiComponent() { global_wifi_component = this; }

bool WiFiComponent::has_ap() const { return this->has_ap_; }
bool WiFiComponent::has_sta() const { return !this->sta_.empty(); }
void WiFiComponent::set_fast_connect(bool fast_connect) { this->fast_connect_ = fast_connect; }
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
std::string WiFiComponent::get_use_address() const {
  if (this->use_address_.empty()) {
    return App.get_name() + ".local";
  }
  return this->use_address_;
}
void WiFiComponent::set_use_address(const std::string &use_address) { this->use_address_ = use_address; }

#ifdef USE_WIFI_AP
void WiFiComponent::setup_ap_config_() {
  this->wifi_mode_({}, true);

  if (this->ap_setup_)
    return;

  if (this->ap_.get_ssid().empty()) {
    std::string name = App.get_name();
    if (name.length() > 32) {
      if (App.is_name_add_mac_suffix_enabled()) {
        name.erase(name.begin() + 25, name.end() - 7);  // Remove characters between 25 and the mac address
      } else {
        name = name.substr(0, 32);
      }
    }
    this->ap_.set_ssid(name);
  }

  ESP_LOGCONFIG(TAG, "Setting up AP...");

  ESP_LOGCONFIG(TAG, "  AP SSID: '%s'", this->ap_.get_ssid().c_str());
  ESP_LOGCONFIG(TAG, "  AP Password: '%s'", this->ap_.get_password().c_str());
  if (this->ap_.get_manual_ip().has_value()) {
    auto manual = *this->ap_.get_manual_ip();
    ESP_LOGCONFIG(TAG, "  AP Static IP: '%s'", manual.static_ip.str().c_str());
    ESP_LOGCONFIG(TAG, "  AP Gateway: '%s'", manual.gateway.str().c_str());
    ESP_LOGCONFIG(TAG, "  AP Subnet: '%s'", manual.subnet.str().c_str());
  }

  this->ap_setup_ = this->wifi_start_ap_(this->ap_);
  ESP_LOGCONFIG(TAG, "  IP Address: %s", this->wifi_soft_ap_ip().str().c_str());

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

void WiFiComponent::add_sta(const WiFiAP &ap) { this->sta_.push_back(ap); }
void WiFiComponent::set_sta(const WiFiAP &ap) {
  this->clear_sta();
  this->add_sta(ap);
}
void WiFiComponent::clear_sta() { this->sta_.clear(); }
void WiFiComponent::save_wifi_sta(const std::string &ssid, const std::string &password) {
  SavedWifiSettings save{};
  strncpy(save.ssid, ssid.c_str(), sizeof(save.ssid));
  strncpy(save.password, password.c_str(), sizeof(save.password));
  this->pref_.save(&save);
  // ensure it's written immediately
  global_preferences->sync();

  WiFiAP sta{};
  sta.set_ssid(ssid);
  sta.set_password(password);
  this->set_sta(sta);
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

#ifdef USE_WIFI_WPA2_EAP
  if (ap.get_eap().has_value()) {
    ESP_LOGV(TAG, "  WPA2 Enterprise authentication configured:");
    EAPAuth eap_config = ap.get_eap().value();
    ESP_LOGV(TAG, "    Identity: " LOG_SECRET("'%s'"), eap_config.identity.c_str());
    ESP_LOGV(TAG, "    Username: " LOG_SECRET("'%s'"), eap_config.username.c_str());
    ESP_LOGV(TAG, "    Password: " LOG_SECRET("'%s'"), eap_config.password.c_str());
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
    ESP_LOGV(TAG, "  Channel: Not Set");
  }
  if (ap.get_manual_ip().has_value()) {
    ManualIP m = *ap.get_manual_ip();
    ESP_LOGV(TAG, "  Manual IP: Static IP=%s Gateway=%s Subnet=%s DNS1=%s DNS2=%s", m.static_ip.str().c_str(),
             m.gateway.str().c_str(), m.subnet.str().c_str(), m.dns1.str().c_str(), m.dns2.str().c_str());
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

const LogString *get_signal_bars(int8_t rssi) {
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

  ESP_LOGCONFIG(TAG, "  Local MAC: %s", get_mac_address_pretty().c_str());
  if (this->is_disabled()) {
    ESP_LOGCONFIG(TAG, "  WiFi is disabled!");
    return;
  }
  ESP_LOGCONFIG(TAG, "  SSID: " LOG_SECRET("'%s'"), wifi_ssid().c_str());
  for (auto &ip : wifi_sta_ip_addresses()) {
    if (ip.is_set()) {
      ESP_LOGCONFIG(TAG, "  IP Address: %s", ip.str().c_str());
    }
  }
  ESP_LOGCONFIG(TAG, "  BSSID: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X"), bssid[0], bssid[1], bssid[2], bssid[3],
                bssid[4], bssid[5]);
  ESP_LOGCONFIG(TAG, "  Hostname: '%s'", App.get_name().c_str());
  int8_t rssi = wifi_rssi();
  ESP_LOGCONFIG(TAG, "  Signal strength: %d dB %s", rssi, LOG_STR_ARG(get_signal_bars(rssi)));
  if (this->selected_ap_.get_bssid().has_value()) {
    ESP_LOGV(TAG, "  Priority: %.1f", this->get_sta_priority(*this->selected_ap_.get_bssid()));
  }
  ESP_LOGCONFIG(TAG, "  Channel: %" PRId32, wifi_channel_());
  ESP_LOGCONFIG(TAG, "  Subnet: %s", wifi_subnet_mask_().str().c_str());
  ESP_LOGCONFIG(TAG, "  Gateway: %s", wifi_gateway_ip_().str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS1: %s", wifi_dns_ip_(0).str().c_str());
  ESP_LOGCONFIG(TAG, "  DNS2: %s", wifi_dns_ip_(1).str().c_str());
#ifdef USE_WIFI_11KV_SUPPORT
  ESP_LOGCONFIG(TAG, "  BTM: %s", this->btm_ ? "enabled" : "disabled");
  ESP_LOGCONFIG(TAG, "  RRM: %s", this->rrm_ ? "enabled" : "disabled");
#endif
}

void WiFiComponent::enable() {
  if (this->state_ != WIFI_COMPONENT_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Enabling WIFI...");
  this->error_from_callback_ = false;
  this->state_ = WIFI_COMPONENT_STATE_OFF;
  this->start();
}

void WiFiComponent::disable() {
  if (this->state_ == WIFI_COMPONENT_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Disabling WIFI...");
  this->state_ = WIFI_COMPONENT_STATE_DISABLED;
  this->wifi_disconnect_();
  this->wifi_mode_(false, false);
}

bool WiFiComponent::is_disabled() { return this->state_ == WIFI_COMPONENT_STATE_DISABLED; }

void WiFiComponent::start_scanning() {
  this->action_started_ = millis();
  ESP_LOGD(TAG, "Starting scan...");
  this->wifi_scan_start_(this->passive_scan_);
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

    if (res.get_matches()) {
      ESP_LOGI(TAG, "- '%s' %s" LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(),
               res.get_is_hidden() ? "(HIDDEN) " : "", bssid_s, LOG_STR_ARG(get_signal_bars(res.get_rssi())));
      ESP_LOGD(TAG, "    Channel: %u", res.get_channel());
      ESP_LOGD(TAG, "    RSSI: %d dB", res.get_rssi());
    } else {
      ESP_LOGD(TAG, "- " LOG_SECRET("'%s'") " " LOG_SECRET("(%s) ") "%s", res.get_ssid().c_str(), bssid_s,
               LOG_STR_ARG(get_signal_bars(res.get_rssi())));
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
    // copy manual IP (if set)
    connect_params.set_manual_ip(config.get_manual_ip());

#ifdef USE_WIFI_WPA2_EAP
    // copy EAP parameters (if set)
    connect_params.set_eap(config.get_eap());
#endif

    // copy password (if set)
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
  auto status = this->wifi_sta_connect_status_();

  if (status == WiFiSTAConnectStatus::CONNECTED) {
    if (wifi_ssid().empty()) {
      ESP_LOGW(TAG, "Incomplete connection.");
      this->retry_connect();
      return;
    }

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
#ifdef USE_IMPROV
    if (this->is_esp32_improv_active_()) {
      esp32_improv::global_improv_component->stop();
    }
#endif

    this->state_ = WIFI_COMPONENT_STATE_STA_CONNECTED;
    this->num_retried_ = 0;

    if (this->fast_connect_) {
      this->save_fast_connect_settings_();
    }

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

  if (status == WiFiSTAConnectStatus::CONNECTING) {
    return;
  }

  if (status == WiFiSTAConnectStatus::ERROR_NETWORK_NOT_FOUND) {
    ESP_LOGW(TAG, "WiFi network can not be found anymore.");
    this->retry_connect();
    return;
  }

  if (status == WiFiSTAConnectStatus::ERROR_CONNECT_FAILED) {
    ESP_LOGW(TAG, "Connecting to WiFi network failed. Are the credentials wrong?");
    this->retry_connect();
    return;
  }

  ESP_LOGW(TAG, "WiFi Unknown connection status %d", (int) status);
  this->retry_connect();
}

void WiFiComponent::retry_connect() {
  if (this->selected_ap_.get_bssid()) {
    auto bssid = *this->selected_ap_.get_bssid();
    float priority = this->get_sta_priority(bssid);
    this->set_sta_priority(bssid, priority - 1.0f);
  }

  delay(10);
  if (!this->is_captive_portal_active_() && !this->is_esp32_improv_active_() &&
      (this->num_retried_ > 3 || this->error_from_callback_)) {
    if (this->num_retried_ > 5) {
      // If retry failed for more than 5 times, let's restart STA
      ESP_LOGW(TAG, "Restarting WiFi adapter...");
      this->wifi_mode_(false, {});
      delay(100);  // NOLINT
      this->num_retried_ = 0;
    } else {
      // Try hidden networks after 3 failed retries
      ESP_LOGD(TAG, "Retrying with hidden networks...");
      this->fast_connect_ = true;
      this->num_retried_++;
    }
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
  if (!this->has_sta() || this->state_ == WIFI_COMPONENT_STATE_DISABLED) {
    return true;
  }
  return this->is_connected();
}
void WiFiComponent::set_reboot_timeout(uint32_t reboot_timeout) { this->reboot_timeout_ = reboot_timeout; }
bool WiFiComponent::is_connected() {
  return this->state_ == WIFI_COMPONENT_STATE_STA_CONNECTED &&
         this->wifi_sta_connect_status_() == WiFiSTAConnectStatus::CONNECTED && !this->error_from_callback_;
}
void WiFiComponent::set_power_save_mode(WiFiPowerSaveMode power_save) { this->power_save_ = power_save; }

void WiFiComponent::set_passive_scan(bool passive) { this->passive_scan_ = passive; }

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
bool WiFiComponent::is_esp32_improv_active_() {
#ifdef USE_IMPROV
  return esp32_improv::global_improv_component != nullptr && esp32_improv::global_improv_component->is_active();
#else
  return false;
#endif
}

void WiFiComponent::load_fast_connect_settings_() {
  SavedWifiFastConnectSettings fast_connect_save{};

  if (this->fast_connect_pref_.load(&fast_connect_save)) {
    bssid_t bssid{};
    std::copy(fast_connect_save.bssid, fast_connect_save.bssid + 6, bssid.begin());
    this->selected_ap_.set_bssid(bssid);
    this->selected_ap_.set_channel(fast_connect_save.channel);

    ESP_LOGD(TAG, "Loaded saved fast_connect wifi settings");
  }
}

void WiFiComponent::save_fast_connect_settings_() {
  bssid_t bssid = wifi_bssid();
  uint8_t channel = wifi_channel_();

  if (bssid != this->selected_ap_.get_bssid() || channel != this->selected_ap_.get_channel()) {
    SavedWifiFastConnectSettings fast_connect_save{};

    memcpy(fast_connect_save.bssid, bssid.data(), 6);
    fast_connect_save.channel = channel;

    this->fast_connect_pref_.save(&fast_connect_save);

    ESP_LOGD(TAG, "Saved fast_connect wifi settings");
  }
}

void WiFiAP::set_ssid(const std::string &ssid) { this->ssid_ = ssid; }
void WiFiAP::set_bssid(bssid_t bssid) { this->bssid_ = bssid; }
void WiFiAP::set_bssid(optional<bssid_t> bssid) { this->bssid_ = bssid; }
void WiFiAP::set_password(const std::string &password) { this->password_ = password; }
#ifdef USE_WIFI_WPA2_EAP
void WiFiAP::set_eap(optional<EAPAuth> eap_auth) { this->eap_ = std::move(eap_auth); }
#endif
void WiFiAP::set_channel(optional<uint8_t> channel) { this->channel_ = channel; }
void WiFiAP::set_manual_ip(optional<ManualIP> manual_ip) { this->manual_ip_ = manual_ip; }
void WiFiAP::set_hidden(bool hidden) { this->hidden_ = hidden; }
const std::string &WiFiAP::get_ssid() const { return this->ssid_; }
const optional<bssid_t> &WiFiAP::get_bssid() const { return this->bssid_; }
const std::string &WiFiAP::get_password() const { return this->password_; }
#ifdef USE_WIFI_WPA2_EAP
const optional<EAPAuth> &WiFiAP::get_eap() const { return this->eap_; }
#endif
const optional<uint8_t> &WiFiAP::get_channel() const { return this->channel_; }
const optional<ManualIP> &WiFiAP::get_manual_ip() const { return this->manual_ip_; }
bool WiFiAP::get_hidden() const { return this->hidden_; }

WiFiScanResult::WiFiScanResult(const bssid_t &bssid, std::string ssid, uint8_t channel, int8_t rssi, bool with_auth,
                               bool is_hidden)
    : bssid_(bssid),
      ssid_(std::move(ssid)),
      channel_(channel),
      rssi_(rssi),
      with_auth_(with_auth),
      is_hidden_(is_hidden) {}
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

}  // namespace wifi
}  // namespace esphome
