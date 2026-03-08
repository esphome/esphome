#ifdef USE_ZEPHYR
#include "ble_nus.h"
#include <zephyr/kernel.h>
#include <bluetooth/services/nus.h>
#include "esphome/core/log.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#include "esphome/core/application.h"
#endif
#include <zephyr/sys/ring_buffer.h>

namespace esphome::ble_nus {

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
BLENUS *global_ble_nus;
RING_BUF_DECLARE(global_ble_tx_ring_buf, ESPHOME_BLE_NUS_TX_RING_BUFFER_SIZE);
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
RING_BUF_DECLARE(global_ble_rx_ring_buf, ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE);
#endif
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

static const char *const TAG = "ble_nus";

void BLENUS::write_array(const uint8_t *data, size_t len) {
  if (atomic_get(&this->tx_status_) == TX_DISABLED) {
    return;
  }
  auto sent = ring_buf_put(&global_ble_tx_ring_buf, data, len);
  if (sent < len) {
    ESP_LOGE(TAG, "TX dropping %u bytes", len - sent);
    return;
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool BLENUS::peek_byte(uint8_t *data) {
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
  if (this->has_peek_) {
    *data = this->peek_buffer_;
    return true;
  }

  if (this->read_byte(&this->peek_buffer_)) {
    *data = this->peek_buffer_;
    this->has_peek_ = true;
    return true;
  }

  return false;
#else
  return false;
#endif
}

bool BLENUS::read_array(uint8_t *data, size_t len) {
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
  if (len == 0) {
    return true;
  }
  if (this->available() < len) {
    return false;
  }

  // First, use the peek buffer if available
  if (this->has_peek_) {
    data[0] = this->peek_buffer_;
    this->has_peek_ = false;
    data++;
    if (--len == 0) {  // Decrement len first, then check it...
      return true;     // No more to read
    }
  }

  if (ring_buf_get(&global_ble_rx_ring_buf, data, len) != len) {
    ESP_LOGE(TAG, "UART BLE unexpected size");
    return false;
  }
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
#else
  return false;
#endif
}

size_t BLENUS::available() {
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
  uint32_t size = ring_buf_size_get(&global_ble_rx_ring_buf);
  ESP_LOGVV(TAG, "UART BLE available %u", size);
  return size + (this->has_peek_ ? 1 : 0);
#else
  return 0;
#endif
}

uart::FlushResult BLENUS::flush() {
  constexpr uint32_t timeout_5sec = 5000;
  uint32_t start = millis();
  while (atomic_get(&this->tx_status_) != TX_DISABLED && !ring_buf_is_empty(&global_ble_tx_ring_buf)) {
    if (millis() - start > timeout_5sec) {
      ESP_LOGW(TAG, "Flush timeout");
      return uart::FlushResult::TIMEOUT;
    }
    delay(1);
  }
  return uart::FlushResult::SUCCESS;
}

void BLENUS::connected(bt_conn *conn, uint8_t err) {
  if (err == 0) {
    global_ble_nus->conn_.store(bt_conn_ref(conn));
    global_ble_nus->connected_ = true;
  }
}

void BLENUS::disconnected(bt_conn *conn, uint8_t reason) {
  if (global_ble_nus->conn_) {
    bt_conn_unref(global_ble_nus->conn_.load());
    // Connection array is global static.
    // Reference can be kept even if disconnected.
    global_ble_nus->connected_ = false;
  }
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
  ESP_LOGV(TAG, "Received %d bytes.", len);
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
  auto recv_len = ring_buf_put(&global_ble_rx_ring_buf, data, len);
  if (recv_len < len) {
    ESP_LOGE(TAG, "RX dropping %u bytes", len - recv_len);
  }
#endif
}
void BLENUS::setup() {
#ifdef ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE
  this->rx_buffer_size_ = ESPHOME_BLE_NUS_RX_RING_BUFFER_SIZE;
#endif
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
    logger::global_logger->add_log_callback(
        this, [](void *self, uint8_t level, const char *tag, const char *message, size_t message_len) {
          static_cast<BLENUS *>(self)->on_log(level, tag, message, message_len);
        });
  }
#endif
}

#ifdef USE_LOGGER
void BLENUS::on_log(uint8_t level, const char *tag, const char *message, size_t message_len) {
  (void) level;
  (void) tag;
  this->write_array(reinterpret_cast<const uint8_t *>(message), message_len);
  const char c = '\n';
  this->write_array(reinterpret_cast<const uint8_t *>(&c), 1);
}
#endif

void BLENUS::dump_config() {
  uint32_t mtu = 0;
  bt_conn *conn = this->conn_.load();
  if (conn && this->connected_) {
    mtu = bt_nus_get_mtu(conn);
  }
  ESP_LOGCONFIG(TAG,
                "ble nus:\n"
                "  log: %s\n"
                "  connected: %s\n"
                "  MTU: %u",
                YESNO(this->expose_log_), YESNO(this->connected_.load()), mtu);
}

void BLENUS::loop() {
  if (ring_buf_is_empty(&global_ble_tx_ring_buf)) {
    return;
  }

  if (!atomic_cas(&this->tx_status_, TX_ENABLED, TX_BUSY)) {
    if (atomic_get(&this->tx_status_) == TX_DISABLED) {
      ring_buf_reset(&global_ble_tx_ring_buf);
    }
    return;
  }

  bt_conn *conn = this->conn_.load();
  if (conn) {
    conn = bt_conn_ref(conn);
  }

  if (nullptr == conn) {
    atomic_cas(&this->tx_status_, TX_BUSY, TX_ENABLED);
    return;
  }

  uint32_t req_len = bt_nus_get_mtu(conn);

  uint8_t *buf;
  uint32_t size = ring_buf_get_claim(&global_ble_tx_ring_buf, &buf, req_len);

  int err, err2;

  err = bt_nus_send(conn, buf, size);
  err2 = ring_buf_get_finish(&global_ble_tx_ring_buf, size);
  if (err2) {
    // It should no happen.
    ESP_LOGE(TAG, "Size %u exceeds valid bytes in the ring buffer (%d error)", size, err2);
  }
  if (err == 0) {
    ESP_LOGVV(TAG, "Sent %d bytes", size);
  } else {
    ESP_LOGE(TAG, "Failed to send %d bytes (%d error)", size, err);
    atomic_cas(&this->tx_status_, TX_BUSY, TX_ENABLED);
  }
  bt_conn_unref(conn);
}

}  // namespace esphome::ble_nus
#endif
