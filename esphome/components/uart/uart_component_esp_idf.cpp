#ifdef USE_ESP_IDF

#include "uart_component_esp_idf.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace uart {
static const char *const TAG = "uart.idf";

uart_config_t IDFUARTComponent::get_config_() {
  uart_parity_t parity = UART_PARITY_DISABLE;
  if (this->parity_ == UART_CONFIG_PARITY_EVEN) {
    parity = UART_PARITY_EVEN;
  } else if (this->parity_ == UART_CONFIG_PARITY_ODD) {
    parity = UART_PARITY_ODD;
  }

  uart_word_length_t data_bits;
  switch (this->data_bits_) {
    case 5:
      data_bits = UART_DATA_5_BITS;
      break;
    case 6:
      data_bits = UART_DATA_6_BITS;
      break;
    case 7:
      data_bits = UART_DATA_7_BITS;
      break;
    case 8:
      data_bits = UART_DATA_8_BITS;
      break;
    default:
      data_bits = UART_DATA_BITS_MAX;
      break;
  }

  uart_config_t uart_config;
  uart_config.baud_rate = this->baud_rate_;
  uart_config.data_bits = data_bits;
  uart_config.parity = parity;
  uart_config.stop_bits = this->stop_bits_ == 1 ? UART_STOP_BITS_1 : UART_STOP_BITS_2;
  uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  uart_config.source_clk = UART_SCLK_DEFAULT;
#else
  uart_config.source_clk = UART_SCLK_APB;
#endif
  uart_config.rx_flow_ctrl_thresh = 122;

  return uart_config;
}

void IDFUARTComponent::setup() {
  static uint8_t next_uart_num = 0;

#ifdef USE_LOGGER
  bool logger_uses_hardware_uart = true;

#ifdef USE_LOGGER_USB_CDC
  if (logger::global_logger->get_uart() == logger::UART_SELECTION_USB_CDC) {
    // this is not a hardware UART, ignore it
    logger_uses_hardware_uart = false;
  }
#endif  // USE_LOGGER_USB_CDC

#ifdef USE_LOGGER_USB_SERIAL_JTAG
  if (logger::global_logger->get_uart() == logger::UART_SELECTION_USB_SERIAL_JTAG) {
    // this is not a hardware UART, ignore it
    logger_uses_hardware_uart = false;
  }
#endif  // USE_LOGGER_USB_SERIAL_JTAG

  if (logger_uses_hardware_uart && logger::global_logger->get_baud_rate() > 0 &&
      logger::global_logger->get_uart_num() == next_uart_num) {
    next_uart_num++;
  }
#endif  // USE_LOGGER

  if (next_uart_num >= SOC_UART_NUM) {
    ESP_LOGW(TAG, "Maximum number of UART components created already.");
    this->mark_failed();
    return;
  }
  this->uart_num_ = static_cast<uart_port_t>(next_uart_num++);
  ESP_LOGCONFIG(TAG, "Setting up UART %u...", this->uart_num_);

  this->lock_ = xSemaphoreCreateMutex();

  xSemaphoreTake(this->lock_, portMAX_DELAY);

  uart_config_t uart_config = this->get_config_();
  esp_err_t err = uart_param_config(this->uart_num_, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  int8_t tx = this->tx_pin_ != nullptr ? this->tx_pin_->get_pin() : -1;
  int8_t rx = this->rx_pin_ != nullptr ? this->rx_pin_->get_pin() : -1;

  uint32_t invert = 0;
  if (this->tx_pin_ != nullptr && this->tx_pin_->is_inverted())
    invert |= UART_SIGNAL_TXD_INV;
  if (this->rx_pin_ != nullptr && this->rx_pin_->is_inverted())
    invert |= UART_SIGNAL_RXD_INV;

  err = uart_set_line_inverse(this->uart_num_, invert);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_line_inverse failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = uart_set_pin(this->uart_num_, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_set_pin failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  err = uart_driver_install(this->uart_num_, /* UART RX ring buffer size. */ this->rx_buffer_size_,
                            /* UART TX ring buffer size. If set to zero, driver will not use TX buffer, TX function will
                               block task until all data have been sent out.*/
                            0,
                            /* UART event queue size/depth. */ 20, &(this->uart_event_queue_),
                            /* Flags used to allocate the interrupt. */ 0);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_driver_install failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  xSemaphoreGive(this->lock_);
}

void IDFUARTComponent::load_settings(bool dump_config) {
  uart_config_t uart_config = this->get_config_();
  esp_err_t err = uart_param_config(this->uart_num_, &uart_config);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "uart_param_config failed: %s", esp_err_to_name(err));
    this->mark_failed();
    return;
  } else if (dump_config) {
    ESP_LOGCONFIG(TAG, "UART %u was reloaded.", this->uart_num_);
    this->dump_config();
  }
}

void IDFUARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus %u:", this->uart_num_);
  LOG_PIN("  TX Pin: ", tx_pin_);
  LOG_PIN("  RX Pin: ", rx_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %" PRIu32 " baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  this->check_logger_conflict();
}

void IDFUARTComponent::write_array(const uint8_t *data, size_t len) {
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  uart_write_bytes(this->uart_num_, data, len);
  xSemaphoreGive(this->lock_);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool IDFUARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  if (this->has_peek_) {
    *data = this->peek_byte_;
  } else {
    int len = uart_read_bytes(this->uart_num_, data, 1, 20 / portTICK_PERIOD_MS);
    if (len == 0) {
      *data = 0;
    } else {
      this->has_peek_ = true;
      this->peek_byte_ = *data;
    }
  }
  xSemaphoreGive(this->lock_);
  return true;
}

bool IDFUARTComponent::read_array(uint8_t *data, size_t len) {
  size_t length_to_read = len;
  if (!this->check_read_timeout_(len))
    return false;
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  if (this->has_peek_) {
    length_to_read--;
    *data = this->peek_byte_;
    data++;
    this->has_peek_ = false;
  }
  if (length_to_read > 0)
    uart_read_bytes(this->uart_num_, data, length_to_read, 20 / portTICK_PERIOD_MS);
  xSemaphoreGive(this->lock_);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

int IDFUARTComponent::available() {
  size_t available;

  xSemaphoreTake(this->lock_, portMAX_DELAY);
  uart_get_buffered_data_len(this->uart_num_, &available);
  if (this->has_peek_)
    available++;
  xSemaphoreGive(this->lock_);

  return available;
}

void IDFUARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing...");
  xSemaphoreTake(this->lock_, portMAX_DELAY);
  uart_wait_tx_done(this->uart_num_, portMAX_DELAY);
  xSemaphoreGive(this->lock_);
}

void IDFUARTComponent::check_logger_conflict() {}

}  // namespace uart
}  // namespace esphome

#endif  // USE_ESP32
