#pragma once
#ifdef USE_ZEPHYR
#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif
#include <shell/shell_bt_nus.h>
#include <atomic>

namespace esphome::ble_nus {

class BLENUS : public Component
#ifdef USE_LOGGER
    ,
               public logger::LogListener
#endif
{
  enum TxStatus {
    TX_DISABLED,
    TX_ENABLED,
    TX_BUSY,
  };

 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  size_t write_array(const uint8_t *data, size_t len);
  void set_expose_log(bool expose_log) { this->expose_log_ = expose_log; }
#ifdef USE_LOGGER
  void on_log(uint8_t level, const char *tag, const char *message, size_t message_len) override;
#endif

 protected:
  static void send_enabled_callback(bt_nus_send_status status);
  static void tx_callback(bt_conn *conn);
  static void rx_callback(bt_conn *conn, const uint8_t *data, uint16_t len);
  static void connected(bt_conn *conn, uint8_t err);
  static void disconnected(bt_conn *conn, uint8_t reason);

  std::atomic<bt_conn *> conn_ = nullptr;
  bool expose_log_ = false;
  atomic_t tx_status_ = ATOMIC_INIT(TX_DISABLED);
};

}  // namespace esphome::ble_nus
#endif
