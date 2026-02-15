#if defined(USE_ZEPHYR)
#include "usb_cdc_acm.h"
#include "esphome/core/log.h"
#include <zephyr/usb/usb_device.h>
#include <zephyr/drivers/uart.h>
#include "esphome/components/zephyr/cdc_acm.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

//==============================================================================
// USBCDCACMInstance Implementation
//==============================================================================
namespace esphome::usb_cdc_acm {

static const char *const TAG = "usb_cdc_acm";

void USBCDCACMInstance::uart_tx_process_() {
  uint8_t *data;
  auto send_len = ring_buf_get_claim(&this->tx_ringbuf_, &data, this->tx_ringbuf_.size);
  if (send_len) {
    send_len = uart_fifo_fill(this->uart_dev_, data, send_len);
    ring_buf_get_finish(&this->tx_ringbuf_, send_len);
  } else {
    uart_irq_tx_disable(this->uart_dev_);
  }
}

void USBCDCACMInstance::uart_rx_process_() {
  uint8_t *data;

  uint32_t total_size = 0;
  uint32_t recv_len = ring_buf_put_claim(&this->rx_ringbuf_, &data, UINT32_MAX);
  if (recv_len) {
    int rx = uart_fifo_read(this->uart_dev_, data, recv_len);
    if (rx < 0) {
      ESP_LOGE(TAG, "Failed to read UART FIFO, err %d", recv_len);
    } else {
      total_size += rx;
    }
  }
  ring_buf_put_finish(&this->rx_ringbuf_, total_size);
}

void USBCDCACMInstance::uart_irq_handler(const device *dev, void *instance) {
  USBCDCACMInstance *thiz = static_cast<USBCDCACMInstance *>(instance);
  while (uart_irq_update(dev) && uart_irq_is_pending(dev)) {
    if (uart_irq_rx_ready(dev)) {
      thiz->uart_rx_process_();
    }

    if (uart_irq_tx_ready(dev)) {
      thiz->uart_tx_process_();
    }
  }
}

void USBCDCACMInstance::setup() {
  if (!device_is_ready(this->uart_dev_)) {
    ESP_LOGE(TAG, "UART Bus %s: is not ready.", this->uart_dev_->name);
    this->get_parent()->mark_failed();
    return;
  }
  usb_enable(nullptr);
  ring_buf_init(&this->rx_ringbuf_, ESPHOME_CDC_RX_RING_BUFFER_SIZE, this->rx_ringbuf_data_);
  ring_buf_init(&this->tx_ringbuf_, ESPHOME_CDC_TX_RING_BUFFER_SIZE, this->tx_ringbuf_data_);
#if defined(CONFIG_CDC_ACM_DTE_RATE_CALLBACK_SUPPORT)
  zephyr::global_cdc_acm.add_on_rate_callback([this](const device *dev, uint32_t bit_rate) {
    if (dev == this->uart_dev_) {
      // Queue event for processing in main loop
      this->queue_line_coding_event(bit_rate, this->stop_bits_, this->parity_, this->data_bits_);
    }
  });
#endif
  uart_irq_callback_user_data_set(this->uart_dev_, uart_irq_handler, this);

  uart_irq_rx_enable(this->uart_dev_);
}

void USBCDCACMInstance::loop() {
  uint32_t dtr = 0;
  uint32_t rts = 0;
  uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_DTR, &dtr);
  uart_line_ctrl_get(this->uart_dev_, UART_LINE_CTRL_RTS, &rts);
  if (dtr != this->dtr_) {
    ESP_LOGD(TAG, "UART Bus %s: %s", this->uart_dev_->name, dtr ? "Opened" : "Closed");
  }
  if (dtr != this->dtr_ || rts != this->rts_) {
    ESP_LOGV(TAG, "Line state device %s: DTR=%d, RTS=%d", this->uart_dev_->name, dtr, rts);
    this->dtr_ = dtr;
    this->rts_ = rts;
    // Queue event for processing in main loop
    this->queue_line_state_event(dtr != 0, rts != 0);
  }
  // Process events from the lock-free queue
  this->process_events_();
}

void USBCDCACMInstance::dump_config() {
  ESP_LOGCONFIG(TAG,
                "UART Bus %s:\n"
                "  Opened: %s\n"
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u",
                this->uart_dev_->name, YESNO(this->dtr_), this->baud_rate_, this->data_bits_,
                LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);
  this->check_logger_conflict();
}

void USBCDCACMInstance::check_logger_conflict() {
#ifdef USE_LOGGER
  if (logger::global_logger->get_baud_rate() == 0 || this->uart_dev_ == nullptr) {
    return;
  }

  if (this->uart_dev_ == logger::global_logger->get_hw_serial()) {
    ESP_LOGW(TAG, "  You're using the same serial port for logging and the UART component. Please "
                  "disable logging over the serial port by setting logger->baud_rate to 0.");
  }
#endif
}

size_t USBCDCACMInstance::available() {
  int size = ring_buf_size_get(&this->rx_ringbuf_);
  ESP_LOGVV(TAG, "UART Bus %s: available %d", this->uart_dev_->name, size);
  return size;
}

bool USBCDCACMInstance::read_array(uint8_t *data, size_t len) {
  if (len == 0) {
    return true;
  }
  if ((available() + (this->has_peek_ ? 1 : 0)) < len) {
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

  ring_buf_get(&this->rx_ringbuf_, data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

void USBCDCACMInstance::flush() {
  uart_irq_tx_enable(this->uart_dev_);
  while (!ring_buf_is_empty(&this->tx_ringbuf_)) {
    delay(1);
  }
}

void USBCDCACMInstance::write_array(const uint8_t *data, size_t len) {
  if (!device_is_ready(this->uart_dev_)) {
    return;
  }
  auto recv_len = ring_buf_put(&this->tx_ringbuf_, data, len);
  if (recv_len < len) {
    ESP_LOGE(TAG, "TX ring buffer full. Dropping %zu bytes", len - recv_len);
  }
  uart_irq_tx_enable(this->uart_dev_);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(uart::UART_DIRECTION_TX, data[i]);
  }
#endif
}

}  // namespace esphome::usb_cdc_acm
#endif
