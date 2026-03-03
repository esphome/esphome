#pragma once

#ifdef USE_ESP32

#include "espnow_err.h"

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <vector>

#include <esp_err.h>
#include <esp_idf_version.h>
#include <esp_now.h>

namespace esphome::espnow {

static const uint8_t ESPNOW_BROADCAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static const uint8_t ESPNOW_MULTICAST_ADDR[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFE};

struct WifiPacketRxControl {
  int8_t rssi;         // Received Signal Strength Indicator (RSSI) of packet, unit: dBm
  uint32_t timestamp;  // Timestamp in microseconds when the packet was received, precise only if modem sleep or
                       // light sleep is not enabled
};

struct ESPNowRecvInfo {
  uint8_t src_addr[ESP_NOW_ETH_ALEN]; /**< Source address of ESPNOW packet */
  uint8_t des_addr[ESP_NOW_ETH_ALEN]; /**< Destination address of ESPNOW packet */
  wifi_pkt_rx_ctrl_t *rx_ctrl;        /**< Rx control info of ESPNOW packet */
};

using send_callback_t = std::function<void(esp_err_t)>;

class ESPNowPacket {
 public:
  // NOLINTNEXTLINE(readability-identifier-naming)
  enum esp_now_packet_type_t : uint8_t {
    RECEIVED,
    SENT,
  };

  // Constructor for received data
  ESPNowPacket(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
    this->init_received_data_(info, data, size);
  };

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  // Constructor for sent data
  ESPNowPacket(const esp_now_send_info_t *info, esp_now_send_status_t status) {
    this->init_sent_data_(info->src_addr, status);
  }
#else
  // Constructor for sent data
  ESPNowPacket(const uint8_t *mac_addr, esp_now_send_status_t status) { this->init_sent_data_(mac_addr, status); }
#endif

  // Default constructor for pre-allocation in pool
  ESPNowPacket() {}

  void release() {}

  void load_received_data(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
    this->type_ = RECEIVED;
    this->init_received_data_(info, data, size);
  }

  void load_sent_data(const uint8_t *mac_addr, esp_now_send_status_t status) {
    this->type_ = SENT;
    this->init_sent_data_(mac_addr, status);
  }

  // Disable copy to prevent double-delete
  ESPNowPacket(const ESPNowPacket &) = delete;
  ESPNowPacket &operator=(const ESPNowPacket &) = delete;

  union {
    // NOLINTNEXTLINE(readability-identifier-naming)
    struct received_data {
      ESPNowRecvInfo info;                 // Information about the received packet
      uint8_t data[ESP_NOW_MAX_DATA_LEN];  // Data received in the packet
      uint8_t size;                        // Size of the received data
      WifiPacketRxControl rx_ctrl;         // Status of the received packet
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

 private:
  void init_received_data_(const esp_now_recv_info_t *info, const uint8_t *data, int size) {
    memcpy(this->packet_.receive.info.src_addr, info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(this->packet_.receive.info.des_addr, info->des_addr, ESP_NOW_ETH_ALEN);
    memcpy(this->packet_.receive.data, data, size);
    this->packet_.receive.size = size;

    this->packet_.receive.rx_ctrl.rssi = info->rx_ctrl->rssi;
    this->packet_.receive.rx_ctrl.timestamp = info->rx_ctrl->timestamp;

    this->packet_.receive.info.rx_ctrl = reinterpret_cast<wifi_pkt_rx_ctrl_t *>(&this->packet_.receive.rx_ctrl);
  }

  void init_sent_data_(const uint8_t *mac_addr, esp_now_send_status_t status) {
    memcpy(this->packet_.sent.address, mac_addr, ESP_NOW_ETH_ALEN);
    this->packet_.sent.status = status;
  }
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

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size, const send_callback_t &callback) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = callback;
  }

  void load_data(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    this->init_data_(peer_address, payload, size);
    this->callback_ = nullptr;  // Reset callback
  }

  uint8_t address_[ESP_NOW_ETH_ALEN]{0};   // MAC address of the peer to send the packet to
  uint8_t data_[ESP_NOW_MAX_DATA_LEN]{0};  // Data to send
  uint8_t size_{0};                        // Size of the data to send, must be <= ESP_NOW_MAX_DATA_LEN
  send_callback_t callback_{nullptr};      // Callback to call when the send operation is complete

 private:
  void init_data_(const uint8_t *peer_address, const uint8_t *payload, size_t size) {
    memcpy(this->address_, peer_address, ESP_NOW_ETH_ALEN);
    if (size > ESP_NOW_MAX_DATA_LEN) {
      this->size_ = 0;
      return;
    }
    this->size_ = size;
    memcpy(this->data_, payload, this->size_);
  }
};

}  // namespace esphome::espnow

#endif  // USE_ESP32
