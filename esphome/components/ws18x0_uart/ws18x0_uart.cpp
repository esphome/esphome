#include "ws18x0_uart.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ws18x0_uart {

static const char *const TAG = "ws18x0_uart";

static const uint8_t WS18X0_UART_START_BYTE = 0x02;
static const uint8_t WS18X0_UART_END_BYTE = 0x03;

static const int8_t WS18X0_UART_STATE_WAITING_FOR_START = -1;
static const int8_t WS18X0_UART_STATE_WAITING_FOR_LENGTH = -2;
static const int8_t WS18X0_UART_STATE_WAITING_FOR_CHECKSUM = 2;
static const int8_t WS18X0_UART_STATE_WAITING_FOR_END = 1;

void ws18x0_uart::WS18x0UARTComponent::loop() {
  while (this->available() > 0) {
    uint8_t data;
    if (!this->read_byte(&data)) {
      ESP_LOGW(TAG, "Reading data from WS18x0UART failed!");
      this->status_set_warning();
      return;
    }

    switch (this->read_state_) {
      case WS18X0_UART_STATE_WAITING_FOR_START:
        if (data == WS18X0_UART_START_BYTE) {
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_LENGTH;
        } else {
          // Not start byte, probably not synced up correctly, skip forward.
        }
        break;
      case WS18X0_UART_STATE_WAITING_FOR_LENGTH:
        if (data >= 9 && data <= 10) {
          this->raw_ = data;
          this->read_state_ = data - 2;
        } else {
          // Invalid length. Warn and resync
          ESP_LOGW(TAG, "Reading data from WS18x0UART failed with length out of expected range: %d", data);
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_START;
          this->status_set_warning();
        }
        break;
      case WS18X0_UART_STATE_WAITING_FOR_CHECKSUM: {
        uint8_t checksum = 0;
        for (int i = 0; i < 8; ++i)
          checksum ^= (this->raw_ >> i * 8) & 0xFF;

        if (checksum == data) {
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_END;
        } else {
          // Invalid checksum. Warn and resync
          ESP_LOGW(TAG,
                   "Reading data from WS18x0UART failed with invalid checksum read 0x%2X != calculated 0x%2X from raw "
                   "data: 0x%llX",
                   data, checksum, this->raw_);
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_START;
          this->status_set_warning();
        }
        break;
      }
      case WS18X0_UART_STATE_WAITING_FOR_END:
        if (data != WS18X0_UART_END_BYTE) {
          // Invalid end byte. Warn and resync
          ESP_LOGW(TAG, "Reading data from WS18x0UART failed with invalid end byte: 0x%2X", data);
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_START;
          this->status_set_warning();
        } else {
          // Valid data
          this->status_clear_warning();
          bool report = this->raw_ != this->last_raw_;
          uint32_t result = (uint32_t) this->raw_;

          for (auto *card : this->cards_) {
            if (card->process(result)) {
              report = false;
            }
          }
          for (auto *trig : this->triggers_)
            trig->process(result);

          if (report) {
            ESP_LOGD(TAG, "Found new tag with ID %" PRIu32, result);
          }
          this->last_raw_ = this->raw_;
          this->read_state_ = WS18X0_UART_STATE_WAITING_FOR_START;
        }
        break;
      default:
        // Read next byte
        this->raw_ = this->raw_ << 8 | data;
        this->read_state_--;
        break;
    }
  }
}

}  // namespace ws18x0_uart
}  // namespace esphome
