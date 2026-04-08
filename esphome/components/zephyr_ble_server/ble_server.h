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
  void numeric_comparison_reply(bool accept);

 protected:
  static void connected(bt_conn *conn, uint8_t err);
  static void disconnected(bt_conn *conn, uint8_t reason);
  static void auth_passkey_confirm(bt_conn *conn, unsigned int passkey);
  bt_conn *conn_{};
  CallbackManager<void(uint32_t)> passkey_cb_;
};

template<typename... Ts> class BLENumericComparisonReplyAction : public Action<Ts...> {
 public:
  explicit BLENumericComparisonReplyAction(BLEServer *parent) : parent_(parent) {}

  TEMPLATABLE_VALUE(bool, accept)

  void play(const Ts &...x) override { this->parent_->numeric_comparison_reply(this->accept_.value(x...)); }

 protected:
  BLEServer *parent_;
};

}  // namespace esphome::zephyr_ble_server
#endif
