#include "wireguard.h"

#ifdef USE_ESP32

#include <ctime>
#include <functional>

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "esphome/core/time.h"
#include "esphome/components/network/util.h"

#include <esp_err.h>

#include <esp_wireguard.h>

// includes for resume/suspend wdt
#if defined(USE_ESP_IDF)
#include <esp_task_wdt.h>
#if ESP_IDF_VERSION_MAJOR >= 5
#include <spi_flash_mmap.h>
#endif
#elif defined(USE_ARDUINO)
#include <esp32-hal.h>
#endif

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
    this->defer(std::bind(&Wireguard::start_connection_, this));  // defer to avoid blocking setup
  } else {
    ESP_LOGE(TAG, "cannot initialize WireGuard, error code %d", this->wg_initialized_);
    this->mark_failed();
  }
}

void Wireguard::loop() {
  if ((this->wg_initialized_ == ESP_OK) && (this->wg_connected_ == ESP_OK) && (!network::is_connected())) {
    ESP_LOGV(TAG, "local network connection has been lost, stopping WireGuard...");
    this->stop_connection_();
  }
}

void Wireguard::update() {
  bool peer_up = this->is_peer_up();
  time_t lhs = this->get_latest_handshake();
  bool lhs_updated = (lhs > this->latest_saved_handshake_);

  ESP_LOGV(TAG, "handshake: latest=%.0f, saved=%.0f, updated=%d", (double) lhs, (double) this->latest_saved_handshake_,
           (int) lhs_updated);

  if (lhs_updated) {
    this->latest_saved_handshake_ = lhs;
  }

  std::string latest_handshake =
      (this->latest_saved_handshake_ > 0)
          ? ESPTime::from_epoch_local(this->latest_saved_handshake_).strftime("%Y-%m-%d %H:%M:%S %Z")
          : "timestamp not available";

  if (peer_up) {
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
      this->start_connection_();
    }

    // check reboot timeout every time the peer is down
    if (this->reboot_timeout_ > 0) {
      if (millis() - this->wg_peer_offline_time_ > this->reboot_timeout_) {
        ESP_LOGE(TAG, "WireGuard remote peer is unreachable, rebooting...");
        App.reboot();
      }
    }
  }

#ifdef USE_BINARY_SENSOR
  if (this->status_sensor_ != nullptr) {
    this->status_sensor_->publish_state(peer_up);
  }
#endif

#ifdef USE_SENSOR
  if (this->handshake_sensor_ != nullptr && lhs_updated) {
    this->handshake_sensor_->publish_state((double) this->latest_saved_handshake_);
  }
#endif
}

void Wireguard::dump_config() {
  ESP_LOGCONFIG(TAG, "WireGuard:");
  ESP_LOGCONFIG(TAG, "  Address: %s", this->address_.c_str());
  ESP_LOGCONFIG(TAG, "  Netmask: %s", this->netmask_.c_str());
  ESP_LOGCONFIG(TAG, "  Private Key: " LOG_SECRET("%s"), mask_key(this->private_key_).c_str());
  ESP_LOGCONFIG(TAG, "  Peer Endpoint: " LOG_SECRET("%s"), this->peer_endpoint_.c_str());
  ESP_LOGCONFIG(TAG, "  Peer Port: " LOG_SECRET("%d"), this->peer_port_);
  ESP_LOGCONFIG(TAG, "  Peer Public Key: " LOG_SECRET("%s"), this->peer_public_key_.c_str());
  ESP_LOGCONFIG(TAG, "  Peer Pre-shared Key: " LOG_SECRET("%s"),
                (this->preshared_key_.length() > 0 ? mask_key(this->preshared_key_).c_str() : "NOT IN USE"));
  ESP_LOGCONFIG(TAG, "  Peer Allowed IPs:");
  for (auto &allowed_ip : this->allowed_ips_) {
    ESP_LOGCONFIG(TAG, "    - %s/%s", std::get<0>(allowed_ip).c_str(), std::get<1>(allowed_ip).c_str());
  }
  ESP_LOGCONFIG(TAG, "  Peer Persistent Keepalive: %d%s", this->keepalive_,
                (this->keepalive_ > 0 ? "s" : " (DISABLED)"));
  ESP_LOGCONFIG(TAG, "  Reboot Timeout: %d%s", (this->reboot_timeout_ / 1000),
                (this->reboot_timeout_ != 0 ? "s" : " (DISABLED)"));
  // be careful: if proceed_allowed_ is true, require connection is false
  ESP_LOGCONFIG(TAG, "  Require Connection to Proceed: %s", (this->proceed_allowed_ ? "NO" : "YES"));
  LOG_UPDATE_INTERVAL(this);
}

void Wireguard::on_shutdown() { this->stop_connection_(); }

bool Wireguard::can_proceed() { return (this->proceed_allowed_ || this->is_peer_up()); }

bool Wireguard::is_peer_up() const {
  return (this->wg_initialized_ == ESP_OK) && (this->wg_connected_ == ESP_OK) &&
         (esp_wireguardif_peer_is_up(&(this->wg_ctx_)) == ESP_OK);
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

#ifdef USE_BINARY_SENSOR
void Wireguard::set_status_sensor(binary_sensor::BinarySensor *sensor) { this->status_sensor_ = sensor; }
#endif

#ifdef USE_SENSOR
void Wireguard::set_handshake_sensor(sensor::Sensor *sensor) { this->handshake_sensor_ = sensor; }
#endif

void Wireguard::disable_auto_proceed() { this->proceed_allowed_ = false; }

void Wireguard::start_connection_() {
  if (this->wg_initialized_ != ESP_OK) {
    ESP_LOGE(TAG, "cannot start WireGuard, initialization in error with code %d", this->wg_initialized_);
    return;
  }

  if (!network::is_connected()) {
    ESP_LOGD(TAG, "WireGuard is waiting for local network connection to be available");
    return;
  }

  if (!this->srctime_->now().is_valid()) {
    ESP_LOGD(TAG, "WireGuard is waiting for system time to be synchronized");
    return;
  }

  if (this->wg_connected_ == ESP_OK) {
    ESP_LOGV(TAG, "WireGuard connection already started");
    return;
  }

  ESP_LOGD(TAG, "starting WireGuard connection...");

  /*
   * The function esp_wireguard_connect() contains a DNS resolution
   * that could trigger the watchdog, so before it we suspend (or
   * increase the time, it depends on the platform) the wdt and
   * then we resume the normal timeout.
   */
  suspend_wdt();
  ESP_LOGV(TAG, "executing esp_wireguard_connect");
  this->wg_connected_ = esp_wireguard_connect(&(this->wg_ctx_));
  resume_wdt();

  if (this->wg_connected_ == ESP_OK) {
    ESP_LOGI(TAG, "WireGuard connection started");
  } else {
    ESP_LOGW(TAG, "cannot start WireGuard connection, error code %d", this->wg_connected_);
    return;
  }

  ESP_LOGD(TAG, "configuring WireGuard allowed IPs list...");
  bool allowed_ips_ok = true;
  for (std::tuple<std::string, std::string> ip : this->allowed_ips_) {
    allowed_ips_ok &=
        (esp_wireguard_add_allowed_ip(&(this->wg_ctx_), std::get<0>(ip).c_str(), std::get<1>(ip).c_str()) == ESP_OK);
  }

  if (allowed_ips_ok) {
    ESP_LOGD(TAG, "allowed IPs list configured correctly");
  } else {
    ESP_LOGE(TAG, "cannot configure WireGuard allowed IPs list, aborting...");
    this->stop_connection_();
    this->mark_failed();
  }
}

void Wireguard::stop_connection_() {
  if (this->wg_initialized_ == ESP_OK && this->wg_connected_ == ESP_OK) {
    ESP_LOGD(TAG, "stopping WireGuard connection...");
    esp_wireguard_disconnect(&(this->wg_ctx_));
    this->wg_connected_ = ESP_FAIL;
  }
}

void suspend_wdt() {
#if defined(USE_ESP_IDF)
#if ESP_IDF_VERSION_MAJOR >= 5
  ESP_LOGV(TAG, "temporarily increasing wdt timeout to 15000 ms");
  esp_task_wdt_config_t wdtc;
  wdtc.timeout_ms = 15000;
  wdtc.idle_core_mask = 0;
  wdtc.trigger_panic = false;
  esp_task_wdt_reconfigure(&wdtc);
#else
  ESP_LOGV(TAG, "temporarily increasing wdt timeout to 15 seconds");
  esp_task_wdt_init(15, false);
#endif
#elif defined(USE_ARDUINO)
  ESP_LOGV(TAG, "temporarily disabling the wdt");
  disableLoopWDT();
#endif
}

void resume_wdt() {
#if defined(USE_ESP_IDF)
#if ESP_IDF_VERSION_MAJOR >= 5
  wdtc.timeout_ms = CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000;
  esp_task_wdt_reconfigure(&wdtc);
  ESP_LOGV(TAG, "wdt resumed with %d ms timeout", wdtc.timeout_ms);
#else
  esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, false);
  ESP_LOGV(TAG, "wdt resumed with %d seconds timeout", CONFIG_ESP_TASK_WDT_TIMEOUT_S);
#endif
#elif defined(USE_ARDUINO)
  enableLoopWDT();
  ESP_LOGV(TAG, "wdt resumed");
#endif
}

std::string mask_key(const std::string &key) { return (key.substr(0, 5) + "[...]="); }

}  // namespace wireguard
}  // namespace esphome

#endif  // USE_ESP32
