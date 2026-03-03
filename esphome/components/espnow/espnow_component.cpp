#include "espnow_component.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_err.h"

#if defined(USE_ESP8266)
#include "espnow_platform.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#if defined(USE_ESP32)
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_now.h>
#include <esp_random.h>
#include <esp_wifi.h>
#endif

#include <cstring>
#include <memory>

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

namespace esphome::espnow {

static constexpr const char *TAG = "espnow";

static const int CONFIG_ESPNOW_WAKE_WINDOW = 50;
static const int CONFIG_ESPNOW_WAKE_INTERVAL = 100;

ESPNowComponent *global_esp_now = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
static const LogString *espnow_send_status_to_str(espnow_send_status_t status) {
#if defined(USE_ESP8266)
  switch (status) {
    case 0:
      return LOG_STR("Send Success (ACK)");
    case 1:
      return LOG_STR("Send Failed (No ACK/Peer Unreachable)");
    default:
      return LOG_STR("Send Unknown");
  }
#else
  if (status == ESP_NOW_SEND_SUCCESS) {
    return LOG_STR("Send Success (ACK)");
  }
  if (status == ESP_NOW_SEND_FAIL) {
    return LOG_STR("Send Failed");
  }
  return LOG_STR("Send Unknown");
#endif
}
#endif

static const LogString *espnow_error_to_str(espnow_err_t error) {
  switch (error) {
    case ESPNOW_ERR_FAILED:
      return LOG_STR("ESPNow is in fail mode");
    case ESPNOW_ERR_OWN_ADDRESS:
      return LOG_STR("Message to your self");
    case ESPNOW_ERR_DATA_SIZE:
      return LOG_STR("Data size to large");
    case ESPNOW_ERR_PEER_NOT_SET:
      return LOG_STR("Peer address not set");
    case ESPNOW_ERR_PEER_NOT_PAIRED:
      return LOG_STR("Peer address not paired");
    case ESPNOW_ERR_NOT_INIT:
      return LOG_STR("Not init");
    case ESPNOW_ERR_NO_MEM:
      return LOG_STR("Out of memory");
    case ESPNOW_OK:
      return LOG_STR("OK");
#if defined(USE_ESP32)
    case ESP_ERR_ESPNOW_ARG:
      return LOG_STR("Invalid argument");
    case ESP_ERR_ESPNOW_INTERNAL:
      return LOG_STR("Internal Error");
    case ESP_ERR_ESPNOW_NOT_FOUND:
      return LOG_STR("Peer not found");
    case ESP_ERR_ESPNOW_IF:
      return LOG_STR("Interface does not match");
    case ESP_NOW_SEND_FAIL:
      return LOG_STR("Failed");
#endif
    default:
      return LOG_STR("Unknown Error");
  }
}

#if defined(USE_ESP32) && defined(ESP_IDF_VERSION_VAL)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
void on_send_report(const esp_now_send_info_t *info, esp_now_send_status_t status)
#else
void on_send_report(const uint8_t *mac_addr, esp_now_send_status_t status)
#endif
#elif defined(USE_ESP32)
void on_send_report(const uint8_t *mac_addr, esp_now_send_status_t status)
#else
void on_send_report(uint8_t *mac_addr, uint8_t status)
#endif
{
  ESPNowComponent *esp_now = global_esp_now;
  if (esp_now == nullptr) {
    return;
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
#if defined(USE_ESP32) && defined(ESP_IDF_VERSION_VAL)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  format_mac_addr_upper(info->des_addr, peer_buf);
#else
  format_mac_addr_upper(mac_addr, peer_buf);
#endif
#else
  format_mac_addr_upper(mac_addr, peer_buf);
#endif
  ESP_LOGVV(TAG, "Send callback peer=%s status=%d (%s) ch=%u", peer_buf, status,
            LOG_STR_ARG(espnow_send_status_to_str(status)), esp_now->wifi_channel_);
#endif

  // Allocate an event from the pool
  ESPNowPacket *packet = esp_now->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    // No events available - queue is full or we're out of memory
    esp_now->receive_packet_queue_.increment_dropped_count();
    return;
  }

// Load new packet data (replaces previous packet)
#if defined(USE_ESP32) && defined(ESP_IDF_VERSION_VAL)
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  packet->load_sent_data(info->des_addr, status);
#else
  packet->load_sent_data(mac_addr, status);
#endif
#elif defined(USE_ESP32)
  packet->load_sent_data(mac_addr, status);
#else
  packet->load_sent_data(mac_addr, status);
#endif

  // Push the packet to the queue
  esp_now->receive_packet_queue_.push(packet);
  // Push always because we're the only producer and the pool ensures we never exceed queue size

  // Wake main loop immediately to process ESP-NOW send event instead of waiting for select() timeout
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif
}

#if defined(USE_ESP32)
void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
#else
void on_data_received(uint8_t *mac_addr, uint8_t *data, uint8_t size) {
#endif
  ESPNowComponent *esp_now = global_esp_now;
  if (esp_now == nullptr) {
    return;
  }

  // Allocate an event from the pool
  ESPNowPacket *packet = esp_now->receive_packet_pool_.allocate();
  if (packet == nullptr) {
    // No events available - queue is full or we're out of memory
    esp_now->receive_packet_queue_.increment_dropped_count();
    return;
  }

  // Load new packet data (replaces previous packet)
#if defined(USE_ESP32)
  packet->load_received_data(info, data, size);
#else
  packet->load_received_data(mac_addr, data, size, esp_now->own_address_);
#endif

  // Push the packet to the queue
  esp_now->receive_packet_queue_.push(packet);
  // Push always because we're the only producer and the pool ensures we never exceed queue size

  // Wake main loop immediately to process ESP-NOW receive event instead of waiting for select() timeout
#if defined(USE_SOCKET_SELECT_SUPPORT) && defined(USE_WAKE_LOOP_THREADSAFE)
  App.wake_loop_threadsafe();
#endif
}

ESPNowComponent::ESPNowComponent() { global_esp_now = this; }

void ESPNowComponent::dump_config() {
  uint32_t version = 0;
#if defined(USE_ESP32)
  esp_now_get_version(&version);
#endif

  ESP_LOGCONFIG(TAG, "espnow:");
  if (this->is_disabled()) {
    ESP_LOGCONFIG(TAG, "  Disabled");
    return;
  }
  char own_addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->own_address_, own_addr_buf);
  ESP_LOGCONFIG(TAG,
                "  Own address: %s\n"
                "  Version: v%" PRIu32 "\n"
                "  Wi-Fi channel: %d",
                own_addr_buf, version, this->wifi_channel_);
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
#if defined(USE_ESP32)
#ifndef USE_WIFI
  // Initialize LwIP stack for wake_loop_threadsafe() socket support
  // When WiFi component is present, it handles esp_netif_init()
  ESP_ERROR_CHECK(esp_netif_init());
#endif
#endif

  if (this->enable_on_boot_) {
    this->enable_();
  } else {
    this->state_ = ESPNOW_STATE_DISABLED;
  }
}

void ESPNowComponent::enable() {
  if (this->state_ == ESPNOW_STATE_ENABLED)
    return;

  ESP_LOGD(TAG, "Enabling");
  this->state_ = ESPNOW_STATE_OFF;

  this->enable_();
}

void ESPNowComponent::enable_() {
  if (!this->is_wifi_enabled()) {
#if defined(USE_ESP32)
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_disconnect());
#else
    espnow_esp8266::init_wifi_station();
#endif

    this->apply_wifi_channel();
  }
  this->get_wifi_channel();

  espnow_err_t err = esp_now_init();
  if (err != ESPNOW_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", LOG_STR_ARG(espnow_error_to_str(err)));
    this->mark_failed();
    return;
  }

#if defined(USE_ESP8266)
  err = espnow_esp8266::set_self_role();
  if (err != ESPNOW_OK) {
    ESP_LOGE(TAG, "esp_now_set_self_role failed: %d", err);
    this->mark_failed();
    return;
  }
#endif

  err = esp_now_register_recv_cb(on_data_received);
  if (err != ESPNOW_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", LOG_STR_ARG(espnow_error_to_str(err)));
    this->mark_failed();
    return;
  }

  err = esp_now_register_send_cb(on_send_report);
  if (err != ESPNOW_OK) {
    ESP_LOGE(TAG, "esp_now_register_send_cb failed: %s", LOG_STR_ARG(espnow_error_to_str(err)));
    this->mark_failed();
    return;
  }

#if defined(USE_ESP32)
  esp_wifi_get_mac(WIFI_IF_STA, this->own_address_);
#else
  espnow_esp8266::read_mac(this->own_address_);
#endif

#ifdef USE_DEEP_SLEEP
#if defined(USE_ESP32)
  esp_now_set_wake_window(CONFIG_ESPNOW_WAKE_WINDOW);
  esp_wifi_connectionless_module_set_wake_interval(CONFIG_ESPNOW_WAKE_INTERVAL);
#endif
#endif

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

  esp_now_unregister_recv_cb();
  esp_now_unregister_send_cb();

  espnow_err_t err = esp_now_deinit();
  if (err != ESPNOW_OK) {
    ESP_LOGE(TAG, "esp_now_deinit failed! 0x%x", err);
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
#if defined(USE_ESP32)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(this->wifi_channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
#else
  espnow_esp8266::apply_wifi_channel(this->wifi_channel_);
#endif
}

void ESPNowComponent::loop() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr && wifi::global_wifi_component->is_connected()) {
    int32_t new_channel = wifi::global_wifi_component->get_wifi_channel();
    if (new_channel != this->wifi_channel_) {
      ESP_LOGI(TAG, "Wifi Channel is changed from %d to %d.", this->wifi_channel_, new_channel);
      this->wifi_channel_ = new_channel;
#if defined(USE_ESP8266)
      auto peers_copy = this->peers_;
      for (const auto &peer : peers_copy) {
        (void) this->del_peer(peer.address);
        (void) this->add_peer(peer.address);
      }
#endif
    }
  }
#endif
  // Process received packets
  ESPNowPacket *packet = this->receive_packet_queue_.pop();
  while (packet != nullptr) {
    switch (packet->type_) {
      case ESPNowPacket::RECEIVED: {
        const ESPNowRecvInfo info = packet->get_receive_info();
        if (!espnow_is_peer_exist(info.src_addr)) {
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
        if (espnow_is_peer_exist(info.src_addr)) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
          char src_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
          char dst_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
          char hex_buf[format_hex_pretty_size(ESP_NOW_MAX_DATA_LEN)];
          format_mac_addr_upper(info.src_addr, src_buf);
          format_mac_addr_upper(info.des_addr, dst_buf);
          ESP_LOGVV(TAG, "<<< [%s -> %s] %s", src_buf, dst_buf,
                    format_hex_pretty_to(hex_buf, packet->packet_.receive.data, packet->packet_.receive.size));
#endif
          if (memcmp(info.des_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0) {
            for (auto *handler : this->broadcasted_handlers_) {
              if (handler->on_broadcasted(info, packet->packet_.receive.data, packet->packet_.receive.size))
                break;  // If a handler returns true, stop processing further handlers
            }
          } else {
            for (auto *handler : this->received_handlers_) {
              if (handler->on_received(info, packet->packet_.receive.data, packet->packet_.receive.size))
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
        ESP_LOGVV(TAG, ">>> [%s] %s", addr_buf, LOG_STR_ARG(espnow_send_status_to_str(packet->packet_.sent.status)));
#endif
        if (this->current_send_packet_ != nullptr) {
          this->current_send_packet_->callback_(packet->packet_.sent.status);
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
    ESP_LOGW(TAG, "Dropped %u received packets due to buffer overflow", received_dropped);
  }

  // Log dropped send packets periodically
  uint16_t send_dropped = this->send_packet_queue_.get_and_reset_dropped_count();
  if (send_dropped > 0) {
    ESP_LOGW(TAG, "Dropped %u send packets due to buffer overflow", send_dropped);
  }
}

uint8_t ESPNowComponent::get_wifi_channel() {
#if defined(USE_ESP32)
  wifi_second_chan_t dummy;
  esp_wifi_get_channel(&this->wifi_channel_, &dummy);
#else
  this->wifi_channel_ = espnow_esp8266::get_wifi_channel();
#endif
  return this->wifi_channel_;
}

espnow_err_t ESPNowComponent::send(const uint8_t *peer_address, const uint8_t *payload, size_t size,
                                   const send_callback_t &callback) {
  if (this->state_ != ESPNOW_STATE_ENABLED) {
    return ESPNOW_ERR_NOT_INIT;
  } else if (this->is_failed()) {
    return ESPNOW_ERR_FAILED;
  } else if (peer_address == nullptr) {
    return ESPNOW_ERR_PEER_NOT_SET;
  } else if (memcmp(peer_address, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    return ESPNOW_ERR_OWN_ADDRESS;
  } else if (size > ESP_NOW_MAX_DATA_LEN) {
    return ESPNOW_ERR_DATA_SIZE;
  } else if (!espnow_is_peer_exist(peer_address)) {
    if (memcmp(peer_address, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0 || this->auto_add_peer_) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
      char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(peer_address, peer_buf);
      ESP_LOGVV(TAG, "Peer %s not in table, adding dynamically (auto_add=%s)", peer_buf, YESNO(this->auto_add_peer_));
#endif
      espnow_err_t err = this->add_peer(peer_address);
      if (err != ESPNOW_OK) {
        return err;
      }
    } else {
      return ESPNOW_ERR_PEER_NOT_PAIRED;
    }
  }
  // Allocate a packet from the pool
  ESPNowSendPacket *packet = this->send_packet_pool_.allocate();
  if (packet == nullptr) {
    this->send_packet_queue_.increment_dropped_count();
    ESP_LOGE(TAG, "Failed to allocate send packet from pool");
    this->status_momentary_warning("send-packet-pool-full");
    return ESPNOW_ERR_NO_MEM;
  }
  // Load the packet data
  packet->load_data(peer_address, payload, size, callback);
  // Push the packet to the send queue
  if (!this->send_packet_queue_.push(packet)) {
    this->send_packet_queue_.increment_dropped_count();
    ESP_LOGE(TAG, "Failed to enqueue send packet (send queue full)");
    this->status_momentary_warning("send-queue-full");
    this->send_packet_pool_.release(packet);
    return ESPNOW_ERR_NO_MEM;
  }
  return ESPNOW_OK;
}

void ESPNowComponent::send_() {
  ESPNowSendPacket *packet = this->send_packet_queue_.pop();
  if (packet == nullptr) {
    return;  // No packets to send
  }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(packet->address_, addr_buf);
  ESP_LOGVV(TAG, "Sending packet to %s size=%u ch=%u peer_exists=%s", addr_buf, packet->size_, this->wifi_channel_,
            YESNO(espnow_is_peer_exist(packet->address_)));
#endif

  this->current_send_packet_ = packet;
  espnow_err_t err = esp_now_send(packet->address_, packet->data_, packet->size_);
  if (err != ESPNOW_OK) {
    char addr_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    format_mac_addr_upper(packet->address_, addr_buf);
    ESP_LOGE(TAG, "Failed to send packet to %s - %s", addr_buf, LOG_STR_ARG(espnow_error_to_str(err)));
    if (packet->callback_ != nullptr) {
      packet->callback_(err);
    }
    this->status_momentary_warning("send-failed");
    this->send_packet_pool_.release(packet);
    this->current_send_packet_ = nullptr;  // Reset current packet
    return;
  }
}

espnow_err_t ESPNowComponent::add_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESPNOW_ERR_NOT_INIT;
  }

  if (memcmp(peer, this->own_address_, ESP_NOW_ETH_ALEN) == 0) {
    this->status_momentary_warning("peer-add-failed");
    return ESPNOW_ERR_INVALID_MAC;
  }

  if (!espnow_is_peer_exist(peer)) {
#if defined(USE_ESP32)
    esp_now_peer_info_t peer_info = {};
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    peer_info.ifidx = WIFI_IF_STA;
    memcpy(peer_info.peer_addr, peer, ESP_NOW_ETH_ALEN);
    espnow_err_t err = esp_now_add_peer(&peer_info);
#else
    espnow_err_t err = espnow_esp8266::add_peer(peer);
#endif

    if (err != ESPNOW_OK) {
      char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(peer, peer_buf);
      ESP_LOGE(TAG, "Failed to add peer %s - %s", peer_buf, LOG_STR_ARG(espnow_error_to_str(err)));
      this->status_momentary_warning("peer-add-failed");
      return err;
    }

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
    char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
    format_mac_addr_upper(peer, peer_buf);
    ESP_LOGVV(TAG, "Added peer %s on channel %u", peer_buf, this->wifi_channel_);
#endif
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

  return ESPNOW_OK;
}

espnow_err_t ESPNowComponent::del_peer(const uint8_t *peer) {
  if (this->state_ != ESPNOW_STATE_ENABLED || this->is_failed()) {
    return ESPNOW_ERR_NOT_INIT;
  }
  if (espnow_is_peer_exist(peer)) {
#if defined(USE_ESP32)
    espnow_err_t err = esp_now_del_peer(peer);
#else
    espnow_err_t err = espnow_esp8266::del_peer(peer);
#endif
    if (err != ESPNOW_OK) {
      char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
      format_mac_addr_upper(peer, peer_buf);
      ESP_LOGE(TAG, "Failed to delete peer %s - %s", peer_buf, LOG_STR_ARG(espnow_error_to_str(err)));
      this->status_momentary_warning("peer-del-failed");
      return err;
    }
  }
  for (auto it = this->peers_.begin(); it != this->peers_.end(); ++it) {
    if (*it == peer) {
      this->peers_.erase(it);
      break;
    }
  }
  return ESPNOW_OK;
}

}  // namespace esphome::espnow

#endif  // USE_ESP32 || ESP8266
