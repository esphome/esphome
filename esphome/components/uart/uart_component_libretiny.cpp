#ifdef USE_LIBRETINY

#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "uart_component_libretiny.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#if LT_ARD_HAS_SOFTSERIAL
#include <SoftwareSerial.h>
#endif

namespace esphome {
namespace uart {

static const char *const TAG = "uart.lt";

static const char *UART_TYPE[] = {
    "hardware",
    "software",
};

uint16_t LibreTinyUARTComponent::get_config() {
  uint16_t config = 0;

  switch (this->parity_) {
    case UART_CONFIG_PARITY_NONE:
      config |= SERIAL_PARITY_NONE;
      break;
    case UART_CONFIG_PARITY_EVEN:
      config |= SERIAL_PARITY_EVEN;
      break;
    case UART_CONFIG_PARITY_ODD:
      config |= SERIAL_PARITY_ODD;
      break;
  }

  config |= (this->data_bits_ - 4) << 8;
  config |= 0x10 + (this->stop_bits_ - 1) * 0x20;

  return config;
}

void LibreTinyUARTComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up UART...");

  int8_t tx_pin = tx_pin_ == nullptr ? -1 : tx_pin_->get_pin();
  int8_t rx_pin = rx_pin_ == nullptr ? -1 : rx_pin_->get_pin();
  bool tx_inverted = tx_pin_ != nullptr && tx_pin_->is_inverted();
  bool rx_inverted = rx_pin_ != nullptr && rx_pin_->is_inverted();

  if (false)
    return;
#if LT_HW_UART0
  else if ((tx_pin == -1 || tx_pin == PIN_SERIAL0_TX) && (rx_pin == -1 || rx_pin == PIN_SERIAL0_RX)) {
    this->serial_ = &Serial0;
    this->hardware_idx_ = 0;
  }
#endif
#if LT_HW_UART1
  else if ((tx_pin == -1 || tx_pin == PIN_SERIAL1_TX) && (rx_pin == -1 || rx_pin == PIN_SERIAL1_RX)) {
    this->serial_ = &Serial1;
    this->hardware_idx_ = 1;
  }
#endif
#if LT_HW_UART2
  else if ((tx_pin == -1 || tx_pin == PIN_SERIAL2_TX) && (rx_pin == -1 || rx_pin == PIN_SERIAL2_RX)) {
    this->serial_ = &Serial2;
    this->hardware_idx_ = 2;
  }
#endif
  else {
#if LT_ARD_HAS_SOFTSERIAL
    this->serial_ = new SoftwareSerial(rx_pin, tx_pin, rx_inverted || tx_inverted);
#else
    this->serial_ = &Serial;
    ESP_LOGE(TAG, "  SoftwareSerial is not implemented for this chip. Only hardware pins are supported:");
#if LT_HW_UART0
    ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL0_TX, PIN_SERIAL0_RX);
#endif
#if LT_HW_UART1
    ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL1_TX, PIN_SERIAL1_RX);
#endif
#if LT_HW_UART2
    ESP_LOGE(TAG, "    TX=%u, RX=%u", PIN_SERIAL2_TX, PIN_SERIAL2_RX);
#endif
    this->mark_failed();
    return;
#endif
  }

  this->serial_->begin(this->baud_rate_, get_config());
}

void LibreTinyUARTComponent::dump_config() {
  bool is_software = this->hardware_idx_ == -1;
  ESP_LOGCONFIG(TAG, "UART Bus:");
  ESP_LOGCONFIG(TAG, "  Type: %s", UART_TYPE[is_software]);
  if (!is_software) {
    ESP_LOGCONFIG(TAG, "  Port number: %d", this->hardware_idx_);
  }
  LOG_PIN("  TX Pin: ", tx_pin_);
  LOG_PIN("  RX Pin: ", rx_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %u baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  this->check_logger_conflict();
}

void LibreTinyUARTComponent::write_array(const uint8_t *data, size_t len) {
  this->serial_->write(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}

bool LibreTinyUARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  *data = this->serial_->peek();
  return true;
}

bool LibreTinyUARTComponent::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  this->serial_->readBytes(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}

int LibreTinyUARTComponent::available() { return this->serial_->available(); }
void LibreTinyUARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing...");
  this->serial_->flush();
}

void LibreTinyUARTComponent::check_logger_conflict() {
#ifdef USE_LOGGER
  if (this->hardware_idx_ == -1 || logger::global_logger->get_baud_rate() == 0) {
    return;
  }

  if (this->serial_ == logger::global_logger->get_hw_serial()) {
    ESP_LOGW(TAG, "  You're using the same serial port for logging and the UART component. Please "
                  "disable logging over the serial port by setting logger->baud_rate to 0.");
  }
#endif
}

}  // namespace uart
}  // namespace esphome

#endif  // USE_LIBRETINY
