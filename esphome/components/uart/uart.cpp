#include "uart.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"
#include <cinttypes>

namespace esphome::uart {

static const char *const TAG = "uart";

// UART parity strings indexed by UARTParityOptions enum (0-2): NONE, EVEN, ODD
PROGMEM_STRING_TABLE(UARTParityStrings, "NONE", "EVEN", "ODD", "UNKNOWN");

void UARTDevice::check_uart_settings(uint32_t baud_rate, uint8_t stop_bits, UARTParityOptions parity,
                                     uint8_t data_bits) {
  if (this->parent_->get_baud_rate() != baud_rate) {
    ESP_LOGE(TAG, "  Invalid baud_rate: Integration requested baud_rate %" PRIu32 " but you have %" PRIu32 "!",
             baud_rate, this->parent_->get_baud_rate());
  }
  if (this->parent_->get_stop_bits() != stop_bits) {
    ESP_LOGE(TAG, "  Invalid stop bits: Integration requested stop_bits %u but you have %u!", stop_bits,
             this->parent_->get_stop_bits());
  }
  if (this->parent_->get_data_bits() != data_bits) {
    ESP_LOGE(TAG, "  Invalid number of data bits: Integration requested %u data bits but you have %u!", data_bits,
             this->parent_->get_data_bits());
  }
  if (this->parent_->get_parity() != parity) {
    ESP_LOGE(TAG, "  Invalid parity: Integration requested parity %s but you have %s!",
             LOG_STR_ARG(parity_to_str(parity)), LOG_STR_ARG(parity_to_str(this->parent_->get_parity())));
  }
}

const LogString *parity_to_str(UARTParityOptions parity) {
  return UARTParityStrings::get_log_str(static_cast<uint8_t>(parity), UARTParityStrings::LAST_INDEX);
}

}  // namespace esphome::uart
