#include "uart_event.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::uart {

static const char *const TAG = "uart.event";

void UARTEvent::setup() {}

void UARTEvent::dump_config() { LOG_EVENT("", "UART Event", this); }

void UARTEvent::loop() { this->read_data_(); }

void UARTEvent::read_data_() {
  while (this->available()) {
    uint8_t data;
    this->read_byte(&data);
    this->buffer_.push_back(data);

    bool match_found = false;
    for (uint16_t i = 0; i < this->matcher_count_; i++) {
      const UARTEventMatcher &matcher = this->matchers_[i];
      if (this->buffer_.size() < matcher.data_len) {
        continue;
      }

      const uint8_t *tail = this->buffer_.data() + this->buffer_.size() - matcher.data_len;
      size_t pos = 0;
      // The pattern is in flash; ESP8266 only allows word loads there, so read it with progmem_read_byte
      while (pos < matcher.data_len && progmem_read_byte(matcher.data + pos) == tail[pos])
        pos++;
      if (pos == matcher.data_len) {
        this->trigger(matcher.event_name);
        this->buffer_.clear();
        match_found = true;
        break;
      }
    }

    if (!match_found && this->max_matcher_len_ > 0 && this->buffer_.size() > this->max_matcher_len_) {
      this->buffer_.erase(this->buffer_.begin());
    }
  }
}

}  // namespace esphome::uart
