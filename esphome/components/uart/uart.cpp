#include "uart.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace uart {

static const char *TAG = "uart";

size_t UARTComponent::write(uint8_t data) {
  this->write_byte(data);
  return 1;
}
int UARTComponent::read() {
  uint8_t data;
  if (!this->read_byte(&data))
    return -1;
  return data;
}
int UARTComponent::peek() {
  uint8_t data;
  if (!this->peek_byte(&data))
    return -1;
  return data;
}

void UARTComponent::check_logger_conflict_() {
#ifdef USE_LOGGER
  if (this->hw_serial_ == nullptr || logger::global_logger->get_baud_rate() == 0) {
    return;
  }

  if (this->hw_serial_ == logger::global_logger->get_hw_serial()) {
    ESP_LOGW(TAG, "  You're using the same serial port for logging and the UART component. Please "
                  "disable logging over the serial port by setting logger->baud_rate to 0.");
  }
#endif
}

void UARTDevice::check_uart_settings(uint32_t baud_rate, uint8_t stop_bits, UARTParityOptions parity,
                                     uint8_t data_bits) {
  if (this->parent_->baud_rate_ != baud_rate) {
    ESP_LOGE(TAG, "  Invalid baud_rate: Integration requested baud_rate %u but you have %u!", baud_rate,
             this->parent_->baud_rate_);
  }
  if (this->parent_->stop_bits_ != stop_bits) {
    ESP_LOGE(TAG, "  Invalid stop bits: Integration requested stop_bits %u but you have %u!", stop_bits,
             this->parent_->stop_bits_);
  }
  if (this->parent_->data_bits_ != data_bits) {
    ESP_LOGE(TAG, "  Invalid number of data bits: Integration requested %u data bits but you have %u!", data_bits,
             this->parent_->data_bits_);
  }
  if (this->parent_->parity_ != parity) {
    ESP_LOGE(TAG, "  Invalid parity: Integration requested parity %s but you have %s!", parity_to_str(parity),
             parity_to_str(this->parent_->parity_));
  }
}

const char *parity_to_str(UARTParityOptions parity) {
  switch (parity) {
    case UART_CONFIG_PARITY_NONE:
      return "NONE";
    case UART_CONFIG_PARITY_EVEN:
      return "EVEN";
    case UART_CONFIG_PARITY_ODD:
      return "ODD";
    default:
      return "UNKNOWN";
  }
}

}  // namespace uart
}  // namespace esphome
