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

typedef uint8_t espnow_addr_t[6];

static const espnow_addr_t ESPNOW_BROADCAST_ADDR = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static espnow_addr_t ESPNOW_ADDR_SELF = {0};

static const uint8_t ESPNOW_DATA_HEADER = 0x00;
static const uint8_t ESPNOW_DATA_PROTOCOL = 0x03;
static const uint8_t ESPNOW_DATA_PACKET = 0x07;
static const uint8_t ESPNOW_DATA_CRC = 0x08;
static const uint8_t ESPNOW_DATA_CONTENT = 0x0A;

static const uint8_t MAX_ESPNOW_DATA_SIZE = 240;

static const uint32_t TRANSPORT_HEADER = 0xC19983;

static uint8_t last_ref_id = 0;

class ESPNowPacket {
 private:
  espnow_addr_t peer_{0};
  uint8_t rssi_{0};
  uint8_t retrys_{0};
  bool is_broadcast_{false};
  uint32_t timestamp_{0};
  ByteBuffer *content_;

 public:
  ESPNowPacket() {
    this->content_ = new ByteBuffer(251);
    this->content_->put_uint24(TRANSPORT_HEADER);
  };
  // Create packet to be send.
  ESPNowPacket(espnow_addr_t peer, const uint8_t *data, uint8_t size, uint32_t app_id);

  // Load received packet's.
  ESPNowPacket(espnow_addr_t *peer, const uint8_t *data, uint8_t size);

  espnow_addr_t *peer() { return &this->peer_; }
  void peer(espnow_addr_t *peer) {
    if (*peer[0] == 0) {
      std::memcpy(&this->peer_, &ESPNOW_BROADCAST_ADDR, 6);
    }
  }
  else {
    std::memcpy(&this->peer_, &peer, 6);
  }
}

uint8_t
size() {
  if (this->content_->get_used() <= ESPNOW_DATA_CONTENT) {
    return ESPNOW_DATA_CONTENT;
  }
  return this->content_->get_used();
}

bool broadcast(){return this->is_broadcast_};
void broadcast(bool state) { this->is_broadcast_ = state; }

uint32_t timestamp() { return this->timestamp_; };
void timestamp(uint32_t timestamp) { this->timestamp_ = timestamp; };

uint32_t protocol() {
  this->content_->set_position(ESPNOW_DATA_PROTOCOL);
  return this->content_->get_uint24();
}
void protocol(uint32_t protocol) {
  this->content_->set_position(ESPNOW_DATA_PROTOCOL);
  this->content_->put_uint24(protocol);
}

uint8_t packet_id() {
  this->content_->set_position(ESPNOW_DATA_PACKET);
  return this->content_->get_uint8();
}
void packet_id(uint8_t packet_id) {
  this->content_->set_position(ESPNOW_DATA_PACKET);
  this->content_->put_uint8(packet_id);
}

uint16_t crc() {
  this->content_->set_position(ESPNOW_DATA_CRC);
  return this->content_->get_uint16();
}
void crc(uint16_t crc) {
  this->content_->set_position(ESPNOW_DATA_CRC);
  this->content_->put_uint16(crc);
}

ByteBuffer *content() {
  this->content_->set_position(this->size());
  return &this->content_;
}

uint8_t content(uint8_t pos) {
  this->content_->set_position(pos);
  return this->content_->get_uint8();
}
uint8_t *content_bytes() { return this->content_->get_data()->data(); };

void retry() {
  if (this->retrys_ < 7) {
    this->retrys_ = this->retrys_ + 1;
  }
}

void calc_crc() {
  this->crc(0);
  this->crc(esp_crc16_le(this->packet(), this->dataptr(), this->size()));
}

bool is_valid();

};

class ESPNowComponent;

static const uint32_t ESPNOW_DEFAULT_APP_ID = 0x11CFAF;

class ESPNowProtocol : public Parented<ESPNowComponent> {
 public:
  ESPNowProtocol(){};

  virtual void on_receive(ESPNowPacket *packet){};
  virtual void on_sent(ESPNowPacket *packet, bool status){};
  virtual void on_new_peer(ESPNowPacket *packet){};

  virtual uint32_t get_protocol_id() = 0;
  uint8_t get_next_ref_id() { return next_ref_id_++; }

  bool write(uint64_t mac_address, const uint8_t *data, uint8_t len);
  bool write(uint64_t mac_address, std::vector<uint8_t> &data);
  bool write(ESPNowPacket *packet);

 protected:
  uint8_t next_ref_id_{0};
};

class ESPNowDefaultProtocol : public ESPNowProtocol {
 public:
  void on_receive(ESPNowPacket *packet) override { this->on_receive_.call(packet); };
  void on_sent(ESPNowPacket *packet, bool status) override { this->on_sent_.call(packet, status); };
  void on_new_peer(ESPNowPacket *packet) override { this->on_new_peer_.call(packet); };

  uint32_t get_protocol_id() override { return ESPNOW_DEFAULT_APP_ID; };

  void add_on_sent_callback(std::function<void(ESPNowPacket *, bool status)> &&callback) {
    this->on_sent_.add(std::move(callback));
  }
  void add_on_receive_callback(std::function<void(ESPNowPacket *)> &&callback) {
    this->on_receive_.add(std::move(callback));
  }
  void add_on_peer_callback(std::function<void(ESPNowPacket *)> &&callback) {
    this->on_new_peer_.add(std::move(callback));
  }

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
  void loop() override;

  bool write(ESPNowPacket *packet);

  void register_protocol(ESPNowProtocol *protocol) {
    protocol->set_parent(this);
    this->protocols_[protocol->get_protocol_id()] = protocol;
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

 protected:
  bool validate_channel_(uint8_t channel);
  ESPNowProtocol *get_protocol_(uint32_t protocol);

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

  TaskHandle_t espnow_task_handle_{nullptr};
};

template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  template<typename V> void set_mac(V mac) { this->mac_ = mac; }
  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
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
      this->parent_->get_default_protocol()->write(mac, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->get_default_protocol()->write(mac, val);
    }
  }

 protected:
  TemplatableValue<uint64_t, Ts...> mac_{};
  bool static_{false};
  std::function<std::vector<uint8_t>(Ts...)> data_func_{};
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

extern ESPNowComponent *global_esp_now;

}  // namespace espnow
}  // namespace esphome

#endif
