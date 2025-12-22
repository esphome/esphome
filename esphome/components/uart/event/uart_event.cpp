#include "uart_event.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome::uart {

static const char *const TAG = "uart.event";

void UARTEvent::setup() {}

void UARTEvent::dump_config() { LOG_EVENT("", "UART Event", this); }

void UARTEvent::loop() { this->read_data_(); }

void UARTEvent::add_event_matcher(const char *event_name, const uint8_t *match_data, size_t match_data_len) {
  this->matchers_.push_back({event_name, match_data, match_data_len});
  if (match_data_len > this->max_matcher_len_) {
    this->max_matcher_len_ = match_data_len;
  }
}

void UARTEvent::read_data_() {
  while (this->available()) {
    uint8_t data;
    this->read_byte(&data);
    this->buffer_.push_back(data);

    bool match_found = false;
    for (const auto &matcher : this->matchers_) {
      if (this->buffer_.size() < matcher.data_len) {
        continue;
      }

      if (std::equal(matcher.data, matcher.data + matcher.data_len, this->buffer_.end() - matcher.data_len)) {
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
