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

namespace esphome {
namespace espnow {

static const char *const TAG = "espnow";

static const size_t SEND_BUFFER_SIZE = 200;

static void application_task(void *param) {
  // delegate onto the application
  ESPNowComponent *application = (ESPNowComponent *) param;
  application->runner();
}

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 1)
struct {
  uint16_t frame_head;
  uint16_t duration;
  uint8_t destination_address[6];
  uint8_t source_address[6];
  uint8_t broadcast_address[6];
  uint16_t sequence_control;

  uint8_t category_code;
  uint8_t organization_identifier[3];  // 0x18fe34
  uint8_t random_values[4];
  struct {
    uint8_t element_id;                  // 0xdd
    uint8_t lenght;                      //
    uint8_t organization_identifier[3];  // 0x18fe34
    uint8_t type;                        // 4
    uint8_t version;
    uint8_t body[0];
  } vendor_specific_content;
} __attribute__((packed)) espnow_frame_format_t;
#endif

/* ESPNowPacket ********************************************************************** */

ESPNowPacket::ESPNowPacket(espnow_addr_t peer, const uint8_t *data, uint8_t size, uint32_t protocol) : ESPNowPacket() {
  this->peer(&peer);

  this->is_broadcast_ = memcpr(&this->peer(), &ESPNOW_BROADCAST_ADDR, 6);
  this->protocol(protocol);
  this->content()->put_bytes(data, size);

  this->recalc();
}

ESPNowPacket::ESPNowPacket(espnow_addr_t *peer, const uint8_t *data, uint8_t size) : ESPNowPacket() {
  this->peer(peer);
  this->content_->set_position(0);
  this->content_->put_bytes(data, size);
}

bool ESPNowPacket::is_valid() {
  uint16_t crc = this->crc();
  this->calc_crc();
  bool valid = (std::memcmp(this->header(), &TRANSPORT_HEADER, 3) == 0);
  valid &= (this->protocol() != 0);
  valid &= (this->crc() == crc);
  this->crc(crc);
  return valid;
}

/* ESPNowProtocol ********************************************************************** */

bool ESPNowProtocol::write(uint64_t mac_address, const uint8_t *data, uint8_t len) {
  ESPNowPacket *packet = new ESPNowPacket(mac_address, data, len, this->get_protocol_id());
  return this->parent_->write(packet);
}
bool ESPNowProtocol::write(uint64_t mac_address, std::vector<uint8_t> &data) {
  ESPNowPacket *packet =
      new ESPNowPacket(mac_address, (uint8_t *) data.data(), (uint8_t) data.size(), this->get_protocol_id());
  return this->parent_->write(packet);
}
bool ESPNowProtocol::write(ESPNowPacket *packet) {
  packet->protocol(this->get_protocol_id());
  packet->packet_id(this->get_next_ref_id());
  packet->recalc();
  return this->parent_->write(packet);
}

ESPNowComponent::ESPNowComponent() { global_esp_now = this; }

void ESPNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "esp_now:");
  ESP_LOGCONFIG(TAG, "  MAC Address: " MACSTR, MAC2STR(ESPNOW_ADDR_SELF));
  ESPNowPacket *packet = new ESPNowPacket(0x112233445566, (uint8_t *) TAG, 5, 0x111111);
  ESP_LOGI(TAG, "test: %s |H:%02x%02x%02x  A:%02x%02x%02x %02x  T:%02x  C:%02x%02x S:%02d", packet->content_bytes(),
           packet->content(0), packet->content(1), packet->content(2), packet->content(3), packet->content(4),
           packet->content(5), packet->content(6), packet->content(7), packet->content(8), packet->content(9),
           packet->size());

  ESP_LOGI(TAG, "test: A:%06x  R:%02x  C:%04x S:%02d", packet->app_id(), packet->packet_id(), packet->crc16(),
           packet->size());
  ESP_LOGI(TAG, "test: is_valid: %s",
           packet->is_valid() ? "Yes" : "No");  // ESP_LOGCONFIG(TAG, "  WiFi Channel: %n", WiFi.channel());
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

#ifdef USE_WIFI
  wifi::global_wifi_component->disable();
#else  // Set device as a Wi-Fi Station
  esp_event_loop_create_default();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_start());
  ESP_ERROR_CHECK(esp_wifi_disconnect());

#endif
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

#ifdef CONFIG_ESPNOW_ENABLE_LONG_RANGE
  esp_wifi_get_protocol(ESP_IF_WIFI_STA, WIFI_PROTOCOL_LR);
#endif

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(this->wifi_channel_, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

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

  esp_wifi_get_mac(WIFI_IF_STA, ESPNOW_ADDR_SELF);

  for (auto &address : this->peers_) {
    ESP_LOGI(TAG, "Add peer 0x%s .", format_hex(address).c_str());
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
    uint8_t mac[6];
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
  if (this->protocols_[ESPNOW_DEFAULT_APP_ID] == nullptr) {
    ESPNowDefaultProtocol *tmp = new ESPNowDefaultProtocol();
    this->register_protocol(tmp);
  }
  return (ESPNowDefaultProtocol *) this->protocols_[ESPNOW_DEFAULT_APP_ID];
}

ESPNowProtocol *ESPNowComponent::get_protocol_(uint32_t protocol) {
  if (this->protocols_[protocol] == nullptr) {
    ESP_LOGE(TAG, "Protocol for '%06x' is not registered", protocol);
    return nullptr;
  }
  return this->protocols_[protocol];
}
void ESPNowComponent::on_receive_(ESPNowPacket *packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->protocol());
  if (protocol != nullptr) {
    protocol->on_receive(packet);
  }
}

void ESPNowComponent::on_sent_(ESPNowPacket *packet, bool status) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->protocol());
  if (protocol != nullptr) {
    protocol->on_sent(packet, status);
  }
}

void ESPNowComponent::on_new_peer_(ESPNowPacket *packet) {
  ESPNowProtocol *protocol = this->get_protocol_(packet->protocol());
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
  ESPNowPacket *packet = new ESPNowPacket(espnow_addr_t(*addr), data, size);
  wifi_pkt_rx_ctrl_t *rx_ctrl = nullptr;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  uint8_t *addr = recv_info->src_addr;
  packet->broadcast(*recv_info->des_addr == ESPNOW_BROADCAST_ADDR);
  rx_ctrl = recv_info->rx_ctrl;
#else
  wifi_promiscuous_pkt_t *promiscuous_pkt =
      (wifi_promiscuous_pkt_t *) (data - sizeof(wifi_pkt_rx_ctrl_t) - sizeof(espnow_frame_format_t));
  rx_ctrl = &promiscuous_pkt->rx_ctrl;
#endif
  packet->rssi(rx_ctrl->rssi);
  packet->timestamp(rx_ctrl->timestamp);
  ESP_LOGVV(TAG, "Read: %s |H:%02x%02x%02x  A:%02x%02x%02x %02x  T:%02x  C:%02x%02x S:%02d", packet->content_bytes(),
            packet->content(0), packet->content(1), packet->content(2), packet->content(3), packet->content(4),
            packet->content(5), packet->content(6), packet->content(7), packet->content(8), packet->content(9),
            packet->size);

  if (packet->is_valid()) {
    xQueueSendToBack(global_esp_now->receive_queue_, packet, 10);
  } else {
    ESP_LOGE(TAG, "Invalid ESP-NOW packet received (CRC)");
  }
}

bool ESPNowComponent::write(ESPNowPacket *packet) {
  ESP_LOGVV(TAG, "Write: %s |H:%02x%02x%02x  A:%02x%02x%02x %02x  T:%02x  C:%02x%02x S:%02d", packet->content_bytes(),
            packet->content(0), packet->content(1), packet->content(2), packet->content(3), packet->content(4),
            packet->content(5), packet->content(6), packet->content(7), packet->content(8), packet->content(9),
            packet->size);
  espnow_addr_t *mac;
  mac = packet->peer();
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Cannot send espnow packet, espnow failed to setup");
  } else if (this->send_queue_full()) {
    ESP_LOGE(TAG, "Send Buffer Out of Memory.");
  } else if (!esp_now_is_peer_exist((uint8_t *) mac)) {
    ESP_LOGW(TAG, "Peer not registered: 0x%12x.", (uint64_t) *mac);
  } else if (!packet->is_valid()) {
    ESP_LOGW(TAG, "Packet is invalid. maybe you need to ::recalc(). the packat before writing.");
  } else if (this->use_sent_check_) {
    xQueueSendToBack(this->send_queue_, packet, 10);
    ESP_LOGVV(TAG, "Send (0x%04x.%d): 0x%s. Buffer Used: %d", packet->packet_id, packet->retrys(),
              format_hex(packet->peer()).c_str(), this->send_queue_used());
    return true;
  } else {
    esp_err_t err = esp_now_send((uint8_t *) &mac, packet->content_bytes(), packet->size());
    ESP_LOGVV(TAG, "S: 0x%04x.%d B: %d%s.", packet->packet_id(), packet->retrys(), this->send_queue_used(),
              (err == ESP_OK) ? "" : " FAILED");
    this->defer([this, packet, err]() { this->on_sent_(packet, err == ESP_OK); });
  }
  return false;
}

void ESPNowComponent::loop() {
  // runner();
}

void ESPNowComponent::runner() {
  ESPNowPacket *packet;

  for (;;) {
    if (xQueueReceive(this->receive_queue_, packet, (TickType_t) 1) == pdTRUE) {
      espnow_addr_t *mac;
      mac = packet->peer();

      if (!esp_now_is_peer_exist((uint8_t *) mac)) {
        if (this->auto_add_peer_) {
          this->add_peer(mac);
        } else {
          this->defer([this, packet]() { this->on_new_peer_(packet); });
          continue;
        }
      }
      if (esp_now_is_peer_exist((uint8_t *) &mac)) {
        this->defer([this, packet]() { this->on_receive_(packet); });
      } else {
        ESP_LOGE(TAG, "Peer not registered: %s", format_hex(packet->mac64).c_str());
      }
    }
    if (xQueueReceive(this->send_queue_, &packet, (TickType_t) 1) == pdTRUE) {
      if (this->is_locked()) {
        if (millis() - packet->timestamp > 1000) {
          if (packet->retrys() == 6) {
            ESP_LOGW(TAG, "To many send retries. Packet dropped. 0x%04x", packet->packet_id());
            this->unlock();
            continue;
          } else {
            ESP_LOGE(TAG, "TimeOut (0x%04x.%d).", packet->packet_id(), packet->retrys());
            packet->retry();
          }
          this->unlock();
        }
      } else {
        packet->retry();
        if (packet->retrys() == 6) {
          ESP_LOGW(TAG, "To many send retries. Packet dropped. 0x%04x", packet->packet_id());
          // continue;
          return;
        } else {
          packet->timestamp = millis();
          this->lock();
          espnow_addr_t mac;
          packet->get_mac(&mac);

          esp_err_t err = esp_now_send((uint8_t *) &mac, packet->data, packet->size + 10);

          if (err == ESP_OK) {
            ESP_LOGV(TAG, "S: 0x%04x.%d. Wait for conformation. M: %s", packet->packet_id(), packet->retrys(),
                     packet->to_str().c_str());
          } else {
            ESP_LOGE(TAG, "S: 0x%04x.%d B: %d.", packet->packet_id(), packet->retrys(), this->send_queue_used());
            this->unlock();
          }
        }
      }
      xQueueSendToFront(this->send_queue_, &packet, 10 / portTICK_PERIOD_MS);
    }
  }
}

void ESPNowComponent::on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  ESPNowPacket *packet;
  if (!global_esp_now->use_sent_check_) {
    return;
  }
  uint64_t mac64 = packet->to_mac64((espnow_addr_t *) mac_addr);
  if (xQueueReceive(global_esp_now->send_queue_, &packet, 10 / portTICK_PERIOD_MS) == pdTRUE) {
    if (status != ESP_OK) {
      ESP_LOGE(TAG, "sent packet failed (0x%04x.%d)", packet->packet_id(), packet->retrys());
    } else if (packet->mac64 != mac64) {
      ESP_LOGE(TAG, " Invalid mac address. (0x%04x.%d) expected: %s got %s", packet->packet_id(), packet->retrys,
               packet->to_str().c_str(), packet->to_str(mac64).c_str());
    } else {
      ESP_LOGV(TAG, "Confirm sent (0x%04x.%d)", packet->packet_id(), packet->retrys());
      global_esp_now->unlock();
      global_esp_now->defer([packet]() { global_esp_now->on_sent_(packet, true); });
      return;
    }
    global_esp_now->defer([packet]() { global_esp_now->on_sent_(packet, false); });
    xQueueSendToFront(global_esp_now->send_queue_, &packet, 10 / portTICK_PERIOD_MS);
  }
  global_esp_now->unlock();
}

ESPNowComponent *global_esp_now = nullptr;

}  // namespace espnow
}  // namespace esphome

#endif
