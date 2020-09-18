
#include <utility>
#include <algorithm>
#include <map>
#include <queue>

#ifdef ARDUINO_ARCH_ESP8266
#include <user_interface.h>
#include <espnow.h>
#include <ESP8266WiFi.h>
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFiType.h>
#include <WiFi.h>
#endif

#include "esphome/core/log.h"

#include "component.h"

namespace esphome {
namespace wifi_now {

static const char *TAG = "wifi_now.component";

static uint32_t sequenceno_;
static std::queue<WifiNowPacket> receivequeue_;
static std::map<const bssid_t, std::queue<std::function<void(bool)>>> sendresponsecallbackqueue_;
static std::queue<std::function<void()>> sendcallbackqueue_;
static std::vector<std::function<bool(WifiNowPacket &)>> receivecallbacks_;

static void send_internal(const WifiNowPacket &packet, std::function<void(bool)> &&callback);
#ifdef ARDUINO_ARCH_ESP8266
static void ICACHE_FLASH_ATTR receivecallback(uint8_t *bssid, uint8_t *data, uint8_t len);
static void ICACHE_FLASH_ATTR sendcallback(uint8_t *bssid, uint8_t status);
#endif
#ifdef ARDUINO_ARCH_ESP32
static void ICACHE_FLASH_ATTR receivecallback(const uint8_t *bssid, const uint8_t *data, int len);
static void ICACHE_FLASH_ATTR sendcallback(const uint8_t *bssid, esp_now_send_status_t status);
#endif

WifiNowComponent::WifiNowComponent() {}

float WifiNowComponent::get_setup_priority() const { return setup_priority::WIFI + 1; }

void WifiNowComponent::setup() {
  ESP_LOGV(TAG, "enter setup()");

#ifdef ARDUINO_ARCH_ESP8266

  WiFiMode_t mode = WiFi.getMode();
  if ((mode & WIFI_STA) != WIFI_STA) {
    ESP_LOGI(TAG, "Initializing Wifi STA...");

    if (!WiFi.mode((WiFiMode_t)(mode | WIFI_STA))) {
      ESP_LOGE(TAG, "WiFi.mode(...) failed");
      return;
    }

    if (channel_ > 0) {
      wifi_promiscuous_enable(true);
      if (!wifi_set_channel(channel_)) {
        ESP_LOGE(TAG, "wifi_set_channel(...) failed");
        return;
      }
      wifi_promiscuous_enable(false);
    }
  }

  if (esp_now_init()) {
    ESP_LOGE(TAG, "esp_now_init() failed");
    return;
  }
  if (esp_now_register_recv_cb(receivecallback)) {
    ESP_LOGE(TAG, "esp_now_register_recv_cb(...) failed");
    return;
  }
  if (esp_now_register_send_cb(sendcallback)) {
    ESP_LOGE(TAG, "esp_now_register_send_cb(...) failed");
    return;
  }
  if (esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER)) {
    ESP_LOGE(TAG, "esp_now_set_self_role(...) failed()");
    return;
  }
  if (aeskey_) {
    auto aeskey = aeskey_->data();
    if (esp_now_set_kok(aeskey, 16)) {
      ESP_LOGE(TAG, "esp_now_set_kok(...) failed");
      return;
    }
  }
  for (auto peer : this->peers_) {
    auto bssid = (uint8_t *) peer->get_bssid().data();
    if (peer->get_aeskey()) {
      auto aeskey = (uint8_t *) peer->get_aeskey()->data();
      if (esp_now_add_peer(bssid, ESP_NOW_ROLE_CONTROLLER, 0, aeskey, 16)) {
        ESP_LOGE(TAG, "esp_now_add_peer(...) failed");
        return;
      }
    } else {
      if (esp_now_add_peer(bssid, ESP_NOW_ROLE_CONTROLLER, 0, nullptr, 16)) {
        ESP_LOGE(TAG, "esp_now_add_peer(...) failed");
        return;
      }
    }
  }
#endif
#ifdef ARDUINO_ARCH_ESP32
  esp_err_t error;

  wifi_mode_t mode = WiFi.getMode();
  if ((mode & WIFI_MODE_STA) != WIFI_MODE_STA) {
    ESP_LOGI(TAG, "Initializing Wifi STA...");

    if (!WiFi.mode(WIFI_MODE_STA)) {
      ESP_LOGE(TAG, "WiFi.mode(...) failed");
      return;
    }

    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    if (channel_ > 0) {
      ESP_ERROR_CHECK(esp_wifi_set_channel(channel_, WIFI_SECOND_CHAN_NONE));
    }
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(false));
  }

  ESP_ERROR_CHECK(esp_now_init());
  ESP_ERROR_CHECK(esp_now_register_recv_cb(receivecallback));
  ESP_ERROR_CHECK(esp_now_register_send_cb(sendcallback));
  if (aeskey_) {
    auto aeskey = aeskey_->data();
    ESP_ERROR_CHECK(esp_now_set_pmk(aeskey));
  }

  for (auto peer : this->peers_) {
    esp_now_peer_info_t info;
    std::fill((uint8_t *) &info, ((uint8_t *) &info) + sizeof(info), 0);
    std::move(peer->get_bssid().cbegin(), peer->get_bssid().cend(), info.peer_addr);
    info.channel = 0;
    info.ifidx = WIFI_IF_STA;
    if (peer->get_aeskey()) {
      std::move(peer->get_aeskey()->cbegin(), peer->get_aeskey()->cend(), info.lmk);
      info.encrypt = true;
    }
    ESP_ERROR_CHECK(esp_now_add_peer(&info));
  }
#endif

  ESP_LOGV(TAG, "exit setup()");
}

void WifiNowComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "WiFi Now:");

  uint8_t sta_bssid[6];

#ifdef ARDUINO_ARCH_ESP8266
  WiFi.macAddress(sta_bssid);
#endif
#ifdef ARDUINO_ARCH_ESP32
  esp_err_t error;

  error = esp_wifi_get_mac(WIFI_IF_STA, sta_bssid);
  if (error != ESP_OK) {
    ESP_LOGE(TAG, "esp_wifi_get_mac(...) failed (%i)", error);
  }

#endif

  if (channel_ > 0) {
    ESP_LOGCONFIG(TAG, "  Channel: %i", channel_);
  }
  ESP_LOGCONFIG(TAG, "  BSSID: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X"), sta_bssid[0], sta_bssid[1], sta_bssid[2],
                sta_bssid[3], sta_bssid[4], sta_bssid[5]);
  if (this->aeskey_) {
    auto aeskey = this->aeskey_->data();

    ESP_LOGCONFIG(
        TAG, "  AESKEY: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X"),
        aeskey[0], aeskey[1], aeskey[2], aeskey[3], aeskey[4], aeskey[5], aeskey[6], aeskey[7], aeskey[8], aeskey[9],
        aeskey[10], aeskey[11], aeskey[12], aeskey[13], aeskey[14], aeskey[15]);
  }
  ESP_LOGCONFIG(TAG, "  Peers:");
  for (auto peer : this->peers_) {
    auto bssid = peer->get_bssid().data();
    ESP_LOGCONFIG(TAG, "  - BSSID: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X"), bssid[0], bssid[1], bssid[2],
                  bssid[3], bssid[4], bssid[5]);
    if (peer->get_aeskey()) {
      auto aeskey = peer->get_aeskey()->data();

      ESP_LOGCONFIG(
          TAG,
          "    AESKEY: " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X"),
          aeskey[0], aeskey[1], aeskey[2], aeskey[3], aeskey[4], aeskey[5], aeskey[6], aeskey[7], aeskey[8], aeskey[9],
          aeskey[10], aeskey[11], aeskey[12], aeskey[13], aeskey[14], aeskey[15]);
    }
  }
}

float WifiNowComponent::get_loop_priority() const { return 10.0f; }

void WifiNowComponent::loop() {
  ESP_LOGVV(TAG, "enter loop()");

  while (!sendcallbackqueue_.empty()) {
    sendcallbackqueue_.front()();
    sendcallbackqueue_.pop();
  }

  while (!receivequeue_.empty()) {
    bool dispatched = false;
    for (auto &callback : receivecallbacks_) {
      dispatched |= callback(receivequeue_.front());
    }
    if (!dispatched) {
      auto &bssid = receivequeue_.front().get_bssid();
      auto &servicekey = receivequeue_.front().get_servicekey();
      ESP_LOGE(TAG,
               "Packet lost, no dispatcher, bssid = " LOG_SECRET(
                   "%02X:%02X:%02X:%02X:%02X:%02X") ", servicekey = " LOG_SECRET("%02X:%02X:%02X:%02X:%02X:%02X:%02X:%"
                                                                                 "02X") "payload size = %lu",
               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5], servicekey[0], servicekey[1], servicekey[2],
               servicekey[3], servicekey[4], servicekey[5], servicekey[6], servicekey[7],
               receivequeue_.front().get_payload().size());
    }
    receivequeue_.pop();
  }

  ESP_LOGVV(TAG, "exit loop()");
}

void WifiNowComponent::set_channel(uint8_t channel) { channel_ = channel; }
uint8_t WifiNowComponent::get_channel() const { return channel_; }

void WifiNowComponent::set_aeskey(const aeskey_t &aeskey) { this->aeskey_ = aeskey; }

const optional<aeskey_t> &WifiNowComponent::get_aeskey() const { return this->aeskey_; }

void WifiNowComponent::set_peer(WifiNowPeer *peer) {
  this->peers_.clear();
  this->add_peer(peer);
}

void WifiNowComponent::add_peer(WifiNowPeer *peer) { this->peers_.push_back(peer); }

const std::vector<WifiNowPeer *> &WifiNowComponent::get_peers() const { return this->peers_; }

void WifiNowComponent::send(const WifiNowPacket &packet, std::function<void(bool)> &&callback) {
  // HERE WE CAN DOO ALL THE STUFF NEEDED INCLUDING THE "LOCK" signature bla...
  // It has to be asyncrounous -> use settimeout and co

  send_internal(packet, std::move(callback));
}

void WifiNowComponent::register_receive_callback(std::function<bool(WifiNowPacket &)> &&callback) {
  receivecallbacks_.push_back(std::move(callback));
}

void WifiNowComponent::register_priorized_receive_callback(std::function<bool(WifiNowPacket &)> &&callback) {
  receivecallbacks_.insert(receivecallbacks_.begin(), std::move(callback));
}

static void send_internal(const WifiNowPacket &packet, std::function<void(bool)> &&callback) {
  ESP_LOGV(TAG, "enter send_internal()");

  auto &queue = sendresponsecallbackqueue_[packet.get_bssid()];
  queue.emplace(callback);

  uint8_t *bssid = nullptr;
  if (packet.get_bssid() != bssid_t()) {
    bssid = (uint8_t *) packet.get_bssid().data();
  }

#ifdef ARDUINO_ARCH_ESP8266
  if (esp_now_send(bssid, (uint8_t *) packet.get_packetdata().data(), packet.get_packetdata().size())) {
    ESP_LOGE(TAG, "esp_now_send(...) failed");
    queue.pop();
  }
#endif
#ifdef ARDUINO_ARCH_ESP32
  esp_err_t error;

  error = esp_now_send(bssid, (uint8_t *) packet.get_packetdata().data(), packet.get_packetdata().size());
  if (error != ESP_OK) {
    queue.pop();
  }
  ESP_ERROR_CHECK(error)
#endif

  ESP_LOGV(TAG, "exit send_internal()");
}

#ifdef ARDUINO_ARCH_ESP8266
void ICACHE_FLASH_ATTR receivecallback(uint8_t *bssid, uint8_t *data, uint8_t len)
#endif
#ifdef ARDUINO_ARCH_ESP32
    void ICACHE_FLASH_ATTR receivecallback(const uint8_t *bssid, const uint8_t *data, int len)
#endif
{
  receivequeue_.emplace(bssid, data, len);
}

#ifdef ARDUINO_ARCH_ESP8266
void ICACHE_FLASH_ATTR sendcallback(uint8_t *bssid, uint8_t status)
#endif
#ifdef ARDUINO_ARCH_ESP32
    void ICACHE_FLASH_ATTR sendcallback(const uint8_t *bssid, esp_now_send_status_t status)
#endif
{
  bssid_t key;
  if (bssid != nullptr) {
    std::copy_n(bssid, key.size(), key.begin());
  }

  auto &queue = sendresponsecallbackqueue_[key];
  if (!queue.empty()) {
    if (queue.front()) {
      auto callback = queue.front();
      sendcallbackqueue_.emplace([=]() { callback(status == 0 /*MT_TX_STATUS_OK or ESP_NOW_SEND_SUCCESS*/); });
    }
    queue.pop();
  }
}

}  // namespace wifi_now
}  // namespace esphome
