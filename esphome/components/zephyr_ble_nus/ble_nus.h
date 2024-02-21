#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include <zephyr/sys/ring_buffer.h>
#include <shell/shell_bt_nus.h>

namespace esphome {
namespace zephyr_ble_nus {

class BLENUS : public Component {
  enum tx_status {
    TX_DISABLED,
    TX_ENABLED,
    TX_BUSY,
  };

 public:
  BLENUS(size_t buffer_size = 1024);
  void setup() override;
  void dump_config() override;
  void loop() override;
  size_t write_array(const uint8_t *data, size_t len);
  void set_expose_log(bool expose_log) { this->expose_log_ = expose_log; }

 protected:
  static void send_enabled_callback_(bt_nus_send_status status);
  static void tx_callback_(bt_conn *conn);
  static void rx_callback_(bt_conn *conn, const uint8_t *const data, uint16_t len);
  static void connected_(bt_conn *conn, uint8_t err);
  static void disconnected_(bt_conn *conn, uint8_t reason);

  bt_conn *conn_ = nullptr;
  ring_buf tx_ringbuf_;
  bool expose_log_ = false;
  atomic_t tx_status_ = ATOMIC_INIT(TX_DISABLED);
};

}  // namespace zephyr_ble_nus
}  // namespace esphome
