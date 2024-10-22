#pragma once

#if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <esp_now.h>

#include <array>
#include <memory>
#include <queue>
#include <vector>
#include <mutex>
#include <map>

namespace esphome {
namespace espnow {

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;

static const uint8_t MAX_ESPNOW_DATA_SIZE = 241;

static const uint8_t TRANSPORT_HEADER[3] = {'N', '0', 'w'};
static const uint32_t ESPNOW_MAIN_PROTOCOL_ID = 0x447453;  // = StD

static const uint8_t ESPNOW_COMMAND_ACK = 0x06;
static const uint8_t ESPNOW_COMMAND_NAK = 0x15;
static const uint8_t ESPNOW_COMMAND_RESEND = 0x05;

struct ESPNowPacket {
  uint64_t peer{0};
  uint8_t rssi{0};
  int8_t attempts{0};
  bool is_broadcast{false};
  uint32_t timestamp{0};
  uint8_t size{0};
  struct {
    struct {
      uint8_t header[3]{'N', '0', 'w'};
      uint32_t protocol{0};
      uint8_t sequents{0};
    } __attribute__((packed)) prefix;
    uint8_t payload[MAX_ESPNOW_DATA_SIZE + 1]{0};
  } __attribute__((packed)) content;

  inline ESPNowPacket() ESPHOME_ALWAYS_INLINE {}
  // Create packet to be send.
  inline ESPNowPacket(uint64_t peer, const uint8_t *data, uint8_t size, uint32_t protocol,
                      uint8_t command = 0) ESPHOME_ALWAYS_INLINE {
    assert(size <= MAX_ESPNOW_DATA_SIZE);
    if (peer == 0) {
      peer = ESPNOW_BROADCAST_ADDR;
    }

    this->peer = peer;

    this->set_protocol(protocol);
    if (command != 0) {
      this->set_command(command);
    }
    this->size = size;
    std::memcpy(this->get_payload(), data, size);
  }
  // Load received packet's.
  ESPNowPacket(const uint8_t *peer, const uint8_t *data, uint8_t size) ESPHOME_ALWAYS_INLINE {
    this->set_peer(peer);
    std::memcpy(this->get_content(), data, size);
    this->size = size - this->prefix_size();
  }

  inline uint8_t *get_peer() const { return (uint8_t *) &(this->peer); }
  inline void set_peer(const uint8_t *peer) ESPHOME_ALWAYS_INLINE {
    if (*peer == 0) {
      peer = (uint8_t *) &ESPNOW_BROADCAST_ADDR;
    }
    memcpy((void *) this->get_peer(), (const void *) peer, 6);
  };
  inline bool is_peer(const uint8_t *peer) const { return memcmp(peer, this->get_peer(), 6) == 0; }

  uint8_t prefix_size() const { return sizeof(this->content.prefix); }

  uint8_t content_size() const { return (this->prefix_size() + this->size); }

  inline uint32_t get_protocol() const { return this->content.prefix.protocol & 0x00FFFFFF; }
  inline void set_protocol(uint32_t protocol) {
    this->content.prefix.protocol = (this->content.prefix.protocol & 0xFF000000) + (protocol & 0x00FFFFFF);
  }

  inline uint8_t get_command() const { return this->content.prefix.protocol >> 24; }
  inline void set_command(uint32_t command) ESPHOME_ALWAYS_INLINE {
    this->content.prefix.protocol = (this->content.prefix.protocol & 0x00FFFFFF) + (command << 24);
  }

  uint8_t get_sequents() const { return this->content.prefix.sequents; }
  inline void set_sequents(uint8_t sequents) ESPHOME_ALWAYS_INLINE { this->content.prefix.sequents = sequents; }

  inline uint8_t *get_content() const { return (uint8_t *) &(this->content); }
  inline uint8_t *get_payload() const { return (uint8_t *) &(this->content.payload); }
  inline uint8_t get_byte_at(uint8_t pos) const {
    assert(pos < this->size);
    return *(((uint8_t *) &this->content) + pos);
  }

  inline void retry() ESPHOME_ALWAYS_INLINE { attempts++; }

  inline bool is_valid() const {
    bool valid = (memcmp((const void *) this->get_content(), (const void *) &TRANSPORT_HEADER, 3) == 0);
    valid &= (this->get_protocol() != 0);
    return valid;
  }
};

class ESPNowComponent;

class ESPNowProtocol : public Parented<ESPNowComponent> {
 public:
  ESPNowProtocol(){};

  virtual void on_receive(const ESPNowPacket &packet){};
  virtual void on_sent(const ESPNowPacket &packet, bool status){};
  virtual void on_new_peer(const ESPNowPacket &packet){};

  virtual uint32_t get_protocol_id() = 0;
  virtual std::string get_protocol_name() = 0;

  uint8_t get_next_sequents() { return this->get_next_sequents(0); }
  virtual uint8_t get_next_sequents(uint64_t peer) {
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

  bool send(uint64_t peer, const uint8_t *data, uint8_t len, uint8_t command = 0);

 protected:
  uint8_t next_sequents_{255};
};

class ESPNowDefaultProtocol : public ESPNowProtocol {
 public:
  uint32_t get_protocol_id() override { return ESPNOW_MAIN_PROTOCOL_ID; };
  std::string get_protocol_name() override { return "Default"; }

  void add_on_receive_callback(std::function<void(const ESPNowPacket)> &&callback) {
    this->on_receive_.add(std::move(callback));
  }
  void on_receive(const ESPNowPacket &packet) override { this->on_receive_.call(packet); };

  void add_on_sent_callback(std::function<void(const ESPNowPacket, bool status)> &&callback) {
    this->on_sent_.add(std::move(callback));
  }
  void on_sent(const ESPNowPacket &packet, bool status) override { this->on_sent_.call(packet, status); };

  void add_on_peer_callback(std::function<void(const ESPNowPacket)> &&callback) {
    this->on_new_peer_.add(std::move(callback));
  }
  void on_new_peer(const ESPNowPacket &packet) override { this->on_new_peer_.call(packet); };

 protected:
  CallbackManager<void(const ESPNowPacket, bool)> on_sent_;
  CallbackManager<void(const ESPNowPacket)> on_receive_;
  CallbackManager<void(const ESPNowPacket)> on_new_peer_;
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
  void set_convermation_timeout(uint32_t timeout) { this->conformation_timeout_ = timeout; }
  void set_retries(uint8_t value) { this->retries_ = value; }

  void setup() override;
  void loop() override;

  bool send(ESPNowPacket packet);

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

  void show_packet(const std::string &title, const ESPNowPacket &packet);

  static void espnow_task(void *params);

 protected:
  bool validate_channel_(uint8_t channel);
  ESPNowProtocol *get_protocol_(uint32_t protocol);

  uint64_t own_peer_address_{0};
  uint8_t wifi_channel_{0};

  uint32_t conformation_timeout_{5000};
  uint8_t retries_{5};

  bool auto_add_peer_{false};
  bool use_sent_check_{true};

  bool lock_{false};

  void call_on_receive_(ESPNowPacket &packet);
  void call_on_sent_(ESPNowPacket &packet, bool status);
  void call_on_new_peer_(ESPNowPacket &packet);

  QueueHandle_t receive_queue_{};
  QueueHandle_t send_queue_{};

  std::map<uint32_t, ESPNowProtocol *> protocols_{};
  std::vector<uint64_t> peers_{};
  bool task_running_{false};
  static ESPNowComponent *static_;  // NOLINT
};

template<typename... Ts> class SendAction : public Action<Ts...>, public Parented<ESPNowComponent> {
 public:
  template<typename V> void set_mac(V mac) { this->mac_ = mac; }
  template<typename V> void set_command(V command) { this->command_ = command; }

  void set_data_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->data_func_ = func;
    this->dynamic_ = true;
  }
  void set_data_static(const std::vector<uint8_t> &data) { this->data_static_ = data; }

  void play(Ts... x) override {
    uint64_t mac = this->mac_.value(x...);
    uint8_t command = 0;
    if (this->command_.has_value()) {
      command = this->mac_.value(x...);
    }

    if (this->dynamic_) {
      this->data_static_ = this->data_func_(x...);
    }
    this->parent_->get_default_protocol()->send(mac, this->data_static_.data(), this->data_static_.size(), command);
  }

 protected:
  TemplatableValue<uint8_t, Ts...> command_{};
  TemplatableValue<uint64_t, Ts...> mac_{};
  bool dynamic_{false};
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

class ESPNowSentTrigger : public Trigger<const ESPNowPacket, bool> {
 public:
  explicit ESPNowSentTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_sent_callback([this](const ESPNowPacket packet, bool status) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet, status);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

class ESPNowReceiveTrigger : public Trigger<const ESPNowPacket> {
 public:
  explicit ESPNowReceiveTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_receive_callback([this](const ESPNowPacket packet) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

class ESPNowNewPeerTrigger : public Trigger<const ESPNowPacket> {
 public:
  explicit ESPNowNewPeerTrigger(ESPNowComponent *parent) {
    parent->get_default_protocol()->add_on_peer_callback([this](const ESPNowPacket packet) {
      if ((this->command_ == 0) || this->command_ == packet.get_command()) {
        this->trigger(packet);
      }
    });
  }
  void set_command(uint8_t command) { this->command_ = command; }

 protected:
  uint8_t command_{0};
};

}  // namespace espnow
}  // namespace esphome

#endif
