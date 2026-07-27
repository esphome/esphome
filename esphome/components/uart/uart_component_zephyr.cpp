#ifdef USE_ZEPHYR
#include "uart_component_zephyr.h"
#include "esphome/core/log.h"

#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>

namespace esphome::uart {

static const char *const TAG = "uart.zephyr";

void ZephyrUartComponent::uart_irq_handler_s(const struct device *dev, void *user_data) {
  static_cast<ZephyrUartComponent *>(user_data)->uart_irq_handler_();
}

void ZephyrUartComponent::uart_irq_handler_() {
  if (!uart_irq_update(this->uart_dev_)) {
    return;
  }
  while (uart_irq_rx_ready(this->uart_dev_)) {
    uint8_t buf[64];
    int len = uart_fifo_read(this->uart_dev_, buf, sizeof(buf));
    if (len < 0) {
      break;
    }
    uint32_t written = ring_buf_put(&this->rx_rb_, buf, static_cast<uint32_t>(len));
    if (written < static_cast<uint32_t>(len)) {
      this->rx_overflow_count_ += static_cast<uint32_t>(len) - written;
    }
  }
}

void ZephyrUartComponent::setup() {
  const struct device *dev = nullptr;
  switch (this->port_) {
    case ZEPHYR_UART_PORT_0:
      dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart0));
      break;
    case ZEPHYR_UART_PORT_1:
      dev = DEVICE_DT_GET_OR_NULL(DT_NODELABEL(uart1));
      break;
    default:
      __builtin_unreachable();
  }

  if (dev == nullptr || !device_is_ready(dev)) {
    ESP_LOGE(TAG, "UART device uart%u not ready", static_cast<unsigned>(this->port_));
    this->mark_failed();
    return;
  }
  struct uart_config cfg;
  cfg.baudrate = this->baud_rate_;
  cfg.parity = (this->parity_ == UART_CONFIG_PARITY_EVEN)  ? UART_CFG_PARITY_EVEN
               : (this->parity_ == UART_CONFIG_PARITY_ODD) ? UART_CFG_PARITY_ODD
                                                           : UART_CFG_PARITY_NONE;
  cfg.stop_bits = (this->stop_bits_ == 2) ? UART_CFG_STOP_BITS_2 : UART_CFG_STOP_BITS_1;
  switch (this->data_bits_) {
    case 5:
      cfg.data_bits = UART_CFG_DATA_BITS_5;
      break;
    case 6:
      cfg.data_bits = UART_CFG_DATA_BITS_6;
      break;
    case 7:
      cfg.data_bits = UART_CFG_DATA_BITS_7;
      break;
    default:
      cfg.data_bits = UART_CFG_DATA_BITS_8;
      break;
  }
  cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;

  int ret = uart_configure(dev, &cfg);
  if (ret == -ENOSYS) {
    // native_sim PTY driver does not support uart_configure — use driver defaults
    ESP_LOGD(TAG, "uart_configure not supported by driver (e.g. native_sim PTY) — using driver defaults");
  } else if (ret != 0) {
    ESP_LOGW(TAG, "uart_configure failed: %d", ret);
  }

  this->rx_buf_mem_ = std::make_unique<uint8_t[]>(this->rx_buffer_size_);
  ring_buf_init(&this->rx_rb_, static_cast<uint32_t>(this->rx_buffer_size_), this->rx_buf_mem_.get());
  this->uart_dev_ = dev;
  compiler_barrier();  // ensure uart_dev_ write is visible before IRQ is armed
  uart_irq_callback_user_data_set(dev, uart_irq_handler_s, this);
  uart_irq_rx_enable(dev);
}

void ZephyrUartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART (Zephyr):");
  ESP_LOGCONFIG(TAG, "  Port: uart%u", static_cast<unsigned>(this->port_));
  if (this->uart_dev_ == nullptr) {
    ESP_LOGCONFIG(TAG, "  Status: NOT READY");
    return;
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %u bps", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop Bits: %u", this->stop_bits_);
  ESP_LOGCONFIG(TAG, "  RX Buffer: %zu bytes", this->rx_buffer_size_);
  if (this->rx_overflow_count_ > 0) {
    ESP_LOGW(TAG, "  RX Overflow: %u byte(s) dropped", this->rx_overflow_count_.load());
  }
}

void ZephyrUartComponent::write_array(const uint8_t *data, size_t len) {
  if (this->uart_dev_ == nullptr) {
    return;
  }
  for (size_t i = 0; i < len; i++) {
    uart_poll_out(this->uart_dev_, data[i]);
#ifdef USE_UART_DEBUGGER
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
#endif
  }
}

bool ZephyrUartComponent::peek_byte(uint8_t *data) {
  if (this->uart_dev_ == nullptr) {
    return false;
  }
  unsigned int key = irq_lock();
  bool result = !ring_buf_is_empty(&this->rx_rb_) && ring_buf_peek(&this->rx_rb_, data, 1) == 1;
  irq_unlock(key);
  return result;
}

bool ZephyrUartComponent::read_array(uint8_t *data, size_t len) {
  if (this->uart_dev_ == nullptr || len == 0) {
    return false;
  }

  size_t i = 0;
  while (i < len) {
    if (!this->check_read_timeout_()) {
      return false;
    }
    unsigned int key = irq_lock();
    uint32_t got = ring_buf_get(&this->rx_rb_, data + i, static_cast<uint32_t>(len - i));
    irq_unlock(key);
    if (got == 0) {
      return false;
    }
#ifdef USE_UART_DEBUGGER
    for (uint32_t j = 0; j < got; j++) {
      this->debug_callback_.call(UART_DIRECTION_RX, data[i + j]);
    }
#endif
    i += got;
  }
  return true;
}

size_t ZephyrUartComponent::available() {
  if (this->uart_dev_ == nullptr) {
    return 0;
  }
  unsigned int key = irq_lock();
  uint32_t count = ring_buf_size_get(&this->rx_rb_);
  irq_unlock(key);
  return count;
}

UARTFlushResult ZephyrUartComponent::flush() {
  // uart_poll_out is synchronous — all bytes already written by write_array
  return UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

}  // namespace esphome::uart
#endif  // USE_ZEPHYR
