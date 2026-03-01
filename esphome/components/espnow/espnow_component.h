#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include "esphome/core/lock_free_queue.h"
#include "espnow_compat.h"
#include "espnow_packet.h"

#if defined(USE_ESP32)
#include "esphome/core/event_pool.h"
#endif

#if defined(USE_ESP32)
#include <esp_idf_version.h>
#include <esp_mac.h>
#endif

#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace esphome::espnow {

#if defined(USE_ESP8266)
template<class T, uint8_t SIZE> class EventPool {
 public:
  T *allocate() { return new T(); }
  void release(T *event) { delete event; }
};

template<class T, uint8_t SIZE> class EventQueue {
 public:
  void push(T *item) {
    uint8_t next = (this->tail_ + 1) % SIZE;
    if (next == this->head_) {
      this->increment_dropped_count();
      return;
    }
    this->items_[this->tail_] = item;
    this->tail_ = next;
  }

  T *pop() {
    if (this->head_ == this->tail_) {
      return nullptr;
    }
    T *item = this->items_[this->head_];
    this->head_ = (this->head_ + 1) % SIZE;
    return item;
  }

  void increment_dropped_count() { this->dropped_count_++; }

  uint16_t get_and_reset_dropped_count() {
    uint16_t count = this->dropped_count_;
    this->dropped_count_ = 0;
    return count;
  }

 protected:
  T *items_[SIZE]{nullptr};
  uint8_t head_{0};
  uint8_t tail_{0};
  uint16_t dropped_count_{0};
};

template<class T, uint8_t SIZE> using PacketQueue = EventQueue<T, SIZE>;
#else
template<class T, uint8_t SIZE> using PacketQueue = LockFreeQueue<T, SIZE>;
#endif

// Maximum size of the ESPNow event queue - must be power of 2 for lock-free queue
static constexpr size_t MAX_ESP_NOW_SEND_QUEUE_SIZE = 16;
static constexpr size_t MAX_ESP_NOW_RECEIVE_QUEUE_SIZE = 16;

using peer_address_t = std::array<uint8_t, ESPNOW_ETH_ALEN>;

enum class ESPNowTriggers : uint8_t {
  TRIGGER_NONE = 0,
  ON_NEW_PEER = 1,
  ON_RECEIVED = 2,
  ON_BROADCASTED = 3,
  ON_SUCCEED = 10,
  ON_FAILED = 11,
};

enum ESPNowState : uint8_t {
  /** Nothing has been initialized yet. */
  ESPNOW_STATE_OFF = 0,
  /** ESPNOW is disabled. */
  ESPNOW_STATE_DISABLED,
  /** ESPNOW is enabled. */
  ESPNOW_STATE_ENABLED,
};

struct ESPNowPeer {
  uint8_t address[ESPNOW_ETH_ALEN];  // MAC address of the peer

  bool operator==(const ESPNowPeer &other) const {
    return memcmp(this->address, other.address, ESPNOW_ETH_ALEN) == 0;
  }
  bool operator==(const uint8_t *other) const { return memcmp(this->address, other, ESPNOW_ETH_ALEN) == 0; }
};

/// Handler interface for receiving ESPNow packets from unknown peers
/// Components should inherit from this class to handle incoming ESPNow data
class ESPNowUnknownPeerHandler {
 public:
  /// Called when an ESPNow packet is received from an unknown peer
  /// @param info Information about the received packet (sender MAC, etc.)
  /// @param data Pointer to the received data payload
  /// @param size Size of the received data in bytes
  /// @return true if the packet was handled, false otherwise
  virtual bool on_unknown_peer(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) = 0;
};

/// Handler interface for receiving ESPNow packets
/// Components should inherit from this class to handle incoming ESPNow data
class ESPNowReceivedPacketHandler {
 public:
  /// Called when an ESPNow packet is received
  /// @param info Information about the received packet (sender MAC, etc.)
  /// @param data Pointer to the received data payload
  /// @param size Size of the received data in bytes
  /// @return true if the packet was handled, false otherwise
  virtual bool on_received(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) = 0;
};
/// Handler interface for receiving broadcasted ESPNow packets
/// Components should inherit from this class to handle incoming ESPNow data
class ESPNowBroadcastedHandler {
 public:
  /// Called when a broadcasted ESPNow packet is received
  /// @param info Information about the received packet (sender MAC, etc.)
  /// @param data Pointer to the received data payload
  /// @param size Size of the received data in bytes
  /// @return true if the packet was handled, false otherwise
  virtual bool on_broadcasted(const ESPNowRecvInfo &info, const uint8_t *data, uint8_t size) = 0;
};

class ESPNowComponent : public Component {
 public:
  ESPNowComponent();
  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::LATE; }

  // Add a peer to the internal list of peers
  void add_peer(peer_address_t address) {
    ESPNowPeer peer;
    memcpy(peer.address, address.data(), ESPNOW_ETH_ALEN);
    this->peers_.push_back(peer);
  }
  // Add a peer with the esp_now api and add to the internal list if doesnt exist already
  espnow_err_t add_peer(const uint8_t *peer);
  // Remove a peer with the esp_now api and remove from the internal list if exists
  espnow_err_t del_peer(const uint8_t *peer);

  void set_wifi_channel(uint8_t channel) { this->wifi_channel_ = channel; }
  void apply_wifi_channel();
  uint8_t get_wifi_channel();

  void set_auto_add_peer(bool value) { this->auto_add_peer_ = value; }

  void enable();
  void disable();
  bool is_disabled() const { return this->state_ == ESPNOW_STATE_DISABLED; };
  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }
  bool is_wifi_enabled();

  /// @brief Queue a packet to be sent to a specific peer address.
  /// This method will add the packet to the internal queue and
  /// call the callback when the packet is sent.
  /// Only one packet will be sent at any given time and the next one will not be sent until
  /// the previous one has been acknowledged or failed.
  /// @param peer_address MAC address of the peer to send the packet to
  /// @param payload Data payload to send
  /// @param callback Callback to call when the send operation is complete
  /// @return ESP_OK on success, or an error code on failure
  espnow_err_t send(const uint8_t *peer_address, const std::vector<uint8_t> &payload,
                 const send_callback_t &callback = nullptr) {
    return this->send(peer_address, payload.data(), payload.size(), callback);
  }
  espnow_err_t send(const uint8_t *peer_address, const uint8_t *payload, size_t size,
                    const send_callback_t &callback = nullptr);

  void register_received_handler(ESPNowReceivedPacketHandler *handler) { this->received_handlers_.push_back(handler); }
  void register_unknown_peer_handler(ESPNowUnknownPeerHandler *handler) {
    this->unknown_peer_handlers_.push_back(handler);
  }
  void register_broadcasted_handler(ESPNowBroadcastedHandler *handler) {
    this->broadcasted_handlers_.push_back(handler);
  }

 protected:
#if defined(USE_ESP32)
  friend void on_data_received(const esp_now_recv_info_t *info, const uint8_t *data, int size);
  #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  friend void on_send_report(const esp_now_send_info_t *info, esp_now_send_status_t status);
  #else
  friend void on_send_report(const uint8_t *mac_addr, esp_now_send_status_t status);
  #endif
#elif defined(USE_ESP8266)
  friend void on_data_received(uint8_t *mac_addr, uint8_t *data, uint8_t size);
  friend void on_send_report(uint8_t *mac_addr, uint8_t status);
#endif

  void enable_();
  void send_();

  std::vector<ESPNowUnknownPeerHandler *> unknown_peer_handlers_;
  std::vector<ESPNowReceivedPacketHandler *> received_handlers_;
  std::vector<ESPNowBroadcastedHandler *> broadcasted_handlers_;

  std::vector<ESPNowPeer> peers_{};

  uint8_t own_address_[ESPNOW_ETH_ALEN]{0};
  PacketQueue<ESPNowPacket, MAX_ESP_NOW_RECEIVE_QUEUE_SIZE> receive_packet_queue_{};
  EventPool<ESPNowPacket, MAX_ESP_NOW_RECEIVE_QUEUE_SIZE> receive_packet_pool_{};

  PacketQueue<ESPNowSendPacket, MAX_ESP_NOW_SEND_QUEUE_SIZE> send_packet_queue_{};
  EventPool<ESPNowSendPacket, MAX_ESP_NOW_SEND_QUEUE_SIZE> send_packet_pool_{};
  ESPNowSendPacket *current_send_packet_{nullptr};  // Currently sending packet, nullptr if none

  uint8_t wifi_channel_{0};
  ESPNowState state_{ESPNOW_STATE_OFF};

  bool auto_add_peer_{false};
  bool enable_on_boot_{true};
};

extern ESPNowComponent *global_esp_now;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::espnow

#endif  // USE_ESP32 || ESP8266
