#include "wireguard.h"

#ifdef USE_ESP32

#include <ctime>
#include <functional>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <esp_err.h>

#include <esp_wireguard.h>
#include <wireguard.h>  // REKEY_AFTER_TIME, from esp_wireguard library

namespace esphome {
namespace wireguard {

static const char *const TAG = "wireguard";

static const char *const LOGMSG_PEER_STATUS = "WireGuard remote peer is %s (latest handshake %s)";
static const char *const LOGMSG_ONLINE = "online";
static const char *const LOGMSG_OFFLINE = "offline";

void Wireguard::setup() {
  ESP_LOGD(TAG, "initializing WireGuard...");

  this->wg_config_.address = this->address_.c_str();
  this->wg_config_.private_key = this->private_key_.c_str();
  this->wg_config_.endpoint = this->peer_endpoint_.c_str();
  this->wg_config_.public_key = this->peer_public_key_.c_str();
  this->wg_config_.port = this->peer_port_;
  this->wg_config_.netmask = this->netmask_.c_str();
  this->wg_config_.persistent_keepalive = this->keepalive_;

  if (this->preshared_key_.length() > 0)
    this->wg_config_.preshared_key = this->preshared_key_.c_str();

  this->wg_initialized_ = esp_wireguard_init(&(this->wg_config_), &(this->wg_ctx_));

  if (this->wg_initialized_ == ESP_OK) {
    ESP_LOGI(TAG, "WireGuard initialized");
    this->wg_peer_offline_time_ = millis();
    this->srctime_->add_on_time_sync_callback(std::bind(&Wireguard::start_connection_, this));
    this->start_connection_();
  } else {
    ESP_LOGE(TAG, "cannot initialize WireGuard, error code %d", this->wg_initialized_);
    this->mark_failed();
  }
}

void Wireguard::update() {
  time_t lhs = this->get_latest_handshake();
  std::string latest_handshake =
      (lhs > 0) ? time::ESPTime::from_epoch_local(lhs).strftime("%Y-%m-%d %H:%M:%S %Z") : "timestamp not available";

  if (this->is_peer_up()) {
    if (this->wg_peer_offline_time_ != 0) {
      ESP_LOGI(TAG, LOGMSG_PEER_STATUS, LOGMSG_ONLINE, latest_handshake.c_str());
      this->wg_peer_offline_time_ = 0;
    } else {
      ESP_LOGD(TAG, LOGMSG_PEER_STATUS, LOGMSG_ONLINE, latest_handshake.c_str());
    }
  } else {
    if (this->wg_peer_offline_time_ == 0) {
      ESP_LOGW(TAG, LOGMSG_PEER_STATUS, LOGMSG_OFFLINE, latest_handshake.c_str());
      this->wg_peer_offline_time_ = millis();
    } else {
      ESP_LOGD(TAG, LOGMSG_PEER_STATUS, LOGMSG_OFFLINE, latest_handshake.c_str());
    }

    // check reboot timeout every time the peer is down
    if (this->reboot_timeout_ > 0) {
      if (millis() - this->wg_peer_offline_time_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "WireGuard remote peer is unreachable, rebooting...");
        App.reboot();
      }
    }
  }
}

void Wireguard::dump_config() {
  ESP_LOGCONFIG(TAG, "WireGuard:");
  ESP_LOGCONFIG(TAG, "  address: %s", this->address_.c_str());
  ESP_LOGCONFIG(TAG, "  netmask: %s", this->netmask_.c_str());
  ESP_LOGCONFIG(TAG, "  private key: %s[...]=", this->private_key_.substr(0, 5).c_str());
  ESP_LOGCONFIG(TAG, "  peer endpoint: %s", this->peer_endpoint_.c_str());
  ESP_LOGCONFIG(TAG, "  peer port: %d", this->peer_port_);
  ESP_LOGCONFIG(TAG, "  peer public key: %s", this->peer_public_key_.c_str());
  ESP_LOGCONFIG(TAG, "  peer preshared key: %s%s",
                (this->preshared_key_.length() > 0 ? this->preshared_key_.substr(0, 5).c_str() : "NOT IN USE"),
                (this->preshared_key_.length() > 0 ? "[...]=" : ""));
  ESP_LOGCONFIG(TAG, "  peer allowed ips:");
  for (auto &allowed_ip : this->allowed_ips_) {
    ESP_LOGCONFIG(TAG, "    - %s/%s", std::get<0>(allowed_ip).c_str(), std::get<1>(allowed_ip).c_str());
  }
  ESP_LOGCONFIG(TAG, "  peer persistent keepalive: %d%s", this->keepalive_,
                (this->keepalive_ > 0 ? "s" : " (DISABLED)"));
  ESP_LOGCONFIG(TAG, "  reboot timeout: %d%s", (this->reboot_timeout_ / 1000),
                (this->reboot_timeout_ != 0 ? "s" : " (DISABLED)"));
}

void Wireguard::on_shutdown() {
  if (this->wg_initialized_ == ESP_OK && this->wg_connected_ == ESP_OK) {
    ESP_LOGD(TAG, "stopping WireGuard connection...");
    esp_wireguard_disconnect(&(this->wg_ctx_));
    this->wg_connected_ = ESP_FAIL;
  }
}

bool Wireguard::is_peer_up() const {
  return (this->wg_initialized_ == ESP_OK) && (this->wg_connected_ == ESP_OK) &&
         (esp_wireguardif_peer_is_up(&(this->wg_ctx_)) == ESP_OK) &&
         (
             /*
              * When keepalive is disabled we can rely only on the underlying
              * library to check if the remote peer is up.
              */
             (this->keepalive_ == 0) ||
             /*
              * Otherwise we use the value 2*max(keepalive,REKEY_AFTER_TIME) as
              * the upper limit to consider a peer online (this value has been
              * arbitrarily chosen by authors of this component).
              */
             ((this->srctime_->utcnow().timestamp - this->get_latest_handshake()) <
              2 * (this->keepalive_ > REKEY_AFTER_TIME ? this->keepalive_ : REKEY_AFTER_TIME)));
}

time_t Wireguard::get_latest_handshake() const {
  time_t result;
  if (esp_wireguard_latest_handshake(&(this->wg_ctx_), &result) != ESP_OK) {
    result = 0;
  }
  return result;
}

void Wireguard::set_address(const std::string &address) { this->address_ = address; }
void Wireguard::set_netmask(const std::string &netmask) { this->netmask_ = netmask; }
void Wireguard::set_private_key(const std::string &key) { this->private_key_ = key; }
void Wireguard::set_peer_endpoint(const std::string &endpoint) { this->peer_endpoint_ = endpoint; }
void Wireguard::set_peer_public_key(const std::string &key) { this->peer_public_key_ = key; }
void Wireguard::set_peer_port(const uint16_t port) { this->peer_port_ = port; }
void Wireguard::set_preshared_key(const std::string &key) { this->preshared_key_ = key; }

void Wireguard::add_allowed_ip(const std::string &ip, const std::string &netmask) {
  this->allowed_ips_.emplace_back(ip, netmask);
}

void Wireguard::set_keepalive(const uint16_t seconds) { this->keepalive_ = seconds; }
void Wireguard::set_reboot_timeout(const uint32_t seconds) { this->reboot_timeout_ = seconds; }
void Wireguard::set_srctime(time::RealTimeClock *srctime) { this->srctime_ = srctime; }

void Wireguard::start_connection_() {
  if (this->wg_initialized_ != ESP_OK) {
    ESP_LOGE(TAG, "cannot start WireGuard, initialization in error with code %d", this->wg_initialized_);
    return;
  }

  if (!this->srctime_->now().is_valid()) {
    ESP_LOGD(TAG, "WireGuard is waiting for system time to be synchronized");
    return;
  }

  if (this->wg_connected_ == ESP_OK) {
    ESP_LOGD(TAG, "WireGuard connection already started");
    return;
  }

  ESP_LOGD(TAG, "starting WireGuard connection...");
  this->wg_connected_ = esp_wireguard_connect(&(this->wg_ctx_));
  if (this->wg_connected_ == ESP_OK) {
    ESP_LOGI(TAG, "WireGuard connection started");
  } else {
    ESP_LOGW(TAG, "cannot start WireGuard connection, error code %d", this->wg_connected_);
    return;
  }

  ESP_LOGD(TAG, "configuring WireGuard allowed ips list...");
  bool allowed_ips_ok = true;
  for (std::tuple<std::string, std::string> ip : this->allowed_ips_) {
    allowed_ips_ok &=
        (esp_wireguard_add_allowed_ip(&(this->wg_ctx_), std::get<0>(ip).c_str(), std::get<1>(ip).c_str()) == ESP_OK);
  }

  if (allowed_ips_ok) {
    ESP_LOGD(TAG, "allowed ips list configured correctly");
  } else {
    ESP_LOGE(TAG, "cannot configure WireGuard allowed ips list, aborting...");
    this->on_shutdown();
    this->mark_failed();
  }
}

}  // namespace wireguard
}  // namespace esphome

#endif

// vim: tabstop=2 shiftwidth=2 expandtab
