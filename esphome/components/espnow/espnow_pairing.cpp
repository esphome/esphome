#include "espnow_pairing.h"

#if defined(USE_ESP32X)

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/log.h"

#include <memory>
#include <map>

namespace esphome ::espnow_pairing {

static const uint8_t SPNOW_RANDOM_CODE_LENGTH = 5;

void ESPNowPairing::setup(){};

void ESPNowPairing::update_active_keeper_() {
  uint8_t max = 0;
  if (this->parent_ != nullptr) {
    if (this->protocol_mode_ == drudge) {
      this->parent_->set_active_keeper(0);
    }
    for (const auto &[peer, rssi] : this->pairings_) {
      this->parent_->add_peer(peer);
      if (this->protocol_mode_ == drudge) {
        if (rssi > max) {
          this->parent_->set_active_keeper(peer);
          max = rssi;
        }
      }
    }
  }
};

void ESPNowPairing::dump_config() {
  ESP_LOGCONFIG(TAG, "Network Name:", this->network_name_.c_str());
  ESP_LOGCONFIG(TAG, "Protocol mode:", this->get_mode_name_().c_str());
  ESP_LOGCONFIG(TAG, "Number of peers:", this->pairings_.size());
  ESP_LOGCONFIG(TAG, "Current keeper:", espnow_encode_peer(this->active_keeper_).c_str());
};

void ESPNowPairing::start() {
  this->scanning_ = true;
  this->start_scan_timer();
  this->random_code_ = espnow_random_string(ESPNOW_RANDOM_CODE_LENGTH);
  std::string payload = this->network_name_ + "#" + this->random_code_;
  this->send(peer, payload.c_str(), payload.size(), 1);
};

void ESPNowPairing::stop() {
  this->scanning_ = true;
  this->cancel_timeout("espnow_scan");
};

void ESPNowPairing::update() {
  if (this->parent_ == nullptr || !this->parent_->is_ready)
    return;

  if (this->protocol_mode_ == drudge) {
    uint8_t payload = 0;
    for (const auto &[peer, value] : this->pairings_) {
      this->send(peer, &payload, 0, 3);
    }
  } else {
    for (const auto &[peer, value] : this->pairings_) {
      if (value > 0) {
        value -= 1;
        if (value == 128) {
          this->pairings_.erase[key];
        }
      }
    }
    this->save();
  };
}
void ESPNowPairing::start_scan_timer_() {
  this->set_timeout("espnow_scan", this->scan_timeout_, [this]() { this->stop(); });
};

bool on_unknown_peer(const espnow::ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) { return false; }

bool ESPNowPairing::on_received(const espnow::ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) {
  /*
    std::string payload = packet((char *) packet.get_payload(), packet.size);
    if (this->protocol_mode_ == drudge) {
      if (packet.get_command() == 2) {  // pair request response.
        if (packet.is_broadcast)
          return false;
        if (packet.size != 9)
          return false;
        std::string hash = espnow_i2h(fnv1_hash(this->network_name_ + "#" + this->random_code_ + "|" +
                                                payload.substr(0, 4) + "%" + this->private_key_));
        if (hash != payload.substr(5, 4))
          return false;
        uint8_t x;
        this->send(peer, &x, 0, 3);
      } else if (packet.get_command() == 4 && this->pairings_.count(packet.peer) > 0) {  // pong
        this->pairings_[packet.peer] = packet.rssi;
        this->update_active_keeper_();
      } else if (packet.get_command() == 16 && this->pairings_.count(packet.peer) > 0) {  // new keeper
        if (packet.size != 6)
          return false;
        uint64_t newkeeper = 0;
        memcpy((void *) &newkeeper, packet.get_payload(), 6);
        this->pairings_[newkeeper] = 0;
        this->parent_->add_peer(newkeeper);
      } else {
        return false;
      }
    } else {
      if (packet.get_command() == 1) {  // init pairing
        std::string network;
        stringstream ss(payload);
        if (!getline(ss, network, '#'))
          return false;
        if (network != this->network_name_)
          return false;

        std::string rand = espnow_random_string(ESPNOW_RANDOM_CODE_LENGTH);
        std::string hash = rand + "%" + espnow_i2h(fnv1_hash(payload + "|" + rand + "%" + this->private_key_));
        this->send(packet.peer, hash.c_str(), hash.size(), 2);
        this->pairings_[packet.peer] = 255;
      } else if (packet.get_command() == 3) {  // Ping
        this->pairings_[packet.peer] = 127;
        uint8_t x;
        this->send(peer, &x, 0, 4);
      } else {
        return false;
      }
    }
    */
  return true;
};

void ESPNowPairing::save() {
  if (!this->pref_.save(&this->pairings_)) {
    ESP_LOGW(TAG, "Failed to save pairings");
    return;
  }
}

}  // namespace esphome::espnow_pairing

#endif
