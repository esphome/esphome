#ifdef USE_STM32

#include "uart_component_stm32.h"
#include <cinttypes>
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/components/stm32/gpio.h"
#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace uart {
static const char *const TAG = "uart.stm32";

void STM32UARTComponent::setup() {
  if (this->tx_pin_) {
    this->tx_pin_->setup();
  }
  if (this->rx_pin_) {
    this->rx_pin_->setup();
  }

  this->uart_handle_.Init.BaudRate = baud_rate_;
  switch (data_bits_) {
#ifdef UART_WORDLENGTH_7B
    case 7:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_7B;
      break;
#endif
#ifdef UART_WORDLENGTH_9B
    case 9:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_9B;
      break;
#endif
    default:
      this->uart_handle_.Init.WordLength = UART_WORDLENGTH_8B;
      break;
  }
  switch (stop_bits_) {
#ifdef UART_STOPBITS_0_5
    case 5:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_0_5;
      break;
#endif
#ifdef UART_STOPBITS_1_5
    case 15:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_1_5;
      break;
#endif
    case 2:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_2;
    default:
      this->uart_handle_.Init.StopBits = UART_STOPBITS_1;
      break;
  }
  switch (parity_) {
    case UARTParityOptions::UART_CONFIG_PARITY_EVEN:
      this->uart_handle_.Init.Parity = UART_PARITY_EVEN;
      break;
    case UARTParityOptions::UART_CONFIG_PARITY_ODD:
      this->uart_handle_.Init.Parity = UART_PARITY_ODD;
      break;
    default:
      this->uart_handle_.Init.Parity = UART_PARITY_NONE;
      break;
  }
  this->uart_handle_.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  this->uart_handle_.Init.Mode = UART_MODE_TX_RX;
  this->uart_handle_.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&this->uart_handle_) != HAL_OK) {
    Error_Handler();
  }
}

void STM32UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Instance: %s", this->name_.c_str());
  LOG_PIN("  TX Pin: ", this->tx_pin_);
  LOG_PIN("  RX Pin: ", this->rx_pin_);
  ESP_LOGCONFIG(TAG, "  Baud Rate: %" PRIu32 " baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
}

void STM32UARTComponent::write_array(const uint8_t *data, size_t len) {
  HAL_UART_Transmit(&this->uart_handle_, data, len, 1000);
}

bool STM32UARTComponent::peek_byte(uint8_t *data) {
  // TODO
  return false;
}

bool STM32UARTComponent::read_array(uint8_t *data, size_t len) {
  // TODO
  return false;
}

int STM32UARTComponent::available() {
  // TODO
  return 0;
}

void STM32UARTComponent::flush() {
  // TODO
}

}  // namespace uart
}  // namespace esphome

#endif  // USE_STM32
