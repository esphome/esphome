#ifdef USE_ZEPHYR
#include "ble_nus.h"
#include <zephyr/kernel.h>
#include <bluetooth/services/nus.h>
#include "esphome/core/log.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#endif

namespace esphome {
namespace ble_nus {

BLENUS *global_ble_nus;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

static const char *const TAG = "ble_nus";

size_t BLENUS::write_array(const uint8_t *data, size_t len) {
  if (atomic_get(&this->tx_status_) == TX_DISABLED) {
    return 0;
  }
  return ring_buf_put(&this->tx_ringbuf_, data, len);
}

void BLENUS::connected(bt_conn *conn, uint8_t err) {
  if (err == 0) {
    global_ble_nus->conn_ = bt_conn_ref(conn);
  }
}

void BLENUS::disconnected(bt_conn *conn, uint8_t reason) {
  bt_conn_unref(global_ble_nus->conn_);
  global_ble_nus->conn_ = nullptr;
}

void BLENUS::tx_callback(bt_conn *conn) {
  atomic_cas(&global_ble_nus->tx_status_, TX_BUSY, TX_ENABLED);
  ESP_LOGVV(TAG, "Sent operation completed");
}

void BLENUS::send_enabled_callback(bt_nus_send_status status) {
  switch (status) {
    case BT_NUS_SEND_STATUS_ENABLED:
      atomic_set(&global_ble_nus->tx_status_, TX_ENABLED);
#ifdef USE_LOGGER
      if (global_ble_nus->expose_log_) {
        App.schedule_dump_config();
      }
#endif
      ESP_LOGD(TAG, "NUS notification has been enabled");
      break;
    case BT_NUS_SEND_STATUS_DISABLED:
      atomic_set(&global_ble_nus->tx_status_, TX_DISABLED);
      ESP_LOGD(TAG, "NUS notification has been disabled");
      break;
  }
}

void BLENUS::rx_callback(bt_conn *conn, const uint8_t *const data, uint16_t len) {
  ESP_LOGD(TAG, "Received %d bytes.", len);
}

BLENUS::BLENUS(size_t buffer_size) {
  uint8_t *buffer = new uint8_t[buffer_size];
  ring_buf_init(&this->tx_ringbuf_, buffer_size, buffer);
}

void BLENUS::setup() {
  bt_nus_cb callbacks = {
      .received = rx_callback,
      .sent = tx_callback,
      .send_enabled = send_enabled_callback,
  };

  bt_nus_init(&callbacks);

  static bt_conn_cb conn_callbacks = {
      .connected = BLENUS::connected,
      .disconnected = BLENUS::disconnected,
  };

  bt_conn_cb_register(&conn_callbacks);

  global_ble_nus = this;
#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->expose_log_) {
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *message, size_t message_len) {
          this->write_array(reinterpret_cast<const uint8_t *>(message), message_len);
          const char c = '\n';
          this->write_array(reinterpret_cast<const uint8_t *>(&c), 1);
        });
  }

#endif
}

void BLENUS::dump_config() {
  ESP_LOGCONFIG(TAG, "ble nus:");
  ESP_LOGCONFIG(TAG, "  log: %s", YESNO(this->expose_log_));
}

void BLENUS::loop() {
  if (ring_buf_is_empty(&this->tx_ringbuf_)) {
    return;
  }

  if (!atomic_cas(&this->tx_status_, TX_ENABLED, TX_BUSY)) {
    if (atomic_get(&this->tx_status_) == TX_DISABLED) {
      ring_buf_reset(&this->tx_ringbuf_);
    }
    return;
  }

  bt_conn *conn = bt_conn_ref(conn_);

  if (nullptr == conn) {
    atomic_cas(&this->tx_status_, TX_BUSY, TX_ENABLED);
    return;
  }

  uint32_t req_len = bt_nus_get_mtu(conn);

  uint8_t *buf;
  uint32_t size = ring_buf_get_claim(&this->tx_ringbuf_, &buf, req_len);

  int err, err2;

  err = bt_nus_send(conn, buf, size);
  err2 = ring_buf_get_finish(&this->tx_ringbuf_, size);
  if (err2) {
    ESP_LOGE(TAG, "Failed to ring buf finish (%d error)", err2);
  }
  if (err == 0) {
    ESP_LOGVV(TAG, "Sent %d bytes", size);
  } else {
    ESP_LOGE(TAG, "Failed to send %d bytes (%d error)", size, err);
    atomic_cas(&this->tx_status_, TX_BUSY, TX_ENABLED);
  }
  bt_conn_unref(conn);
}

}  // namespace ble_nus
}  // namespace esphome
#endif
