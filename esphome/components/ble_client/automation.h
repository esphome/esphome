#pragma once

#ifdef USE_ESP32

#include <utility>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/components/ble_client/ble_client.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ble_client {

// placeholder class for static TAG .
class Automation {
 public:
  // could be made inline with C++17
  static const char *const TAG;
};

// implement on_connect automation.
class BLEClientConnectTrigger : public Trigger<>, public BLEClientNode {
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
class BLEClientDisconnectTrigger : public Trigger<>, public BLEClientNode {
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

class BLEClientPasskeyRequestTrigger : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_REQ_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr))
      this->trigger();
  }
};

class BLEClientPasskeyNotificationTrigger : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientPasskeyNotificationTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_PASSKEY_NOTIF_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
      this->trigger(param->ble_security.key_notif.passkey);
    }
  }
};

class BLEClientNumericComparisonRequestTrigger : public Trigger<uint32_t>, public BLEClientNode {
 public:
  explicit BLEClientNumericComparisonRequestTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void loop() override {}
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override {
    if (event == ESP_GAP_BLE_NC_REQ_EVT && this->parent_->check_addr(param->ble_security.auth_cmpl.bd_addr)) {
      this->trigger(param->ble_security.key_notif.passkey);
    }
  }
};

// implement the ble_client.ble_write action.
template<typename... Ts> class BLEClientWriteAction : public Action<Ts...>, public BLEClientNode {
 public:
  BLEClientWriteAction(BLEClient *ble_client) {
    ble_client->register_ble_node(this);
    ble_client_ = ble_client;
  }

  void set_service_uuid16(uint16_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->service_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_char_uuid16(uint16_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint16(uuid); }
  void set_char_uuid32(uint32_t uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_uint32(uuid); }
  void set_char_uuid128(uint8_t *uuid) { this->char_uuid_ = espbt::ESPBTUUID::from_raw(uuid); }

  void set_value_template(std::function<std::vector<uint8_t>(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const std::vector<uint8_t> &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

  void play(Ts... x) override {}

  void play_complex(Ts... x) override {
    this->num_running_++;
    this->var_ = std::make_tuple(x...);
    auto value = this->has_simple_value_ ? this->value_simple_ : this->value_template_(x...);
    // on write failure, continue the automation chain rather than stopping so that e.g. disconnect can work.
    if (!write(value))
      this->play_next_(x...);
  }

  /**
   * Note about logging: the esph_log_X macros are used here because the CI checks complain about use of the ESP LOG
   * macros in header files (Can't even write it in a comment!)
   * Not sure why, because they seem to work just fine.
   * The problem is that the implementation of a templated class can't be placed in a .cpp file when using C++ less than
   * 17, so the methods have to be here.  The esph_log_X macros are equivalent in function, but don't trigger the CI
   * errors.
   */
  // initiate the write. Return true if all went well, will be followed by a WRITE_CHAR event.
  bool write(const std::vector<uint8_t> &value) {
    if (this->node_state != espbt::ClientState::ESTABLISHED) {
      esph_log_w(Automation::TAG, "Cannot write to BLE characteristic - not connected");
      return false;
    }
    esph_log_vv(Automation::TAG, "Will write %d bytes: %s", value.size(), format_hex_pretty(value).c_str());
    esp_err_t err = esp_ble_gattc_write_char(this->parent()->get_gattc_if(), this->parent()->get_conn_id(),
                                             this->char_handle_, value.size(), const_cast<uint8_t *>(value.data()),
                                             this->write_type_, ESP_GATT_AUTH_REQ_NONE);
    if (err != ESP_OK) {
      esph_log_e(Automation::TAG, "Error writing to characteristic: %s!", esp_err_to_name(err));
      return false;
    }
    return true;
  }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override {
    switch (event) {
      case ESP_GATTC_WRITE_CHAR_EVT:
        // upstream code checked the MAC address, verify the characteristic.
        if (param->write.handle == this->char_handle_)
          this->parent()->run_later([this]() { this->play_next_tuple_(this->var_); });
        break;
      case ESP_GATTC_DISCONNECT_EVT:
        if (this->num_running_ != 0)
          this->stop_complex();
        break;
      case ESP_GATTC_SEARCH_CMPL_EVT: {
        auto *chr = this->parent()->get_characteristic(this->service_uuid_, this->char_uuid_);
        if (chr == nullptr) {
          esph_log_w("ble_write_action", "Characteristic %s was not found in service %s",
                     this->char_uuid_.to_string().c_str(), this->service_uuid_.to_string().c_str());
          break;
        }
        this->char_handle_ = chr->handle;
        this->char_props_ = chr->properties;
        if (this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE) {
          this->write_type_ = ESP_GATT_WRITE_TYPE_RSP;
          esph_log_d(Automation::TAG, "Write type: ESP_GATT_WRITE_TYPE_RSP");
        } else if (this->char_props_ & ESP_GATT_CHAR_PROP_BIT_WRITE_NR) {
          this->write_type_ = ESP_GATT_WRITE_TYPE_NO_RSP;
          esph_log_d(Automation::TAG, "Write type: ESP_GATT_WRITE_TYPE_NO_RSP");
        } else {
          esph_log_e(Automation::TAG, "Characteristic %s does not allow writing", this->char_uuid_.to_string().c_str());
          break;
        }
        this->node_state = espbt::ClientState::ESTABLISHED;
        esph_log_d(Automation::TAG, "Found characteristic %s on device %s", this->char_uuid_.to_string().c_str(),
                   ble_client_->address_str().c_str());
        break;
      }
      default:
        break;
    }
  }

 private:
  BLEClient *ble_client_;
  bool has_simple_value_ = true;
  std::vector<uint8_t> value_simple_;
  std::function<std::vector<uint8_t>(Ts...)> value_template_{};
  espbt::ESPBTUUID service_uuid_;
  espbt::ESPBTUUID char_uuid_;
  std::tuple<Ts...> var_{};
  uint16_t char_handle_{};
  esp_gatt_char_prop_t char_props_{};
  esp_gatt_write_type_t write_type_{};
};

template<typename... Ts> class BLEClientPasskeyReplyAction : public Action<Ts...> {
 public:
  BLEClientPasskeyReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    uint32_t passkey;
    if (has_simple_value_) {
      passkey = this->value_simple_;
    } else {
      passkey = this->value_template_(x...);
    }
    if (passkey > 999999)
      return;
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_passkey_reply(remote_bda, true, passkey);
  }

  void set_value_template(std::function<uint32_t(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const uint32_t &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  uint32_t value_simple_{0};
  std::function<uint32_t(Ts...)> value_template_{};
};

template<typename... Ts> class BLEClientNumericComparisonReplyAction : public Action<Ts...> {
 public:
  BLEClientNumericComparisonReplyAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    if (has_simple_value_) {
      esp_ble_confirm_reply(remote_bda, this->value_simple_);
    } else {
      esp_ble_confirm_reply(remote_bda, this->value_template_(x...));
    }
  }

  void set_value_template(std::function<bool(Ts...)> func) {
    this->value_template_ = std::move(func);
    has_simple_value_ = false;
  }

  void set_value_simple(const bool &value) {
    this->value_simple_ = value;
    has_simple_value_ = true;
  }

 private:
  BLEClient *parent_{nullptr};
  bool has_simple_value_ = true;
  bool value_simple_{false};
  std::function<bool(Ts...)> value_template_{};
};

template<typename... Ts> class BLEClientRemoveBondAction : public Action<Ts...> {
 public:
  BLEClientRemoveBondAction(BLEClient *ble_client) { parent_ = ble_client; }

  void play(Ts... x) override {
    esp_bd_addr_t remote_bda;
    memcpy(remote_bda, parent_->get_remote_bda(), sizeof(esp_bd_addr_t));
    esp_ble_remove_bond_device(remote_bda);
  }

 private:
  BLEClient *parent_{nullptr};
};

template<typename... Ts> class BLEClientConnectAction : public Action<Ts...>, public BLEClientNode {
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
  void play(Ts... x) override {}

  void play_complex(Ts... x) override {
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

template<typename... Ts> class BLEClientDisconnectAction : public Action<Ts...>, public BLEClientNode {
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
  void play(Ts... x) override {}

  void play_complex(Ts... x) override {
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
}  // namespace ble_client
}  // namespace esphome

#endif
