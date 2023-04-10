#ifdef USE_ESP_IDF

#include <functional>
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_wireguard.h"
#include "wireguard.h"


namespace esphome {
namespace wireguard {

static const char * const TAG = "wireguard";

void Wireguard::setup() {
    ESP_LOGD(TAG, "initializing...");

    wg_config_.allowed_ip = &address_[0];
    wg_config_.private_key = &private_key_[0];
    wg_config_.endpoint = &peer_endpoint_[0];
    wg_config_.public_key = &peer_public_key_[0];
    wg_config_.port = peer_port_;
    wg_config_.allowed_ip_mask = &netmask_[0];
    wg_config_.persistent_keepalive = keepalive_;

    if(preshared_key_.length() > 0)
        wg_config_.preshared_key = &preshared_key_[0];

    wg_initialized_ = esp_wireguard_init(&wg_config_, &wg_ctx_);
    
    if(wg_initialized_ == ESP_OK) {
        ESP_LOGI(TAG, "initialized");
        srctime_->add_on_time_sync_callback(std::bind(&Wireguard::start_connection_, this));
        start_connection_();
    } else {
        ESP_LOGE(TAG, "cannot initialize, error code %d", wg_initialized_);
        this->mark_failed();
    }
}

void Wireguard::update() {
    if(wg_initialized_ == ESP_OK && wg_connected_ == ESP_OK) {
        wg_peer_up_ = (esp_wireguardif_peer_is_up(&wg_ctx_) == ESP_OK);
    } else {
        ESP_LOGD(TAG, "initialized: %s (error %d)", (wg_initialized_ == ESP_OK ? "yes" : "no"), wg_initialized_);
        ESP_LOGD(TAG, "connection: %s (error %d)", (wg_connected_ == ESP_OK ? "active" : "inactive"), wg_connected_);
        wg_peer_up_ = false;
    }

    if(wg_peer_up_) {
        if(wg_last_peer_up_ == 0) {
            ESP_LOGI(TAG, "peer online");
            wg_last_peer_up_ = srctime_->utcnow().timestamp;
        }
    } else {
        ESP_LOGD(TAG, "peer offline");
        wg_last_peer_up_ = 0;
    }
}

void Wireguard::dump_config() {
    ESP_LOGCONFIG(TAG, "Configuration");
    ESP_LOGCONFIG(TAG, "  address: %s",this->address_.data());
    ESP_LOGCONFIG(TAG, "  netmask: %s",this->netmask_.data());
    ESP_LOGCONFIG(TAG, "  private key: %s...=",this->private_key_.substr(0,5).data());
    ESP_LOGCONFIG(TAG, "  peer endpoint: %s",this->peer_endpoint_.data());
    ESP_LOGCONFIG(TAG, "  peer port: %d",this->peer_port_);
    ESP_LOGCONFIG(TAG, "  peer public key: %s",this->peer_public_key_.data());
    ESP_LOGCONFIG(TAG, "  peer preshared key: %s...=",this->preshared_key_.substr(0,5).data());
    ESP_LOGCONFIG(TAG, "  peer persistent keepalive: %d",this->keepalive_);
}

void Wireguard::on_shutdown() {
    if(wg_initialized_ == ESP_OK && wg_connected_ == ESP_OK) {
        ESP_LOGD(TAG, "disconnecting...");
        esp_wireguard_disconnect(&wg_ctx_);
    }
}

void Wireguard::set_address(const std::string address) { this->address_ = std::move(address); }
void Wireguard::set_netmask(const std::string netmask) { this->netmask_ = std::move(netmask); }
void Wireguard::set_private_key(const std::string key) { this->private_key_ = std::move(key); }
void Wireguard::set_peer_endpoint(const std::string endpoint) { this->peer_endpoint_ = std::move(endpoint); }
void Wireguard::set_peer_public_key(const std::string key) { this->peer_public_key_ = std::move(key); }
void Wireguard::set_peer_port(const uint16_t port) { this->peer_port_ = port; }
void Wireguard::set_preshared_key(const std::string key) { this->preshared_key_ = std::move(key); }

void Wireguard::set_keepalive(const uint16_t seconds) { this->keepalive_ = seconds; }
void Wireguard::set_srctime(time::RealTimeClock* srctime) { this->srctime_ = srctime; }

void Wireguard::start_connection_() {
    if(wg_initialized_ != ESP_OK) {
        ESP_LOGE(TAG, "cannot start connection, initialization in error with code %d", wg_initialized_);
        return;
    }

    if(!srctime_->now().is_valid()) {
        ESP_LOGI(TAG, "waiting for system time to be synchronized");
        return;
    }

    if(wg_connected_ == ESP_OK) {
        ESP_LOGD(TAG, "connection already started");
        return;
    }

    ESP_LOGD(TAG, "connecting...");
    wg_connected_ = esp_wireguard_connect(&wg_ctx_);
    if(wg_connected_ == ESP_OK)
        ESP_LOGI(TAG, "connection started");
    else
        ESP_LOGW(TAG, "cannot start connection, error code %d", wg_connected_);
}

}  // namespace wireguard
}  // namespace esphome

#endif
// vim: tabstop=4 shiftwidth=4 expandtab
