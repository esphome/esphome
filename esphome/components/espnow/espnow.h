#pragma once

#if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/bytebuffer.h"

#include <esp_now.h>
#include <esp_crc.h>

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>

namespace esphome {
namespace espnow {

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;

static const uint8_t ESPNOW_DATA_HEADER = 0x00;
static const uint8_t ESPNOW_DATA_PROTOCOL = 0x03;
static const uint8_t ESPNOW_DATA_PACKET = 0x07;
static const uint8_t ESPNOW_DATA_CONTENT = 0x08;

static const uint8_t MAX_ESPNOW_DATA_SIZE = 240;

static const uint8_t MAX_NUMBER_OF_RETRYS = 5;

static const uint32_t TRANSPORT_HEADER = 0xC19983;
static const uint32_t ESPNOW_MAIN_PROTOCOL_ID = 0x11CFAF;

struct ESPNowPacket {
  uint64_t peer{0};
  uint8_t rssi{0};
  uint8_t attempts{0};
  bool is_broadcast{false};
  uint32_t timestamp{0};
  uint8_t size{0};
  struct {
    struct {
      uint8_t header[3]{'N', '0', 'w'};
      uint32_t protocol{0};
      uint8_t sequents{0};
      uint8_t crc{0};
    } __attribute__((packed)) prefix;
    uint8_t payload[MAX_ESPNOW_DATA_SIZE + 1]{0};
  } __attribute__((packed)) content;

  ESPNowPacket() {}
  // Create packet to be send.
  ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size, uint32_t protocol);

  // Load received packet's.
  ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size);

  uint8_t *peer_as_bytes() { return (uint8_t *) &(this->peer); }
  void set_peer(uint64_t peer) {
    if (peer == 0) {
      this->peer = ESPNOW_BROADCAST_ADDR;
    } else {
      this->peer = peer;
    }
  };

  inline uint8_t prefix_size() { return sizeof(this->content.prefix); }

  inline uint8_t content_size() { return this->prefix_size() + this->size; }

  inline uint32_t protocol() { return this->content.prefix.protocol; }
  void protocol(uint32_t protocol) {
    this->content.prefix.protocol = protocol;
    this->calc_crc();
  }

  inline uint8_t sequents() { return this->content.prefix.sequents; }
  void sequents(uint8_t sequents) {
    this->content.prefix.sequents = sequents;
    this->calc_crc();
  }

  uint8_t *content_as_bytes() { return (uint8_t *) &(this->content); }
  uint8_t *payload_as_bytes() { return (uint8_t *) &(this->content.payload); }
  uint8_t content_at(uint8_t pos) {
    assert(pos < this->size);
    return *(((uint8_t *) &this->content) + pos);
  }

  uint8_t crc() { return this->content.prefix.crc; }
  void crc(uint8_t crc) { this->content.prefix.crc = crc; }
  void calc_crc() {
    this->content.prefix.crc = 0;
    uint8_t crc = esp_crc8_le(0, this->peer_as_bytes(), 6);
    this->content.prefix.crc = esp_crc8_le(crc, (const uint8_t *) &this->content, this->size);
  }

  void retry() { this->attempts++; }
  bool is_valid();
};

class ESPNowComponent;

class ESPNowProtocol : public Parented<ESPNowComponent> {
 public:
  ESPNowProtocol(){};

  virtual void on_receive(ESPNowPacket *packet){};
  virtual void on_sent(ESPNowPacket *packet, bool status){};
  virtual void on_new_peer(ESPNowPacket *packet){};

  virtual uint32_t get_protocol_component_id() = 0;
  uint8_t get_next_sequents() {
    if (this->next_sequents_ == 255) {
      this->next_sequents_ = 0;
    } else {
      this->next_sequents_++;
    }
    return this->next_sequents_;
  }
  bool is_valid_squence(uint8_t received_sequence) {
    bool valid = this->next_sequents_ + 1 == received_sequence;
    if (valid) {
      this->next_sequents_ = received_sequence;
    }
    return valid;
  }

  bool write(uint64_t peer, const uint8_t *data, uint8_t len);
  bool write(uint64_t peer, ByteBuffer &data) {
    return this->write(peer, data.get_data().data(), (uint8_t) data.get_used_space());
  }

 protected:
  uint8_t next_sequents_{255};
};

class ESPNowDefaultProtocol : public ESPNowProtocol {
 public:
  uint32_t get_protocol_component_id() override { return ESPNOW_MAIN_PROTOCOL_ID; };

  void add_on_receive_callback(std::function<void(ESPNowPacket *)> &&callback) {
    this->on_receive_.add(std::move(callback));
  }
  void on_receive(ESPNowPacket *packet) override { this->on_receive_.call(packet); };

  void add_on_sent_callback(std::function<void(ESPNowPacket *, bool status)> &&callback) {
    this->on_sent_.add(std::move(callback));
  }
  void on_sent(ESPNowPacket *packet, bool status) override { this->on_sent_.call(packet, status); };

  void add_on_peer_callback(std::function<void(ESPNowPacket *)> &&callback) {
    this->on_new_peer_.add(std::move(callback));
  }
  void on_new_peer(ESPNowPacket *packet) override { this->on_new_peer_.call(packet); };

 protected:
  CallbackManager<void(ESPNowPacket *, bool)> on_sent_;
  CallbackManager<void(ESPNowPacket *)> on_receive_;
  CallbackManager<void(ESPNowPacket *)> on_new_peer_;
};

class ESPNowComponent : public Component {
 public:
  ESPNowComponent();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  static void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size);
#else
  static void on_data_received(const uint8_t *addr, const uint8_t *data, int size);
#endif

  static void on_data_sent(const uint8_t *mac_addr, esp_now_send_status_t status);

  void dump_config() override;

  float get_setup_priority() const override { return -100; }

  void set_wifi_channel(uint8_t channel) { this->wifi_channel_ = channel; }
  void set_auto_add_peer(bool value) { this->auto_add_peer_ = value; }
  void set_use_sent_check(bool value) { this->use_sent_check_ = value; }

  void setup() override;

  void runner();

  bool write(ESPNowPacket *packet);

  void register_protocol(ESPNowProtocol *protocol) {
    protocol->set_parent(this);
    this->protocols_[protocol->get_protocol_component_id()] = protocol;
  }

  esp_err_t add_peer(uint64_t addr);
  esp_err_t del_peer(uint64_t addr);

  bool send_queue_empty() { return uxQueueMessagesWaiting(this->send_queue_) == 0; }
  bool send_queue_full() { return uxQueueSpacesAvailable(this->send_queue_) == 0; }
  size_t send_queue_used() { return uxQueueMessagesWaiting(this->send_queue_); }
  size_t send_queue_free() { return uxQueueSpacesAvailable(this->send_queue_); }

  void lock() { this->lock_ = true; }
  bool is_locked() { return this->lock_; }
  void unlock() { this->lock_ = false; }

  ESPNowDefaultProtocol *get_default_protocol();

  void show_packet(const std::string &title, ESPNowPacket *packet);

 protected:
  bool validate_channel_(uint8_t channel);
  ESPNowProtocol *get_protocol_component_(uint32_t protocol);

  uint8_t wifi_channel_{0};

  bool auto_add_peer_{false};
  bool use_sent_check_{true};
  bool lock_{false};

  void on_receive_(ESPNowPacket *packet);
  void on_sent_(ESPNowPacket *packet, bool status);
  void on_new_peer_(ESPNowPacket *packet);

  QueueHandle_t receive_queue_{};
  QueueHandle_t send_queue_{};

  std::map<uint32_t, ESPNowProtocol *> protocols_{};
  std::vector<uint64_t> peers_{};
  uint64_t own_peer_address_{0};
  ESPNowDefaultProtocol *default_protocol_{nullptr};

  TaskHandle_t espnow_task_handle_{nullptr};

  static ESPNowComponent *static_;  // NOLINT
};

template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  template<typename V> void set_mac(V mac) { this->mac_ = mac; }
  void set_data_template(std::function<ByteBuffer(Ts...)> func) {
    this->data_func_ = func;
    this->static_ = false;
  }
  void set_data_static(const std::vector<uint8_t> &data) {
    this->data_static_ = data;
    this->static_ = true;
  }

  void play(Ts... x) override {
    auto mac = this->mac_.value(x...);

    if (this->static_) {
      this->parent_->get_default_protocol()->write(mac, this->data_static_.data(), this->data_static_.size());
    } else {
      auto val = this->data_func_(x...);
      this->parent_->get_default_protocol()->write(mac, val);
    }
  }

 protected:
  TemplatableValue<uint64_t, Ts...> mac_{};
  bool static_{false};
  std::function<ByteBuffer(Ts...)> data_func_{};
  std::vector<uint8_t> data_static_{};
};

template<typename... Ts> class NewPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  template<typename V> void set_mac(V mac) { this->mac_ = mac; }
  void play(Ts... x) override {
    auto mac = this->mac_.value(x...);
    parent_->add_peer(mac);
  }

 protected:
  TemplatableValue<uint64_t, Ts...> mac_{};
};

template<typename... Ts> class DelPeerAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  template<typename V> void set_mac(V mac) { this->mac_ = mac; }
  void play(Ts... x) override {
    auto mac = this->mac_.value(x...);
    parent_->del_peer(mac);
  }

 protected:
  TemplatableValue<uint64_t, Ts...> mac_{};
};

class ESPNowSentTrigger : public Trigger<ESPNowPacket *, bool> {
 public:
  explicit ESPNowSentTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_sent_callback(
        [this](ESPNowPacket *packet, bool status) { this->trigger(packet, status); });
  }
};

class ESPNowReceiveTrigger : public Trigger<ESPNowPacket *> {
 public:
  explicit ESPNowReceiveTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_receive_callback([this](ESPNowPacket *packet) { this->trigger(packet); });
  }
};

class ESPNowNewPeerTrigger : public Trigger<ESPNowPacket *> {
 public:
  explicit ESPNowNewPeerTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_peer_callback([this](ESPNowPacket *packet) { this->trigger(packet); });
  }
};

}  // namespace espnow
}  // namespace esphome

#endif
