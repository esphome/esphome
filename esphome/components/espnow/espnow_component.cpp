#include "espnow_component.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_err.h"
#include "espnow_platform.h"

#include <algorithm>
#include <cinttypes>
#include <cstring>
#include <utility>

#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::espnow {

ESPHOME_LOG_TAG(TAG, "espnow");

ESPNowComponent *global_esp_now = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const LogString *espnow_error_to_str(esp_err_t error) {
  switch (error) {
    case ESP_ERR_ESPNOW_FAILED:
      return LOG_STR("ESPNow is in fail mode");
    case ESP_ERR_ESPNOW_OWN_ADDRESS:
      return LOG_STR("Message to your self");
    case ESP_ERR_ESPNOW_DATA_SIZE:
      return LOG_STR("Data size to large");
    case ESP_ERR_ESPNOW_PEER_NOT_SET:
      return LOG_STR("Peer address not set");
    case ESP_ERR_ESPNOW_PEER_NOT_PAIRED:
      return LOG_STR("Peer address not paired");
    case ESP_ERR_ESPNOW_NOT_INIT:
      return LOG_STR("Not init");
    case ESP_ERR_ESPNOW_ARG:
      return LOG_STR("Invalid argument");
    case ESP_ERR_ESPNOW_INTERNAL:
      return LOG_STR("Internal Error");
    case ESP_ERR_ESPNOW_NO_MEM:
      return LOG_STR("Out of memory");
    case ESP_ERR_ESPNOW_NOT_FOUND:
      return LOG_STR("Peer not found");
    case ESP_ERR_ESPNOW_IF:
      return LOG_STR("Interface does not match");
    case ESP_OK:
      return LOG_STR("OK");
    case ESP_NOW_SEND_FAIL:
    case ESP_FAIL:
      return LOG_STR("Failed");
    default:
      return LOG_STR("Unknown Error");
  }
}

// The analyzer traces a leak on the failed-push path, which cannot happen: the
// pools are sized to the queue capacity (SIZE-1), so allocate() returns nullptr
// before push() can find the ring full.
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
void ESPNowComponent::send_reported(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // Allocate an event from the pool
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    // No events available - queue is full or we're out of memory
    this->receive_packet_queue_.increment_dropped_count();
    this->enable_loop_soon_any_context();
    return;
  }

  packet->load_sent_data(mac_addr, status);

  // Push always succeeds: pool is sized to queue capacity (SIZE-1), so if
  // allocate() returned non-null, the queue cannot be full.
  this->receive_packet_queue_.push(packet);

  // Re-enable and wake the main loop to process the ESP-NOW send event
  this->enable_loop_soon_any_context();
}

void ESPNowComponent::packet_received(const uint8_t *src_addr, const uint8_t *des_addr, const uint8_t *data, int size,
                                      int8_t rssi, uint32_t timestamp) {
  // Drop oversized frames before copying. ESP-NOW v2 peers (IDF >= 5.4 builds a
  // v2 stack with no opt-out) can send up to ESP_NOW_MAX_DATA_LEN_V2 (1470 B),
  // but the receive buffer only fits frames up to ``max_payload_size``; copying a
  // larger frame would overflow packet_.receive.data.
  if (size < 0 || static_cast<size_t>(size) > ESPNOW_MAX_DATA_LEN) {
    this->receive_packet_queue_.increment_dropped_count();
    this->enable_loop_soon_any_context();
    return;
  }

  // Allocate an event from the pool
  ESPNowPacket *packet = this->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    // No events available - queue is full or we're out of memory
    this->receive_packet_queue_.increment_dropped_count();
    this->enable_loop_soon_any_context();
    return;
  }

  packet->load_received_data(src_addr, des_addr, data, size, rssi, timestamp);

  // Push always succeeds: pool is sized to queue capacity (SIZE-1), so if
  // allocate() returned non-null, the queue cannot be full.
  this->receive_packet_queue_.push(packet);

  // Re-enable and wake the main loop to process the ESP-NOW receive event
  this->enable_loop_soon_any_context();
}
// NOLINTEND(clang-analyzer-unix.Malloc)

ESPNowComponent::ESPNowComponent() { global_esp_now = this; }

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "espnow:");
  // Only report driver details once enabled; with enable_on_boot: false the
  // Wi-Fi driver is not initialized yet and the version call would crash,
  // and after a failed enable_() the values would be meaningless.
  if (this->state_ != ESPNOW_STATE_ENABLED) {
    // OFF here means enable_() failed; the core logs the FAILED marker separately
    ESP_LOGCONFIG(TAG, "  %s", this->is_disabled() ? LOG_STR_LITERAL("Disabled") : LOG_STR_LITERAL("Not enabled"));
    return;
  }
  char own_addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->own_address_, own_addr_buf);
  ESP_LOGCONFIG(TAG,
                "  Own address: %s\n"
                "  Wi-Fi channel: %d",
                own_addr_buf, this->wifi_channel_);
  uint32_t version = platform::get_version();
  if (version != 0) {
    ESP_LOGCONFIG(TAG, "  Version: v%" PRIu32, version);
  }
#ifdef USE_WIFI
  ESP_LOGCONFIG(TAG, "  Wi-Fi enabled: %s", YESNO(this->is_wifi_enabled()));
#endif
}

bool ESPNowComponent::is_wifi_enabled() {
#ifdef USE_WIFI
  return wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_disabled();
#else
  return false;
#endif
}

void ESPNowComponent::setup() {
#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
  if (wifi::global_wifi_component != nullptr) {
    wifi::global_wifi_component->add_connect_state_listener(this);
  }
#endif
  if (this->enable_on_boot_) {
    this->enable_();
  } else {
    this->state_ = ESPNOW_STATE_DISABLED;
  }
}

#if defined(USE_WIFI) && defined(USE_WIFI_CONNECT_STATE_LISTENERS)
void ESPNowComponent::on_wifi_connect_state(StringRef ssid, std::span<const uint8_t, 6> bssid) {
  if (ssid.empty()) {
    return;  // Disconnected; the channel is only meaningful while associated
  }
  uint8_t old_channel = this->wifi_channel_;
  this->get_wifi_channel();
  if (this->wifi_channel_ != old_channel) {
    ESP_LOGI(TAG, "WiFi channel changed from %d to %d", old_channel, this->wifi_channel_);
  }
  // Peers added while the station was scanning were bound to the channel of that moment, so rebind on every connect
  if (this->state_ == ESPNOW_STATE_ENABLED) {
    platform::rebind_peers();
  }
}
#endif

void ESPNowComponent::enable() {
  if (this->state_ == ESPNOW_STATE_ENABLED)
    return;

  ESP_LOGD(TAG, "Enabling");
  this->state_ = ESPNOW_STATE_OFF;

  this->enable_();
}

void ESPNowComponent::enable_() {
  if (!this->is_wifi_enabled()) {
    platform::init_radio();
    this->apply_wifi_channel();
  }
  this->get_wifi_channel();

  esp_err_t err = platform::init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  platform::read_mac(this->own_address_);

  this->state_ = ESPNOW_STATE_ENABLED;

  for (auto peer : this->peers_) {
    this->add_peer(peer.address);
  }
}

void ESPNowComponent::disable() {
  if (this->state_ == ESPNOW_STATE_DISABLED)
    return;

  ESP_LOGD(TAG, "Disabling");
  this->state_ = ESPNOW_STATE_DISABLED;

  esp_err_t err = platform::deinit();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Deinit failed: %s", esp_err_to_name(err));
  }
}

void ESPNowComponent::apply_wifi_channel() {
  if (this->state_ == ESPNOW_STATE_DISABLED) {
    ESP_LOGE(TAG, "Cannot set channel when ESPNOW disabled");
    this->mark_failed();
    return;
  }

  if (this->is_wifi_enabled()) {
    ESP_LOGE(TAG, "Cannot set channel when Wi-Fi enabled");
    this->mark_failed();
    return;
  }

  ESP_LOGI(TAG, "Channel set to %d.", this->wifi_channel_);
  platform::set_channel(this->wifi_channel_);
  if (this->state_ == ESPNOW_STATE_ENABLED) {
    platform::rebind_peers();
  }
}

void ESPNowComponent::loop() {
  // Process received packets
  ESPNowPacket *packet = this->receive_packet_queue_.pop();
  while (packet != nullptr) {
    switch (packet->type_) {
      case ESPNowPacket::RECEIVED: {
        const ESPNowRecvInfo info = packet->get_receive_info();
        if (!platform::peer_exists(info.src_addr)) {
          bool handled = false;
          for (auto *handler : this->unknown_peer_handlers_) {
            if (handler->on_unknown_peer(info, packet->packet_.receive.data, packet->packet_.receive.size)) {
              handled = true;
              break;  // If a handler returns true, stop processing further handlers
            }
          }
          if (!handled && this->auto_add_peer_) {
            this->add_peer(info.src_addr);
          }
        }
        // Intentionally left as if instead of else in case the peer is added above
        if (platform::peer_exists(info.src_addr)) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          char src_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
          char dst_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
          // Cap the hex dump at a v1 frame: a full v2 frame would need a
          // ~4.4 KB stack buffer.
          char hex_buf[format_hex_pretty_size(ESP_NOW_MAX_DATA_LEN)];
          format_mac_addr_upper(info.src_addr, src_buf);
          format_mac_addr_upper(info.des_addr, dst_buf);
          ESP_LOGV(TAG, "<<< [%s -> %s] %s", src_buf, dst_buf,
                   format_hex_pretty_to(hex_buf, packet->packet_.receive.data,
                                        std::min<uint16_t>(packet->packet_.receive.size, ESP_NOW_MAX_DATA_LEN)));
#endif
          // Without a destination address every packet goes to the receive handlers
          if (ESPNOW_REPORTS_DESTINATION && memcmp(info.des_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
            for (auto *handler : this->broadcast_handlers_) {
              if (handler->on_broadcast(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;  // If a handler returns true, stop processing further handlers
            }
          } else {
            for (auto *handler : this->receive_handlers_) {
              if (handler->on_receive(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;  // If a handler returns true, stop processing further handlers
            }
          }
        }
        break;
      }
      case ESPNowPacket::SENT: {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
        char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
        format_mac_addr_upper(packet->packet_.sent.address, addr_buf);
        ESP_LOGV(TAG, ">>> [%s] %s", addr_buf, LOG_STR_ARG(espnow_error_to_str(packet->packet_.sent.status)));
#endif
        if (this->current_send_packet_ != nullptr) {
          if (this->current_send_packet_->callback_ != nullptr) {
            this->current_send_packet_->callback_(packet->packet_.sent.status);
          }
          this->send_packet_pool_.release(this->current_send_packet_);
          this->current_send_packet_ = nullptr;  // Reset current packet after sending
        }
        break;
      }
      default:
        break;
    }
    // Return the packet to the pool
    this->receive_packet_pool_.release(packet);
    packet = this->receive_packet_queue_.pop();
  }

  // Process sending packet queue
  if (this->current_send_packet_ == nullptr) {
    this->send_();
  }

  // Log dropped received packets periodically
  uint16_t received_dropped = this->receive_packet_queue_.get_and_reset_dropped_count();
  if (received_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u received packets (queue full or oversized frame)", received_dropped);
  }

  // Log dropped send packets periodically
  uint16_t send_dropped = this->send_packet_queue_.get_and_reset_dropped_count();
  if (send_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u send packets (queue full)", send_dropped);
  }

  // Nothing left to do; sleep until a callback or send() re-enables the loop.
  // A packet in flight (current_send_packet_) needs no loop time even when more
  // packets are queued behind it: the send callback re-enables the loop when
  // the result arrives, and the SENT event handler above starts the next send.
  if (this->receive_packet_queue_.empty() &&
      (this->current_send_packet_ != nullptr || this->send_packet_queue_.empty())) {
    this->disable_loop();
  }
}

uint8_t ESPNowComponent::get_wifi_channel() {
  this->wifi_channel_ = platform::get_channel();
  return this->wifi_channel_;
}

// Same analyzer false positive as above: the send pool is sized to the send queue's capacity
// NOLINTBEGIN(clang-analyzer-unix.Malloc)
esp_err_t ESPNowComponent::send(const uint8_t *peer_address, const uint8_t *payload, size_t size,
                                send_callback_t callback) {
  if (this->state_ != ESPNOW_STATE_ENABLED) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  } else if (this->is_failed()) {
    return ESP_ERR_ESPNOW_FAILED;
  } else if (peer_address == nullptr) {
    return ESP_ERR_ESPNOW_PEER_NOT_SET;
  } else if (memcmp(peer_address, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    return ESP_ERR_ESPNOW_OWN_ADDRESS;
  } else if (size > ESPNOW_MAX_DATA_LEN) {
    return ESP_ERR_ESPNOW_DATA_SIZE;
  } else if (!platform::peer_exists(peer_address)) {
    if (memcmp(peer_address, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0 || this->auto_add_peer_) {
      esp_err_t err = this->add_peer(peer_address);
      if (err != ESP_OK) {
        return err;
      }
    } else {
      return ESP_ERR_ESPNOW_PEER_NOT_PAIRED;
    }
  }
  // Allocate a packet from the pool
  ESPNowSendPacket *packet = this->send_packet_pool_.allocate();
  if (packet == nullptr) {
    this->send_packet_queue_.increment_dropped_count();
    ESP_LOGE(TAG, "Failed to allocate send packet from pool");
    this->status_momentary_warning();
    return ESP_ERR_ESPNOW_NO_MEM;
  }
  // Load the packet data
  packet->load_data(peer_address, payload, size, std::move(callback));
  // Push the packet to the send queue
  this->send_packet_queue_.push(packet);
  // Loop may be disabled while idle; re-enable it to send the packet
  // (any-context variant so callers off the main loop are safe too)
  this->enable_loop_soon_any_context();
  return ESP_OK;
}
// NOLINTEND(clang-analyzer-unix.Malloc)

void ESPNowComponent::send_() {
  ESPNowSendPacket *packet = this->send_packet_queue_.pop();
  if (packet == nullptr) {
    return;  // No packets to send
  }

  this->current_send_packet_ = packet;
  esp_err_t err = platform::send(packet->address_, packet->data_, packet->size_);
  if (err != ESP_OK) {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    format_mac_addr_upper(packet->address_, addr_buf);
    ESP_LOGE(TAG, "Failed to send packet to %s - %s", addr_buf, LOG_STR_ARG(espnow_error_to_str(err)));
    if (packet->callback_ != nullptr) {
      packet->callback_(err);
    }
    this->status_momentary_warning();
    this->send_packet_pool_.release(packet);
    this->current_send_packet_ = nullptr;  // Reset current packet
    return;
  }
}

esp_err_t ESPNowComponent::add_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  }

  if (memcmp(peer, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    this->status_momentary_warning();
    return ESP_ERR_INVALID_MAC;
  }

  if (!platform::peer_exists(peer)) {
    esp_err_t err = platform::add_peer(peer);
    if (err != ESP_OK) {
      char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(peer, peer_buf);
      ESP_LOGE(TAG, "Failed to add peer %s - %s", peer_buf, LOG_STR_ARG(espnow_error_to_str(err)));
      this->status_momentary_warning();
      return err;
    }
  }
  bool found = false;
  for (auto &it : this->peers_) {
    if (it == peer) {
      found = true;
      break;
    }
  }
  if (!found) {
    ESPNowPeer new_peer;
    memcpy(new_peer.address, peer, ESP_NOW_ETH_ALEN);
    this->peers_.push_back(new_peer);
  }

  return ESP_OK;
}

esp_err_t ESPNowComponent::del_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESP_ERR_ESPNOW_NOT_INIT;
  }
  if (platform::peer_exists(peer)) {
    esp_err_t err = platform::del_peer(peer);
    if (err != ESP_OK) {
      char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(peer, peer_buf);
      ESP_LOGE(TAG, "Failed to delete peer %s - %s", peer_buf, LOG_STR_ARG(espnow_error_to_str(err)));
      this->status_momentary_warning();
      return err;
    }
  }
  for (auto it = this->peers_.begin(); it != this->peers_.end(); ++it) {
    if (*it == peer) {
      this->peers_.erase(it);
      break;
    }
  }
  return ESP_OK;
}

}  // namespace esphome::espnow

#endif  // USE_ESP32 || USE_ESP8266
