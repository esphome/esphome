#include "esphome/core/log.h"
#include "wireguard.h"
#include "WireGuard-ESP32.h"
#include <arpa/inet.h>

namespace esphome {
namespace wireguard {

static const char *TAG = "wireguard";
static WireGuard wg;

void Wireguard::setup() {
    ESP_LOGI(TAG, "wireguard setup start");

    IPAddress local_ip(inet_addr(this->address_.data()));           // VPN IP for this VPN client

    wg.begin(
        local_ip,
        this->private_key_.data(),
        this->peer_endpoint_.data(),
        this->peer_key_.data(),
        this->peer_port_,
        this->preshared_key_.data()
    );

    ESP_LOGI(TAG, "wireguard setup end");
}

void Wireguard::update() {
    // ESP_LOGD(TAG, "wireguard %s", wg.is_initialized()?"true":"false");
}

void Wireguard::dump_config(){
    ESP_LOGCONFIG(TAG, "Configuration");
    ESP_LOGCONFIG(TAG, "  address: %s",this->address_.data());
    ESP_LOGCONFIG(TAG, "  private key: %s",this->private_key_.data());
    ESP_LOGCONFIG(TAG, "  endpoint: %s",this->peer_endpoint_.data());
    ESP_LOGCONFIG(TAG, "  peer key: %s",this->peer_key_.data());
    ESP_LOGCONFIG(TAG, "  peer port: %d",this->peer_port_);
    ESP_LOGCONFIG(TAG, "  preshared key: %s",this->preshared_key_.data());
}

void Wireguard::set_address(std::string address) { this->address_ = std::move(address); }
void Wireguard::set_private_key(std::string key) { this->private_key_ = std::move(key); }
void Wireguard::set_peer_endpoint(std::string endpoint) { this->peer_endpoint_ = std::move(endpoint); }
void Wireguard::set_peer_key(std::string key) { this->peer_key_ = std::move(key); }
void Wireguard::set_peer_port(uint16_t port) { this->peer_port_ = port; }
void Wireguard::set_preshared_key(std::string key) { this->preshared_key_ = std::move(key); }

}  // namespace wireguard
}  // namespace esphome