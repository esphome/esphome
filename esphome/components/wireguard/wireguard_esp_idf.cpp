#ifdef USE_ESP_IDF

#include <functional>
#include <ctime>
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_wireguard.h"
#include "wireguard.h"


namespace esphome {
namespace wireguard {

static const char * const TAG = "wireguard";
static char WG_TMP_BUFFER[34];

void Wireguard::setup() {
    ESP_LOGD(TAG, "initializing...");

    wg_config.allowed_ip = &address_[0];
    wg_config.private_key = &private_key_[0];
    wg_config.endpoint = &peer_endpoint_[0];
    wg_config.public_key = &peer_key_[0];
    wg_config.port = peer_port_;
    wg_config.allowed_ip_mask = &netmask_[0];
    wg_config.persistent_keepalive = keepalive_;

    if(preshared_key_.length() > 0)
        wg_config.preshared_key = &preshared_key_[0];

    wg_initialized = esp_wireguard_init(&wg_config, &wg_ctx);
    
    if(wg_initialized == ESP_OK) {
        ESP_LOGI(TAG, "initialized");
        srctime_->add_on_time_sync_callback(std::bind(&Wireguard::start_connection, this));
        start_connection();
    } else {
        ESP_LOGE(TAG, "cannot initialize, error code %d", wg_initialized);
        this->mark_failed();
    }
}

void Wireguard::update() {
    if(wg_initialized == ESP_OK && wg_connected == ESP_OK) {
        wg_peer_up = (esp_wireguardif_peer_is_up(&wg_ctx) == ESP_OK);
    } else {
        ESP_LOGD(TAG, "initialized: %s (error %d)", (wg_initialized == ESP_OK ? "yes" : "no"), wg_initialized);
        ESP_LOGD(TAG, "connection: %s (error %d)", (wg_connected == ESP_OK ? "active" : "inactive"), wg_connected);
        wg_peer_up = false;
    }

    std::strftime(WG_TMP_BUFFER, sizeof(WG_TMP_BUFFER), "offline since %Y-%m-%d %H:%M:%S", localtime(&wg_last_peer_up));
    ESP_LOGD(TAG, "peer: %s", (wg_peer_up ? "online" : WG_TMP_BUFFER));

    if(wg_peer_up)
        wg_last_peer_up = srctime_->now().timestamp;
}

void Wireguard::dump_config() {
    ESP_LOGCONFIG(TAG, "Configuration");
    ESP_LOGCONFIG(TAG, "  address: %s",this->address_.data());
    ESP_LOGCONFIG(TAG, "  netmask: %s",this->netmask_.data());
    ESP_LOGCONFIG(TAG, "  private key: %s...=",this->private_key_.substr(0,5).data());
    ESP_LOGCONFIG(TAG, "  endpoint: %s",this->peer_endpoint_.data());
    ESP_LOGCONFIG(TAG, "  peer key: %s",this->peer_key_.data());
    ESP_LOGCONFIG(TAG, "  peer port: %d",this->peer_port_);
    ESP_LOGCONFIG(TAG, "  preshared key: %s...=",this->preshared_key_.substr(0,5).data());
    ESP_LOGCONFIG(TAG, "  keepalive: %d",this->keepalive_);
}

void Wireguard::on_shutdown() {
    if(wg_initialized == ESP_OK && wg_connected == ESP_OK) {
        ESP_LOGD(TAG, "disconnecting...");
        esp_wireguard_disconnect(&wg_ctx);
    }
}

void Wireguard::set_address(std::string address) { this->address_ = std::move(address); }
void Wireguard::set_netmask(std::string netmask) { this->netmask_ = std::move(netmask); }
void Wireguard::set_private_key(std::string key) { this->private_key_ = std::move(key); }
void Wireguard::set_peer_endpoint(std::string endpoint) { this->peer_endpoint_ = std::move(endpoint); }
void Wireguard::set_peer_key(std::string key) { this->peer_key_ = std::move(key); }
void Wireguard::set_peer_port(uint16_t port) { this->peer_port_ = port; }
void Wireguard::set_preshared_key(std::string key) { this->preshared_key_ = std::move(key); }

void Wireguard::set_keepalive(uint16_t seconds) { this->keepalive_ = seconds; }
void Wireguard::set_srctime(time::RealTimeClock* srctime) { this->srctime_ = srctime; }

bool Wireguard::is_peer_up() { return this->wg_peer_up; }

void Wireguard::start_connection() {
    if(wg_initialized != ESP_OK) {
        ESP_LOGE(TAG, "cannot start connection, initialization in error with code %d", wg_initialized);
        return;
    }

    if(!srctime_->now().is_valid()) {
        ESP_LOGI(TAG, "waiting for system time to be synchronized");
        return;
    }

    if(wg_connected == ESP_OK) {
        ESP_LOGD(TAG, "connection already started");
        return;
    }

    ESP_LOGD(TAG, "connecting...");
    wg_connected = esp_wireguard_connect(&wg_ctx);
    if(wg_connected == ESP_OK)
        ESP_LOGI(TAG, "connection started");
    else
        ESP_LOGW(TAG, "cannot start connection, error code %d", wg_connected);
}

}  // namespace wireguard
}  // namespace esphome

#endif
// vim: tabstop=4 shiftwidth=4 expandtab
