#ifdef USE_ESP_IDF

#include <functional>
#include <time.h>
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_wireguard.h"
#include "wireguard.h"


namespace esphome {
namespace wireguard {

static const char * const TAG = "wireguard";
static const int WG_MAX_PEER_DOWN = 120; // seconds

void Wireguard::setup() {
    ESP_LOGD(TAG, "initializing...");

    wg_config.allowed_ip = &address_[0];
    wg_config.private_key = &private_key_[0];
    wg_config.endpoint = &peer_endpoint_[0];
    wg_config.public_key = &peer_key_[0];
    wg_config.port = peer_port_;
    wg_config.preshared_key = &preshared_key_[0];
    wg_config.allowed_ip_mask = &netmask_[0];
    wg_config.persistent_keepalive = keepalive_;

    wg_initialized = esp_wireguard_init(&wg_config, &wg_ctx);
    
    if(wg_initialized == ESP_OK)
        ESP_LOGI(TAG, "initialized");
    else
        ESP_LOGE(TAG, "cannot initialize, error code %d", wg_initialized);

    srctime_->add_on_time_sync_callback(std::bind(&Wireguard::start_connection, this));
}

void Wireguard::update() {
    ESP_LOGD(TAG, "initialized: %s (error %d)", (wg_initialized == ESP_OK ? "yes" : "no"), wg_initialized);
    ESP_LOGD(TAG, "connection: %s (error %d)", (wg_connected == ESP_OK ? "active" : "inactive"), wg_connected);

    if(wg_initialized == ESP_OK && wg_connected == ESP_OK)
        wg_peer_up = (esp_wireguardif_peer_is_up(&wg_ctx) == ESP_OK);
    else
        wg_peer_up = false;

    strftime(wg_tmp_buffer, sizeof(wg_tmp_buffer), "offline since %F %T", localtime(&wg_last_peer_up));
    ESP_LOGD(TAG, "peer: %s", (wg_peer_up ? "connected" : wg_tmp_buffer));

    if(wg_peer_up)
        wg_last_peer_up = srctime_->now().timestamp;
    else if(wg_initialized == ESP_OK)
    {
        if(wg_connected == ESP_OK
              && (srctime_->now().timestamp - wg_last_peer_up) > WG_MAX_PEER_DOWN)
        {
            ESP_LOGW(TAG, "peer offline for more than %d seconds", WG_MAX_PEER_DOWN);
            ESP_LOGD(TAG, "aborting connection...");
            wg_aborted = esp_wireguard_disconnect(&wg_ctx);
            if(wg_aborted == ESP_OK)
            {
                ESP_LOGI(TAG, "connection aborted");
                wg_connected = ESP_FAIL;
                start_connection();
            }
            else
                ESP_LOGE(TAG, "cannot abort connection, error code %d", wg_aborted);
        }
        else if(wg_connected != ESP_OK)
        {
            start_connection();
        }
    }
}

void Wireguard::dump_config() {
    ESP_LOGCONFIG(TAG, "Configuration");
    ESP_LOGCONFIG(TAG, "  address: %s",this->address_.data());
    ESP_LOGCONFIG(TAG, "  netmask: %s",this->netmask_.data());
    ESP_LOGCONFIG(TAG, "  private key: %s",this->private_key_.data());  // TODO print?
    ESP_LOGCONFIG(TAG, "  endpoint: %s",this->peer_endpoint_.data());
    ESP_LOGCONFIG(TAG, "  peer key: %s",this->peer_key_.data());
    ESP_LOGCONFIG(TAG, "  peer port: %d",this->peer_port_);
    ESP_LOGCONFIG(TAG, "  preshared key[%d]: %s",this->preshared_key_.length(), this->preshared_key_.data());  // TODO print?
    ESP_LOGCONFIG(TAG, "  keepalive: %d",this->keepalive_);
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

void Wireguard::start_connection() {
	ESP_LOGD(TAG, "time synchronized");

    if(wg_initialized == ESP_OK) {
        if(wg_connected != ESP_OK) {
            ESP_LOGD(TAG, "connecting...");
            wg_connected = esp_wireguard_connect(&wg_ctx);
            if(wg_connected == ESP_OK) {
                ESP_LOGI(TAG, "connection started");
            } else {
                ESP_LOGW(TAG, "cannot start connection, error code %d", wg_connected);
            }
        } else {
            ESP_LOGD(TAG, "connection already started");
        }
    } else {
        ESP_LOGE(TAG, "cannot start connection, initialization in error with code %d", wg_initialized);
        wg_connected = ESP_FAIL;
    }

    wg_last_peer_up = srctime_->now().timestamp;
}

}  // namespace wireguard
}  // namespace esphome

#endif
// vim: tabstop=4 shiftwidth=4 expandtab
