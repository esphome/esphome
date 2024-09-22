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

std::string format_mac_addr(const uint8_t *mac) {
  char buf[20];
  sprintf(buf, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return buf;
}
void show_packet(std::string title, ESPNowPacket *packet) {
  ESP_LOGVV(TAG, "%s packet: M:%s H:%cx%cx%c  P:%c%c%c 0x%02x  S:%02x  C:ox%02x~0x%02x S:%02d V:%s", "test",
            format_mac_addr(packet->peer_as_bytes()).c_str(), packet->content_at(0), packet->content_at(1),
            packet->content_at(2), packet->content_at(3), packet->content_at(4), packet->content_at(5),
            packet->content_at(6), packet->content_at(7), packet->crc(), packet->calc_crc(), packet->get_size(),
            packet->is_valid() ? "Yes" : "No");
}

/* ESPNowPacket ********************************************************************** */

ESPNowPacket::ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size, uint32_t protocol) : ESPNowPacket() {
  this->set_peer(peer);

  this->is_broadcast =
      (std::memcmp((const void *) this->peer_as_bytes(), (const void *) &ESPNOW_BROADCAST_ADDR, 6) == 0);
  this->set_protocol(protocol);
  this->payload()->put_bytes(data, size);
  this->update_payload_();
}

ESPNowPacket::ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size) : ESPNowPacket() {
  this->set_peer(peer);
  std::memcpy(&(this->content), data, this->prefix_size());
  size -= this->prefix_size();
  this->payload()->put_bytes(data + this->prefix_size(), size - 1);
  this->update_payload_();
  this->content.payload[this->size] = *(data + size);
}

bool ESPNowPacket::is_valid() {
  uint16_t crc = this->content.payload[this->size];
  bool valid = (memcmp((const void *) &this->content, (const void *) &TRANSPORT_HEADER, 3) == 0);
  valid &= (this->get_protocol() != 0);
  valid &= (this->calc_crc() == crc);

  return valid;
}

/* ESPNowProtocol ********************************************************************** */

bool ESPNowProtocol::write(uint64_t peer, const uint8_t *data, uint8_t len) {
  ESPNowPacket *packet = new ESPNowPacket(peer, data, len, this->get_protocol_id());
  packet->set_sequents(this->get_next_sequents());
  return this->parent_->write(packet);
}

ESPNowComponent::ESPNowComponent() { global_esp_now = this; }

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "esp_now:");
  ESP_LOGCONFIG(TAG, "  MAC Address: 0x%12llx.", ESPNOW_ADDR_SELF);
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

  esp_wifi_get_mac(WIFI_IF_STA, (uint8_t *) &ESPNOW_ADDR_SELF);

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

  xTaskCreate(application_task, "espnow_task", 8192, this, 2, &this->espnow_task_handle_);

  ESP_LOGI(TAG, "ESP-NOW setup complete");
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
    ESPNowDefaultProtocol *tmp = new ESPNowDefaultProtocol();
    this->register_protocol(tmp);
  }
  return (ESPNowDefaultProtocol *) this->protocols_[ESPNOW_MAIN_PROTOCOL_ID];
}

ESPNowProtocol *ESPNowComponent::get_protocol_(uint32_t protocol) {
  if (this->protocols_[protocol] == nullptr) {
    ESP_LOGE(TAG, "Protocol for '0x%06" PRIx32 "' is not registered", protocol);
    return nullptr;
  }
  return this->protocols_[protocol];
}

void ESPNowComponent::on_receive_(ESPNowPacket *packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->get_protocol());
  if (protocol != nullptr) {
    protocol->on_receive(packet);
  }
}

void ESPNowComponent::on_sent_(ESPNowPacket *packet, bool status) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->get_protocol());
  if (protocol != nullptr) {
    protocol->on_sent(packet, status);
  }
}

void ESPNowComponent::on_new_peer_(ESPNowPacket *packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->get_protocol());
  if (protocol != nullptr) {
    protocol->on_new_peer(packet);
  }
}

/**< callback function of receiving ESPNOW data */
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
void ESPNowComponent::on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size)
#else
void ESPNowComponent::on_data_received(const uint8_t *addr, const uint8_t *data, int size)
#endif
{
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
  ESPNowPacket *packet = new ESPNowPacket((uint64_t) *addr, data, (uint8_t) size);
  packet->is_broadcast = broadcast;
  if (rx_ctrl != nullptr) {
    packet->rssi = rx_ctrl->rssi;
    packet->timestamp = rx_ctrl->timestamp;
  } else {
    packet->timestamp = millis();
  }
  show_packet("Receive", packet);

  if (packet->is_valid()) {
    xQueueSendToBack(global_esp_now->receive_queue_, packet, 10);
  } else {
    ESP_LOGE(TAG, "Invalid ESP-NOW packet received (CRC)");
  }
}

bool ESPNowComponent::write(ESPNowPacket *packet) {
  uint8_t *mac = packet->peer_as_bytes();
  show_packet("Write", packet);
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow failed to setup");
  } else if (this->send_queue_full()) {
    ESP_LOGE(TAG, "Send Buffer Out of Memory.");
  } else if (!esp_now_is_peer_exist(mac)) {
    ESP_LOGW(TAG, "Peer not registered: 0x%12llx.", packet->peer);
  } else if (!packet->is_valid()) {
    ESP_LOGW(TAG, "Packet is invalid. maybe you need to ::calc_crc(). the packat before writing.");
  } else if (this->use_sent_check_) {
    xQueueSendToBack(this->send_queue_, packet, 10);
    ESP_LOGVV(TAG, "Send to 0x%12llx (%d.%d): Buffer Used: %d", packet->peer, packet->get_sequents(), packet->attempts,
              this->send_queue_used());

    return true;
  } else {
    esp_err_t err = esp_now_send((uint8_t *) &mac, packet->content_bytes(), packet->get_size());
    ESP_LOGVV(TAG, "S: 0x%04x.%d B: %d%s.", packet->get_sequents(), packet->attempts, this->send_queue_used(),
              (err == ESP_OK) ? "" : " FAILED");
    this->defer([this, packet, err]() { this->on_sent_(packet, err == ESP_OK); });
    return true;
  }

  return false;
}

void ESPNowComponent::runner() {
  ESPNowPacket *packet{nullptr};

  for (;;) {
    delete packet;
    packet = new ESPNowPacket();
    if (xQueueReceive(this->receive_queue_, packet, (TickType_t) 1) == pdTRUE) {
      uint8_t *mac = packet->peer_as_bytes();

      if (!esp_now_is_peer_exist(mac)) {
        if (!this->auto_add_peer_) {
          this->defer([this, packet]() { this->on_new_peer_(packet); });
          continue;
        } else {
          this->add_peer(packet->peer);
        }
      }
      this->defer([this, packet]() { this->on_receive_(packet); });
    }
    delete packet;
    packet = new ESPNowPacket();
    if (xQueueReceive(this->send_queue_, packet, (TickType_t) 1) == pdTRUE) {
      if (packet->attempts > MAX_NUMBER_OF_RETRYS) {
        ESP_LOGW(TAG, "To many send retries. Packet dropped. 0x%04x", packet->get_sequents());
        this->unlock();
        continue;
      } else if (this->is_locked()) {
        if (millis() - packet->timestamp > 1000) {
          ESP_LOGE(TAG, "TimeOut (0x%04x.%d).", packet->get_sequents(), packet->attempts);
          packet->retry();
          this->unlock();
        }
      } else {
        this->lock();
        packet->retry();
        packet->timestamp = millis();
        uint8_t *mac = packet->peer_as_bytes();

        esp_err_t err = esp_now_send(mac, packet->content_bytes(), packet->get_size() + 1);

        if (err == ESP_OK) {
          ESP_LOGV(TAG, "S: 0x%04x.%d. Wait for conformation.", packet->get_sequents(), packet->attempts);
        } else {
          ESP_LOGE(TAG, "S: 0x%04x.%d B: %d.", packet->get_sequents(), packet->attempts, this->send_queue_used());
          this->unlock();
        }
      }
      xQueueSendToFront(this->send_queue_, packet, 10 / portTICK_PERIOD_MS);
    }
  }
}

void ESPNowComponent::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESPNowPacket *packet = new ESPNowPacket();
  if (!global_esp_now->use_sent_check_) {
    return;
  }
  uint64_t mac64 = (uint64_t) *mac_addr;
  if (xQueuePeek(global_esp_now->send_queue_, packet, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "sent packet failed (0x%04x.%d)", packet->get_sequents(), packet->attempts);
    } else if (packet->peer != mac64) {
      ESP_LOGE(TAG, " Invalid mac address. (0x%04x.%d) expected: 0x%12llx got: 0x%12llx", packet->get_sequents(),
               packet->attempts, packet->peer, mac64);
    } else {
      ESP_LOGV(TAG, "Confirm sent (0x%04x.%d)", packet->get_sequents(), packet->attempts);
      global_esp_now->defer([packet]() {
        global_esp_now->on_sent_(packet, true);
        delete packet;
        ESPNowPacket tmp;
        xQueueReceive(global_esp_now->send_queue_, &tmp, 10 / portTICK_PERIOD_MS);

        global_esp_now->unlock();
      });
      return;
    }
    global_esp_now->defer([packet]() {
      global_esp_now->on_sent_(packet, false);
      ESPNowPacket tmp;
      xQueueReceive(global_esp_now->send_queue_, &tmp, 10 / portTICK_PERIOD_MS);
      xQueueSendToFront(global_esp_now->send_queue_, &tmp, 10 / portTICK_PERIOD_MS);

      global_esp_now->unlock();
    });
  }
}

ESPNowComponent *global_esp_now = nullptr;

}  // namespace espnow
}  // namespace esphome

#endif