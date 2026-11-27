#ifdef USE_ZEPHYR
#include "uart_component_zephyr.h"

#include <zephyr/drivers/uart.h>

#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/wake.h"

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart.zephyr");

void ZephyrUARTComponent::setup() {
  if (this->device_ == nullptr || !device_is_ready(this->device_)) {
    this->mark_failed(LOG_STR("UART device not ready"));
    return;
  }
  if (!this->configure_()) {
    this->mark_failed(LOG_STR("UART configuration not supported"));
    return;
  }
  if (this->rx_pin_ != nullptr) {
    this->rx_storage_ = new uint8_t[this->rx_buffer_size_];  // NOLINT(cppcoreguidelines-owning-memory)
    ring_buf_init(&this->rx_ring_, this->rx_buffer_size_, this->rx_storage_);
    uart_irq_callback_user_data_set(this->device_, ZephyrUARTComponent::rx_isr, this);
    uart_irq_rx_enable(this->device_);
  }
}

bool ZephyrUARTComponent::configure_() {
  struct uart_config config{};
  config.baudrate = this->baud_rate_;
  switch (this->parity_) {
    case UART_CONFIG_PARITY_NONE:
      config.parity = UART_CFG_PARITY_NONE;
      break;
    case UART_CONFIG_PARITY_EVEN:
      config.parity = UART_CFG_PARITY_EVEN;
      break;
    default:
      ESP_LOGE(TAG, "Parity %s is not supported", LOG_STR_ARG(parity_to_str(this->parity_)));
      return false;
  }
  config.stop_bits = this->stop_bits_ == 2 ? UART_CFG_STOP_BITS_2 : UART_CFG_STOP_BITS_1;
  config.data_bits = static_cast<uint8_t>(UART_CFG_DATA_BITS_5 + (this->data_bits_ - 5));
  config.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
  int err = uart_configure(this->device_, &config);
  if (err != 0) {
    ESP_LOGE(TAG, "uart_configure() failed for %" PRIu32 " baud, %u data bits, parity %s, %u stop bits: %d",
             this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_, err);
    return false;
  }
  return true;
}

void ZephyrUARTComponent::load_settings(bool dump_config) {
  if (this->is_failed() || this->device_ == nullptr || !device_is_ready(this->device_) || !this->configure_())
    return;
  if (dump_config) {
    ESP_LOGCONFIG(TAG, "UART bus was reloaded.");
    this->dump_config();
  }
}

void ZephyrUARTComponent::rx_isr(const struct device *device, void *user_data) {
  auto *self = static_cast<ZephyrUARTComponent *>(user_data);
  bool received = false;
  while (uart_irq_update(device) > 0 && uart_irq_rx_ready(device) > 0) {
    uint8_t *space;
    uint32_t claimed = ring_buf_put_claim(&self->rx_ring_, &space, self->rx_buffer_size_);
    if (claimed == 0) {
      // The buffer is full: drain the FIFO so the interrupt clears, and count what is lost
      uint8_t discard[16];
      int dropped = uart_fifo_read(device, discard, sizeof(discard));
      if (dropped <= 0)
        break;
      self->rx_overflow_ = self->rx_overflow_ + dropped;
      continue;
    }
    int got = uart_fifo_read(device, space, claimed);
    ring_buf_put_finish(&self->rx_ring_, got > 0 ? got : 0);
    if (got <= 0)
      break;
    received = true;
  }
  if (received)
    wake_loop_any_context();
}

void ZephyrUARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus:");
  ESP_LOGCONFIG(TAG, "  Device: %s", this->device_ != nullptr ? this->device_->name : "none");
  LOG_PIN("  TX Pin: ", this->tx_pin_);
  LOG_PIN("  RX Pin: ", this->rx_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", (unsigned) this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG,
                "  Baud Rate: %" PRIu32 " baud\n"
                "  Data Bits: %u\n"
                "  Parity: %s\n"
                "  Stop bits: %u",
                this->baud_rate_, this->data_bits_, LOG_STR_ARG(parity_to_str(this->parity_)), this->stop_bits_);
  this->check_logger_conflict();
}

void ZephyrUARTComponent::write_array(const uint8_t *data, size_t len) {
  if (this->is_failed() || this->device_ == nullptr || this->tx_pin_ == nullptr || len == 0)
    return;
  // uart_poll_out() waits for the previous byte, so a long write at a low baud rate outlasts the watchdog
  for (size_t i = 0; i < len; i++) {
    uart_poll_out(this->device_, data[i]);
    App.feed_wdt();
  }
  this->last_tx_start_us_ = micros();
  this->tx_pending_ = true;
#ifdef USE_UART_DEBUGGER
  // After the whole array, so that a slow callback does not split it
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool ZephyrUARTComponent::peek_byte(uint8_t *data) {
  if (!this->has_peek_) {
    if (!this->check_read_timeout_())
      return false;
    if (ring_buf_get(&this->rx_ring_, &this->peek_buffer_, 1) != 1)
      return false;
    this->has_peek_ = true;
  }
  *data = this->peek_buffer_;
  return true;
}

bool ZephyrUARTComponent::read_array(uint8_t *data, size_t len) {
  if (len == 0)
    return true;
  if (!this->check_read_timeout_(len))
    return false;
  size_t offset = 0;
  if (this->has_peek_) {
    data[0] = this->peek_buffer_;
    this->has_peek_ = false;
    offset = 1;
  }
  if (offset < len)
    ring_buf_get(&this->rx_ring_, data + offset, len - offset);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  uint32_t overflow = this->rx_overflow_;
  if (overflow != this->rx_overflow_reported_) {
    ESP_LOGW(TAG, "RX buffer full: %" PRIu32 " bytes lost", overflow - this->rx_overflow_reported_);
    this->rx_overflow_reported_ = overflow;
  }
  return true;
}

size_t ZephyrUARTComponent::available() {
  if (this->rx_storage_ == nullptr)
    return 0;
  return ring_buf_size_get(&this->rx_ring_) + (this->has_peek_ ? 1 : 0);
}

UARTFlushResult ZephyrUARTComponent::flush() {
  if (!this->tx_pending_)
    return UARTFlushResult::UART_FLUSH_RESULT_SUCCESS;
  this->tx_pending_ = false;
  // uart_poll_out() returns once the last byte has started: wait for its start, data, parity and stop bits,
  // plus one bit
  const uint32_t bits =
      1 + this->data_bits_ + (this->parity_ == UART_CONFIG_PARITY_NONE ? 0 : 1) + this->stop_bits_ + 1;
  const uint32_t frame_us = (bits * 1000000 + this->baud_rate_ - 1) / this->baud_rate_;
  const uint32_t elapsed = micros() - this->last_tx_start_us_;
  if (elapsed < frame_us)
    delayMicroseconds(frame_us - elapsed);
  return UARTFlushResult::UART_FLUSH_RESULT_ASSUMED_SUCCESS;
}

}  // namespace esphome::uart
#endif  // USE_ZEPHYR
