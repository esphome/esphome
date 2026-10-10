#pragma once

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "espnow_err.h"
#include "espnow_types.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace esphome::espnow {

static const uint8_t ESPNOW_BROADCAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t ESPNOW_MULTICAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};

// Maximum payload this component sends and receives, from the
// ``max_payload_size`` option. The radio stack speaks ESP-NOW v2 regardless
// (negotiated per peer); payloads beyond the v1 limit (250 bytes) are opt-in
// because the packet pools are statically sized from this, so their RAM cost
// is proportional (~8 KB at 250 bytes, ~44 KB at the v2 limit of 1470).
#ifndef USE_ESPNOW_MAX_PAYLOAD_SIZE
#define USE_ESPNOW_MAX_PAYLOAD_SIZE ESP_NOW_MAX_DATA_LEN
#endif
static constexpr uint16_t ESPNOW_MAX_DATA_LEN = USE_ESPNOW_MAX_PAYLOAD_SIZE;
#ifdef ESP_NOW_MAX_DATA_LEN_V2
static_assert(ESPNOW_MAX_DATA_LEN <= ESP_NOW_MAX_DATA_LEN_V2,
              "espnow max_payload_size cannot exceed the ESP-NOW v2 frame limit");
#else
static_assert(ESPNOW_MAX_DATA_LEN <= ESP_NOW_MAX_DATA_LEN,
              "espnow max_payload_size beyond 250 bytes requires an ESP-IDF with ESP-NOW v2 support (5.4+)");
#endif

struct ESPNowRecvInfo {
  uint8_t src_addr[ESP_NOW_ETH_ALEN]; /**< Source address of ESPNOW packet */
  uint8_t des_addr[ESP_NOW_ETH_ALEN]; /**< Destination address of ESPNOW packet */
  rx_ctrl_t *rx_ctrl;                 /**< Rx control info of ESPNOW packet */
};

using send_callback_t = std::function<void(esp_err_t)>;

class ESPNowPacket {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum esp_now_packet_type_t : uint8_t {
    RECEIVED,
    SENT,
  };

  // Default constructor for pre-allocation in pool
  ESPNowPacket() {}

  void release() {}

  void load_received_data(const uint8_t *src_addr, const uint8_t *des_addr, const uint8_t *data, uint16_t size,
                          int8_t rssi, uint32_t timestamp) {
    this->type_ = RECEIVED;
    memcpy(this->packet_.receive.info.src_addr, src_addr, ESP_NOW_ETH_ALEN);
    memcpy(this->packet_.receive.info.des_addr, des_addr, ESP_NOW_ETH_ALEN);
    memcpy(this->packet_.receive.data, data, size);
    this->packet_.receive.size = size;

    this->packet_.receive.rx_ctrl.rssi = rssi;
    this->packet_.receive.rx_ctrl.timestamp = timestamp;
    this->packet_.receive.info.rx_ctrl = reinterpret_cast<rx_ctrl_t *>(&this->packet_.receive.rx_ctrl);
  }

  void load_sent_data(const uint8_t *mac_addr, esp_now_send_status_t status) {
    this->type_ = SENT;
    memcpy(this->packet_.sent.address, mac_addr, ESP_NOW_ETH_ALEN);
    this->packet_.sent.status = status;
  }

  // Disable copy to prevent double-delete
  ESPNowPacket(const ESPNowPacket &) = delete;
  ESPNowPacket &operator=(const ESPNowPacket &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct received_data {
      ESPNowRecvInfo info;                // Information about the received packet
      uint8_t data[ESPNOW_MAX_DATA_LEN];  // Data received in the packet
      uint16_t size;                      // Size of the received data
      WifiPacketRxControl rx_ctrl;        // Status of the received packet
    } receive;

    // NOLINTNEXTLINE(readability-identifier-naming)
    struct sent_data {
      uint8_t address[ESP_NOW_ETH_ALEN];
      esp_now_send_status_t status;
    } sent;
  } packet_;

  esp_now_packet_type_t type_;

  esp_now_packet_type_t type() const { return this->type_; }
  const ESPNowRecvInfo &get_receive_info() const { return this->packet_.receive.info; }
};

class ESPNowSendPacket {
 public:
  ESPNowSendPacket(const uint8_t *peer_address, const uint8_t *payload, size_t size, const send_callback_t &&callback)
      : callback_(callback) {
    this->init_data_(peer_address, payload, size);
  }
  ESPNowSendPacket(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    this->init_data_(peer_address, payload, size);
  }

  // Default constructor for pre-allocation in pool
  ESPNowSendPacket() {}

  void release() {}

  // Disable copy to prevent double-delete
  ESPNowSendPacket(const ESPNowSendPacket &) = delete;
  ESPNowSendPacket &operator=(const ESPNowSendPacket &) = delete;

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size, send_callback_t &&callback) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = std::move(callback);
  }

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = nullptr;  // Reset callback
  }

  uint8_t address_[ESP_NOW_ETH_ALEN]{0};  // MAC address of the peer to send the packet to
  uint8_t data_[ESPNOW_MAX_DATA_LEN]{0};  // Data to send
  uint16_t size_{0};                      // Size of the data to send, must be <= ESPNOW_MAX_DATA_LEN
  send_callback_t callback_{nullptr};     // Callback to call when the send operation is complete

 private:
  void init_data_(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    memcpy(this->address_, peer_address, ESP_NOW_ETH_ALEN);
    if (size > ESPNOW_MAX_DATA_LEN) {
      this->size_ = 0;
      return;
    }
    this->size_ = size;
    memcpy(this->data_, payload, this->size_);
  }
};

}  // namespace esphome::espnow

#endif  // USE_ESP32 || USE_ESP8266
