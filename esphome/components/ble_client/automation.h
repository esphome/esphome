#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_CLIENT_LEGACY_ENGINE

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/ble_client.h"

namespace esphome::ble_client {

// implement on_connect automation.
class BLEClientConnectTrigger final : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (event == ESP_GATTC_SEARCH_CMPL_EVT) {
      this->node_state = espbt::ClientState::ESTABLISHED;
      this->trigger();
    }
  }
};

// on_disconnect automation
class BLEClientDisconnectTrigger final : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    // test for CLOSE and not DISCONNECT - DISCONNECT can occur even if no virtual connection (OPEN event) occurred.
    // So this will not trigger unless a complete open has previously succeeded.
    switch (event) {
      case ESP_GATTC_SEARCH_CMPL_EVT: {
        this->node_state = espbt::ClientState::ESTABLISHED;
        break;
      }
      case ESP_GATTC_CLOSE_EVT: {
        this->trigger();
        break;
      }
      default: {
        break;
      }
    }
  }
};

class BLEClientPasskeyRequestTrigger final : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr))
      this->trigger();
  }
};

class BLEClientPasskeyNotificationTrigger final : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyNotificationTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_NOTIF_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
      this->trigger(param->ble_security.key_notif.passkey);
    }
  }
};

class BLEClientNumericComparisonRequestTrigger final : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientNumericComparisonRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_NC_REQ_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
      this->trigger(param->ble_security.key_notif.passkey);
    }
  }
};

template<typename... Ts> class BLEClientPasskeyReplyAction final : public Action<Ts...> {
 public:
  BLEClientPasskeyReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(const Ts &...x) override {
    uint32_t passkey;
    if (has_simple_value_) {
      passkey = this->value_.simple;
    } else {
      passkey = this->value_.template_func(x...);
    }
    if (passkey > 999999)
      return;
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_passkey_reply(remote_bda, true, passkey);
  }

  void set_value_template(uint32_t (*func)(Ts...)) {
    this->value_.template_func = func;
    this->has_simple_value_ = false;
  }

  void set_value_simple(const uint32_t &value) {
    this->value_.simple = value;
    this->has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  union {
    uint32_t simple;
    uint32_t (*template_func)(Ts...);
  } value_{.simple = 0};
};

template<typename... Ts> class BLEClientNumericComparisonReplyAction final : public Action<Ts...> {
 public:
  BLEClientNumericComparisonReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(const Ts &...x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    if (has_simple_value_) {
      esp_ble_confirm_reply(remote_bda, this->value_.simple);
    } else {
      esp_ble_confirm_reply(remote_bda, this->value_.template_func(x...));
    }
  }

  void set_value_template(bool (*func)(Ts...)) {
    this->value_.template_func = func;
    this->has_simple_value_ = false;
  }

  void set_value_simple(const bool &value) {
    this->value_.simple = value;
    this->has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  union {
    bool simple;
    bool (*template_func)(Ts...);
  } value_{.simple = false};
};

template<typename... Ts> class BLEClientRemoveBondAction final : public Action<Ts...> {
 public:
  BLEClientRemoveBondAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(const Ts &...x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_remove_bond_device(remote_bda);
  }

 private:
  BLEClient *parent_{nullptr};
};

template<typename... Ts> class BLEClientConnectAction final : public Action<Ts...>, public BLEClientNode {
 public:
  BLEClientConnectAction(BLEClient *ble_client) {
    ble_client->register_ble_node(this);
    ble_client_ = ble_client;
  }
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (this->num_running_ == 0)
      return;
    switch (event) {
      case ESP_GATTC_SEARCH_CMPL_EVT:
        this->node_state = espbt::ClientState::ESTABLISHED;
        this->parent()->run_later([this]() { this->play_next_tuple_(this->var_); });
        break;
      // if the connection is closed, terminate the automation chain.
      case ESP_GATTC_DISCONNECT_EVT:
        this->stop_complex();
        break;
      default:
        break;
    }
  }

  // not used since we override play_complex_
  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    // it makes no sense to have multiple instances of this running at the same time.
    // this would occur only if the same automation was re-triggered while still
    // running. So just cancel the second chain if this is detected.
    if (this->num_running_ != 0) {
      this->stop_complex();
      return;
    }
    this->num_running_++;
    if (this->node_state == espbt::ClientState::ESTABLISHED) {
      this->play_next_(x...);
    } else {
      this->var_ = std::make_tuple(x...);
      this->ble_client_->connect();
    }
  }

 private:
  BLEClient *ble_client_;
  std::tuple<Ts...> var_{};
};

template<typename... Ts> class BLEClientDisconnectAction final : public Action<Ts...>, public BLEClientNode {
 public:
  BLEClientDisconnectAction(BLEClient *ble_client) {
    ble_client->register_ble_node(this);
    ble_client_ = ble_client;
  }
  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    if (this->num_running_ == 0)
      return;
    switch (event) {
      case ESP_GATTC_CLOSE_EVT:
      case ESP_GATTC_DISCONNECT_EVT:
        this->parent()->run_later([this]() { this->play_next_tuple_(this->var_); });
        break;
      default:
        break;
    }
  }

  // not used since we override play_complex_
  void play(const Ts &...x) override {}

  void play_complex(const Ts &...x) override {
    this->num_running_++;
    if (this->node_state == espbt::ClientState::IDLE) {
      this->play_next_(x...);
    } else {
      this->var_ = std::make_tuple(x...);
      this->ble_client_->disconnect();
    }
  }

 private:
  BLEClient *ble_client_;
  std::tuple<Ts...> var_{};
};
}  // namespace esphome::ble_client

#endif
