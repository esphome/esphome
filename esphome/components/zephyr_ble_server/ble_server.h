#pragma once
#ifdef USE_ZEPHYR
#include "esphome/core/component.h"
#include <zephyr/bluetooth/conn.h>
#include "esphome/core/automation.h"

namespace esphome::zephyr_ble_server {

class BLEServer : public Component {
 public:
  void setup() override;
  void dump_config() override;
  template<typename F> void add_passkey_callback(F &&callback) { this->passkey_cb_.add(std::forward<F>(callback)); }
  void bt_conn_auth(bool confirm);

 protected:
  static void connected(bt_conn *conn, uint8_t err);
  static void disconnected(bt_conn *conn, uint8_t reason);
  static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey);
  bt_conn *conn_{};
  CallbackManager<void(uint32_t)> passkey_cb_;
};

template<typename... Ts> class BLENumericComparisonReplyAction : public Action<Ts...> {
 public:
  BLENumericComparisonReplyAction(BLEServer *parent) { this->parent_ = parent; }

  void play(const Ts &...x) override {
    if (has_simple_value_) {
      this->parent_->bt_conn_auth(this->value_.simple);
    } else {
      this->parent_->bt_conn_auth(this->value_.template_func(x...));
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
  BLEServer *parent_{nullptr};
  bool has_simple_value_ = true;
  union {
    bool simple;
    bool (*template_func)(Ts...);
  } value_{.simple = false};
};

}  // namespace esphome::zephyr_ble_server
#endif
