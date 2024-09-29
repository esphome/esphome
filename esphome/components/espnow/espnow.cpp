#include "espnow.h"

#if defined(USE_ESP32)

#include <cstring>

#include "esp_mac.h"
#include "esp_random.h"

#include "esp_wifi.h"
#include "esp_crc.h"
#include "esp_now.h"
#include "esp_event.h"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "esphome/core/application.h"
#include "esphome/core/version.h"
#include "esphome/core/log.h"

#include <memory>

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow";

static const size_t SEND_BUFFER_SIZE = 200;

static void application_task(void *param) {
  // delegate onto the application
  ESPNowComponent *application = (ESPNowComponent *) param;
  application->runner();
}

ESPNowComponent *ESPNowComponent::static_{nullptr};  // NOLINT

/* ESPNowComponent ********************************************************************** */

ESPNowComponent::ESPNowComponent() { ESPNowComponent::static_ = this; }  // NOLINT

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "esp_now:");
  ESP_LOGCONFIG(TAG, "  MAC Address: 0x%12llx.", this->own_peer_address_);
}

void ESPNowComponent::show_packet(const std::string &title, const ESPNowPacket &packet) {
  ESP_LOGVV(TAG, "%s Peer: packet: 0x%12llx  M:%s H:%cx%cx%c  P:%c%c%c 0x%02x  S:%02x  C:ox%02x~0x%02x S:%02d V:%s",
            "test", packet.peer, packet.content_at(0), packet.content_at(1), packet.content_at(2), packet.content_at(3),
            packet.content_at(4), packet.content_at(5), packet.content_at(6), packet.content_at(7), packet.crc(),
            packet.calc_crc(), packet.content_size() packet.is_valid() ? "Yes" : "No");
}

bool ESPNowComponent::validate_channel_(uint8_t channel) {
  wifi_country_t g_self_country;
  esp_wifi_get_country(&g_self_country);
  if (channel >= g_self_country.schan + g_self_country.nchan) {
    ESP_LOGE(TAG, "Can't set channel %d, not allowed in country %c%c%c.", channel, g_self_country.cc[0],
             g_self_country.cc[1], g_self_country.cc[2]);
    return false;
  }
  return true;
}

void ESPNowComponent::setup() {
  ESP_LOGI(TAG, "Setting up ESP-NOW...");

#ifndef USE_WIFI
  esp_event_loop_create_default();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_disconnect());

#ifdef CONFIG_ESPNOW_ENABLE_LONG_RANGE
  esp_wifi_get_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(this->wifi_channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
#else
  // this->wifi_channel_ =  wifi::global_wifi_component->
#endif

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_init failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_now_register_recv_cb(ESPNowComponent::on_data_received);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = esp_now_register_send_cb(ESPNowComponent::on_data_sent);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_now_register_send_cb failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  esp_wifi_get_mac(WIFI_IF_STA, (uint8_t *) &this->own_peer_address_);

  for (auto &address : this->peers_) {
    ESP_LOGI(TAG, "Add peer 0x%12llx.", address);
    add_peer(address);
  }

  this->send_queue_ = xQueueCreate(SEND_BUFFER_SIZE, sizeof(ESPNowPacket));
  if (this->send_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create send queue");
    this->mark_failed();
    return;
  }

  this->receive_queue_ = xQueueCreate(SEND_BUFFER_SIZE, sizeof(ESPNowPacket));
  if (this->receive_queue_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create receive queue");
    this->mark_failed();
    return;
  }
}

void ESPNowComponent::loop() {
  if (!this->task_running_) {
    xTaskCreate(application_task, "espnow_task", 8192, this, 2, &this->espnow_task_handle_);
    this->task_running_ = true;
  }
}

esp_err_t ESPNowComponent::add_peer(uint64_t addr) {
  if (!this->is_ready()) {
    this->peers_.push_back(addr);
    return ESP_OK;
  } else {
    this->del_peer(addr);

    esp_now_peer_info_t peer_info = {};
    memset(&peer_info, 0, sizeof(esp_now_peer_info_t));
    peer_info.channel = this->wifi_channel_;
    peer_info.encrypt = false;
    memcpy((void *) peer_info.peer_addr, (void *) &addr, 6);

    return esp_now_add_peer(&peer_info);
  }
}

esp_err_t ESPNowComponent::del_peer(uint64_t addr) {
  uint8_t mac[6];
  memcpy((void *) &mac, (void *) &addr, 6);
  if (esp_now_is_peer_exist((uint8_t *) &mac))
    return esp_now_del_peer((uint8_t *) &mac);
  return ESP_OK;
}

ESPNowDefaultProtocol *ESPNowComponent::get_default_protocol() {
  if (this->protocols_[ESPNOW_MAIN_PROTOCOL_ID] == nullptr) {
    this->default_protocol_ = new ESPNowDefaultProtocol();  // NOLINT
    this->register_protocol(this->default_protocol_);
  }
  return (ESPNowDefaultProtocol *) this->protocols_[ESPNOW_MAIN_PROTOCOL_ID];
}

ESPNowProtocol *ESPNowComponent::get_protocol_component_(uint32_t protocol) {
  if (this->protocols_[protocol] == nullptr) {
    ESP_LOGE(TAG, "Protocol for '0x%06" PRIx32 "' is not registered", protocol);
    return nullptr;
  }
  return this->protocols_[protocol];
}

void ESPNowComponent::call_on_receive_(ESPNowPacket packet) {
  ESPNowProtocol *protocol = this->get_protocol_component_(packet.protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet]() { protocol->on_receive(packet); });
  }
}

void ESPNowComponent::call_on_sent_(ESPNowPacket packet, bool status) {
  ESPNowProtocol *protocol = this->get_protocol_component_(packet.protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet, status]() { protocol->on_sent(packet, status); });
  }
}

void ESPNowComponent::call_on_new_peer_(ESPNowPacket packet) {
  ESPNowProtocol *protocol = this->get_protocol_component_(packet.protocol());
  if (protocol != nullptr) {
    this->defer([protocol, packet]() { protocol->on_new_peer(packet); });
  }
}

/**< callback function of receiving ESPNOW data */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
void ESPNowComponent::on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size)
#else
void ESPNowComponent::on_data_received(const uint8_t *addr, const uint8_t *data, int size)
#endif
{
  if (ESPNowComponent::static_ == nullptr) {
    return;
  }

  bool broadcast = false;
  wifi_pkt_rx_ctrl_t *rx_ctrl = nullptr;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  uint8_t *addr = recv_info->src_addr;
  broadcast = ((uint64_t) *recv_info->des_addr & ESPNOW_BROADCAST_ADDR) == ESPNOW_BROADCAST_ADDR;
  rx_ctrl = recv_info->rx_ctrl;
#else
  wifi_promiscuous_pkt_t *promiscuous_pkt =
      (wifi_promiscuous_pkt_t *) (data - sizeof(wifi_pkt_rx_ctrl_t) - 39);  // = sizeof (espnow_frame_format_t)
  rx_ctrl = &promiscuous_pkt->rx_ctrl;
#endif
  ESPNowPacket packet((uint64_t) *addr, data, (uint8_t) size);  // NOLINT
  packet.is_broadcast = broadcast;
  if (rx_ctrl != nullptr) {
    packet.rssi = rx_ctrl->rssi;
    packet.timestamp = rx_ctrl->timestamp;
  } else {
    packet.timestamp = millis();
  }
  /// this->show_packet("Receive", *packet);

  if (packet.is_valid()) {
    xQueueSendToBack(ESPNowComponent::static_->receive_queue_, (void *) &packet, 10);
  } else {
    ESP_LOGE(TAG, "Invalid ESP-NOW packet received (CRC)");
  }
}

bool ESPNowComponent::send(ESPNowPacket packet) {
  uint8_t *mac = packet.peer_as_bytes();

  // this->show_packet("Write", packet);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow failed to setup");
  } else if (this->send_queue_full()) {
    ESP_LOGE(TAG, "Send Buffer Out of Memory.");
  } else if (!esp_now_is_peer_exist(mac)) {
    ESP_LOGW(TAG, "Peer not registered: 0x%12llx.", packet.peer);
  } else if (!packet.is_valid()) {
    ESP_LOGW(TAG, "Packet is invalid. maybe you need to ::calc_crc(). the packat before writing.");
  } else if (this->use_sent_check_) {
    ESP_LOGD(TAG, "Place 0x%12llx (%d.%d) into send buffer. Used: %d of %d", packet.peer, packet.sequents(),
             packet.attempts, this->send_queue_used(), SEND_BUFFER_SIZE);
    xQueueSendToBack(this->send_queue_, (void *) &packet, 10);
    return true;
  } else {
    esp_err_t err = esp_now_send((uint8_t *) &mac, packet.content_as_bytes(), packet.content_size());
    ESP_LOGD(TAG, "Sended 0x%12llx (%d.%d) directly%s.", packet.peer, packet.sequents(), packet.attempts,
             (err == ESP_OK) ? "" : " FAILED");

    this->call_on_sent_(packet, err == ESP_OK);
    return true;
  }

  return false;
}

void ESPNowComponent::runner() {
  ESPNowPacket packet;  // NOLINT
  for (;;) {
    if (xQueueReceive(this->receive_queue_, (void *) &packet, (TickType_t) 1) == pdTRUE) {
      uint8_t *mac = packet.peer_as_bytes();
      ESP_LOGD(TAG, "Handling received packet from 0x%12llx (%d.%d).", packet.peer, packet.sequents(), packet.attempts);

      if (esp_now_is_peer_exist(mac)) {
        this->call_on_receive_(packet);
      } else if (this->auto_add_peer_) {
        this->add_peer(packet.peer);
        this->call_on_receive_(packet);
      } else {
        this->call_on_new_peer_(packet);
      }
    }
    if (xQueueReceive(this->send_queue_, (void *) &packet, (TickType_t) 1) == pdTRUE) {
      ESP_LOGD(TAG, "Handling send packet for 0x%12llx (%d.%d).", packet.peer, packet.sequents(), packet.attempts);

      if (packet.attempts > MAX_NUMBER_OF_RETRYS) {
        ESP_LOGE(TAG, "Dropped  0x%12llx (0x%04x.%d). To many send retries", packet.peer, packet.sequents(),
                 packet.attempts);
        this->unlock();
        continue;
      } else if (this->is_locked()) {
        if (millis() - packet.timestamp > 1000) {
          ESP_LOGW(TAG, "TimeOut 0x%12llx (0x%04x.%d).", packet.peer, packet.sequents(), packet.attempts);
          packet.retry();
          this->unlock();
        }
      } else {
        this->lock();
        packet.retry();
        packet.timestamp = millis();
        uint8_t *mac = packet.peer_as_bytes();

        esp_err_t err = esp_now_send(mac, packet.content_as_bytes(), packet.content_size());

        if (err == ESP_OK) {
          ESP_LOGV(TAG, "Sended 0x%12llx (%d.%d). Wait for conformation.", packet.peer, packet.sequents(),
                   packet.attempts);
        } else {
          ESP_LOGE(TAG, "Sending 0x%12llx (%d.%d) FAILED. B: %d.", packet.peer, packet.sequents(), packet.attempts,
                   this->send_queue_used());
          this->unlock();
        }
      }
      xQueueSendToFront(this->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS);
    }
  }
}

void ESPNowComponent::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if ((ESPNowComponent::static_ == nullptr) || !ESPNowComponent::static_->use_sent_check_) {
    return;
  }
  ESPNowPacket packet;  // NOLINT
  uint64_t mac64 = (uint64_t) *mac_addr;
  if (xQueuePeek(ESPNowComponent::static_->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "sent packet failed (0x%04x.%d)", packet.sequents(), packet.attempts);
    } else if (packet.peer != mac64) {
      ESP_LOGE(TAG, " Invalid mac address. (0x%04x.%d) expected: 0x%12llx got: 0x%12llx", packet.sequents(),
               packet.attempts, packet.peer, mac64);
    } else {
      ESP_LOGV(TAG, "Confirm sent (0x%04x.%d)", packet.sequents(), packet.attempts);
      ESPNowComponent::static_->call_on_sent_(packet, true);
      xQueueReceive(ESPNowComponent::static_->send_queue_, (void *) &packet, 10 / portTICK_PERIOD_MS);
      return;
    }
    ESPNowComponent::static_->call_on_sent_(packet, false);
    ESPNowComponent::static_->unlock();
  }
}

/* ESPNowProtocol ********************************************************************** */

bool ESPNowProtocol::send(uint64_t peer, const uint8_t *data, uint8_t len) {
  ESPNowPacket packet(peer, data, len, this->get_protocol_component_id());  // NOLINT
  packet.sequents(this->get_next_sequents());
  ESP_LOGD(TAG, "Created packet to Send for 0x%12llx (%d.%d) directly.", packet.peer, packet.sequents(),
           packet.attempts);

  return this->parent_->send(packet);
}

}  // namespace espnow
}  // namespace esphome

#endif
