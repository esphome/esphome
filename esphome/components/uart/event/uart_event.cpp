#include "uart_event.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <algorithm>

namespace esphome {
namespace uart {

static const char *const TAG = "uart.event";

void UARTEvent::setup() { buffer_.clear(); }

void UARTEvent::dump_config() { LOG_EVENT("", "UART Event", this); }

void UARTEvent::loop() { read_data_(); }

void UARTEvent::add_event_matcher(const std::string &event_name, const std::string &match_string) {
  std::vector<uint8_t> data(match_string.begin(), match_string.end());
  this->matchers_.push_back({event_name, data});
  if (data.size() > this->max_matcher_len_) {
    this->max_matcher_len_ = data.size();
  }
}

void UARTEvent::add_event_matcher(const std::string &event_name, const std::vector<uint8_t> &match_binary) {
  this->matchers_.push_back({event_name, match_binary});
  if (match_binary.size() > this->max_matcher_len_) {
    this->max_matcher_len_ = match_binary.size();
  }
}

void UARTEvent::read_data_() {
  while (available()) {
    uint8_t data;
    read_byte(&data);
    this->buffer_.push_back(data);

    bool match_found = false;
    for (const auto &matcher : this->matchers_) {
      const auto &match_data = matcher.binary_data;

      if (this->buffer_.size() < match_data.size()) {
        continue;
      }

      if (std::equal(match_data.begin(), match_data.end(), this->buffer_.end() - match_data.size())) {
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

}  // namespace uart
}  // namespace esphome
