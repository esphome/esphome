#pragma once

// #if defined(USE_ESP32)

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include <esp_now.h>

#include <array>
#include <memory>
#include <queue>
#include <vector>

namespace esphome {
namespace espnow {

typedef uint8_t espnow_addr_t[6];

static const uint64_t ESPNOW_BROADCAST_ADDR = 0xFFFFFFFFFFFF;
static espnow_addr_t ESPNOW_ADDR_SELF = {0};

static void uint64_to_addr(uint64_t address, uint8_t *bd_addr) {
  *(bd_addr + 0) = (address >> 40) & 0xff;
  *(bd_addr + 1) = (address >> 32) & 0xff;
  *(bd_addr + 2) = (address >> 24) & 0xff;
  *(bd_addr + 3) = (address >> 16) & 0xff;
  *(bd_addr + 4) = (address >> 8) & 0xff;
  *(bd_addr + 5) = (address >> 0) & 0xff;
}

static uint64_t addr_to_uint64(const uint8_t *address) {
  uint64_t u = 0;
  u |= uint64_t(*(address + 0) & 0xFF) << 40;
  u |= uint64_t(*(address + 1) & 0xFF) << 32;
  u |= uint64_t(*(address + 2) & 0xFF) << 24;
  u |= uint64_t(*(address + 3) & 0xFF) << 16;
  u |= uint64_t(*(address + 4) & 0xFF) << 8;
  u |= uint64_t(*(address + 5) & 0xFF) << 0;
  return u;
}

class ESPNowComponent;

template<typename T, typename Container = std::deque<T> > class iterable_queue : public std::queue<T, Container> {
 public:
  typedef typename Container::iterator iterator;
  typedef typename Container::const_iterator const_iterator;

  iterator begin() { return this->c.begin(); }
  iterator end() { return this->c.end(); }
  const_iterator begin() const { return this->c.begin(); }
  const_iterator end() const { return this->c.end(); }
};

class ESPNowPackage {
 public:
  ESPNowPackage(const uint64_t mac_address, const std::vector<uint8_t> data);
  ESPNowPackage(const uint64_t mac_address, const uint8_t *data, size_t len);

  uint64_t mac_address() { return this->mac_address_ == 0 ? ESPNOW_BROADCAST_ADDR : this->mac_address_; }

  void mac_bytes(uint8_t *mac_addres) {
    uint64_t mac = this->mac_address_ == 0 ? ESPNOW_BROADCAST_ADDR : this->mac_address_;
    uint64_to_addr(mac, mac_addres);
  }

  std::vector<uint8_t> data() { return data_; }

  uint8_t get_counter() { return send_count_; }
  void inc_counter() {
    send_count_ = send_count_ + 1;
    if (send_count_ > 5 && !is_holded_) {
      set_holding();
    }
  }
  void reset_counter() {
    send_count_ = 0;
    del_holding();
  }

  void is_broadcast(bool value) { this->is_broadcast_ = value; }
  bool is_broadcast() const { return this->is_broadcast_; }

  void timestamp(uint32_t value) { this->timestamp_ = value; }
  uint32_t timestamp() { return this->timestamp_; }

  void rssi(int8_t rssi) { this->rssi_ = rssi; }
  int8_t rssi() { return this->rssi_; }

  bool is_holded() { return this->is_holded_; }
  void set_holding() { this->is_holded_ = true; }
  void del_holding() { this->is_holded_ = false; }

 protected:
  uint64_t mac_address_{0};
  std::vector<uint8_t> data_;

  uint8_t send_count_{0};
  bool is_broadcast_{false};
  uint32_t timestamp_{0};
  uint8_t rssi_{0};

  bool is_holded_{false};
};

class ESPNowInterface : public Component, public Parented<ESPNowComponent> {
 public:
  ESPNowInterface(){};

  void setup() override;

  virtual bool on_package_received(ESPNowPackage *package) { return false; };
  virtual bool on_package_send(ESPNowPackage *package) { return false; };
  virtual bool on_new_peer(ESPNowPackage *package) { return false; };
};

class ESPNowComponent : public Component {
 public:
  ESPNowComponent();

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 1)
  static void on_data_received(const esp_now_recv_info_t *recv_info, const uint8_t *data, int size);
#else
  static void on_data_received(const uint8_t *addr, const uint8_t *data, int size);
#endif

  static void on_data_send(const uint8_t *mac_addr, esp_now_send_status_t status);

  void dump_config() override;
  float get_setup_priority() const override { return -100; }

  void setup() override;

  void loop() override;
  void set_wifi_channel(uint8_t channel) { this->wifi_channel_ = channel; }

  ESPNowPackage *send_package(const uint64_t mac_address, const uint8_t *data, int len) {
    //  ESPNowPackage * package = new ESPNowPackage(mac_address, data, len);
    //  return this->send_package(package);
    return nullptr;
  }

  ESPNowPackage *send_package(const uint64_t mac_address, const std::vector<uint8_t> data) {
    ESPNowPackage *package = new ESPNowPackage(mac_address, data);
    return this->send_package(package);
  }

  ESPNowPackage *send_package(ESPNowPackage *package);

  void add_on_package_send_callback(std::function<void(ESPNowPackage *)> &&callback) {
    this->on_package_send_.add(std::move(callback));
  }

  void add_on_package_receive_callback(std::function<void(ESPNowPackage *)> &&callback) {
    this->on_package_receved_.add(std::move(callback));
  }

  void add_on_peer_callback(std::function<void(ESPNowPackage *)> &&callback) {
    this->on_new_peer_.add(std::move(callback));
  }

  void register_protocol(ESPNowInterface *protocol) {
    protocol->set_parent(this);
    this->protocols_.push_back(protocol);
  }

  esp_err_t add_peer(uint64_t addr);
  esp_err_t del_peer(uint64_t addr);

  void set_auto_add_peer(bool value) { this->auto_add_peer_ = value; }

  void on_package_received(ESPNowPackage *package);
  void on_package_send(ESPNowPackage *package);
  void on_new_peer(ESPNowPackage *package);

  void log_error_(std::string msg, esp_err_t err);

 protected:
  void unHold_send_(uint64_t mac);
  void push_receive_package_(ESPNowPackage *package) { this->receive_queue_.push(std::move(package)); }
  bool validate_channel_(uint8_t channel);
  uint8_t wifi_channel_{0};
  bool auto_add_peer_{false};

  CallbackManager<void(ESPNowPackage *)> on_package_send_;
  CallbackManager<void(ESPNowPackage *)> on_package_receved_;
  CallbackManager<void(ESPNowPackage *)> on_new_peer_;

  iterable_queue<ESPNowPackage *> receive_queue_{};
  iterable_queue<ESPNowPackage *> send_queue_{};

  std::vector<ESPNowInterface *> protocols_{};
  std::vector<uint64_t> peers_{};

  SemaphoreHandle_t send_lock_ = NULL;

  bool can_send_{true};
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
      this->parent_->send_package(mac, this->data_static_);
    } else {
      auto val = this->data_func_(x...);
      this->parent_->send_package(mac, val);
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

class ESPNowSendTrigger : public Trigger<ESPNowPackage *> {
 public:
  explicit ESPNowSendTrigger(ESPNowComponent *parent) {
    parent->add_on_package_send_callback([this](ESPNowPackage *value) { this->trigger(value); });
  }
};

class ESPNowReceiveTrigger : public Trigger<ESPNowPackage *> {
 public:
  explicit ESPNowReceiveTrigger(ESPNowComponent *parent) {
    parent->add_on_package_receive_callback([this](ESPNowPackage *value) { this->trigger(value); });
  }
};

class ESPNowNewPeerTrigger : public Trigger<ESPNowPackage *> {
 public:
  explicit ESPNowNewPeerTrigger(ESPNowComponent *parent) {
    parent->add_on_peer_callback([this](ESPNowPackage *value) { this->trigger(value); });
  }
};

extern ESPNowComponent *global_esp_now;

}  // namespace espnow
}  // namespace esphome

// #endif
