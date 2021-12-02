#include "esphome/core/defines.h"
#ifdef USE_UART_DEBUGGER

#include <vector>
#include "uart_debugger.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace uart {

static const char *const TAG = "uart_debug";

UARTDebugger::UARTDebugger(UARTComponent *parent) {
  parent->add_debug_callback([this](UARTDirection direction, uint8_t byte) {
    if (!this->is_my_direction_(direction) || this->is_recursive_()) {
      return;
    }
    this->trigger_after_direction_change_(direction);
    this->store_byte_(direction, byte);
    this->trigger_after_delimiter_(byte);
    this->trigger_after_bytes_();
  });
}

void UARTDebugger::loop() { this->trigger_after_timeout_(); }

bool UARTDebugger::is_my_direction_(UARTDirection direction) {
  return this->for_direction_ == UART_DIRECTION_BOTH || this->for_direction_ == direction;
}

bool UARTDebugger::is_recursive_() { return this->is_triggering_; }

void UARTDebugger::trigger_after_direction_change_(UARTDirection direction) {
  if (this->has_buffered_bytes_() && this->for_direction_ == UART_DIRECTION_BOTH &&
      this->last_direction_ != direction) {
    this->fire_trigger_();
  }
}

void UARTDebugger::store_byte_(UARTDirection direction, uint8_t byte) {
  this->bytes_.push_back(byte);
  this->last_direction_ = direction;
  this->last_time_ = millis();
}

void UARTDebugger::trigger_after_delimiter_(uint8_t byte) {
  if (this->after_delimiter_.empty() || !this->has_buffered_bytes_()) {
    return;
  }
  if (this->after_delimiter_[this->after_delimiter_pos_] != byte) {
    this->after_delimiter_pos_ = 0;
    return;
  }
  this->after_delimiter_pos_++;
  if (this->after_delimiter_pos_ == this->after_delimiter_.size()) {
    this->fire_trigger_();
    this->after_delimiter_pos_ = 0;
  }
}

void UARTDebugger::trigger_after_bytes_() {
  if (this->has_buffered_bytes_() && this->after_bytes_ > 0 && this->bytes_.size() >= this->after_bytes_) {
    this->fire_trigger_();
  }
}

void UARTDebugger::trigger_after_timeout_() {
  if (this->has_buffered_bytes_() && this->after_timeout_ > 0 && millis() - this->last_time_ >= this->after_timeout_) {
    this->fire_trigger_();
  }
}

bool UARTDebugger::has_buffered_bytes_() { return !this->bytes_.empty(); }

void UARTDebugger::fire_trigger_() {
  this->is_triggering_ = true;
  trigger(this->last_direction_, this->bytes_);
  this->bytes_.clear();
  this->is_triggering_ = false;
}

void UARTDummyReceiver::loop() {
  // Reading up to a limited number of bytes, to make sure that this loop()
  // won't lock up the system on a continuous incoming stream of bytes.
  uint8_t data;
  int count = 50;
  while (this->available() && count--) {
    this->read_byte(&data);
  }
}

// In the upcoming log functions, a delay was added after all log calls.
// This is done to allow the system to ship the log lines via the API
// TCP connection(s). Without these delays, debug log lines could go
// missing when UART devices block the main loop for too long.

void UARTDebug::log_hex(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
  std::string res;
  if (direction == UART_DIRECTION_RX) {
    res += "<<< ";
  } else {
    res += ">>> ";
  }
  size_t len = bytes.size();
  char buf[5];
  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      res += separator;
    }
    sprintf(buf, "%02X", bytes[i]);
    res += buf;
  }
  ESP_LOGD(TAG, "%s", res.c_str());
  delay(10);
}

void UARTDebug::log_string(UARTDirection direction, std::vector<uint8_t> bytes) {
  std::string res;
  if (direction == UART_DIRECTION_RX) {
    res += "<<< \"";
  } else {
    res += ">>> \"";
  }
  size_t len = bytes.size();
  char buf[5];
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] == 7) {
      res += "\\a";
    } else if (bytes[i] == 8) {
      res += "\\b";
    } else if (bytes[i] == 9) {
      res += "\\t";
    } else if (bytes[i] == 10) {
      res += "\\n";
    } else if (bytes[i] == 11) {
      res += "\\v";
    } else if (bytes[i] == 12) {
      res += "\\f";
    } else if (bytes[i] == 13) {
      res += "\\r";
    } else if (bytes[i] == 27) {
      res += "\\e";
    } else if (bytes[i] == 34) {
      res += "\\\"";
    } else if (bytes[i] == 39) {
      res += "\\'";
    } else if (bytes[i] == 92) {
      res += "\\\\";
    } else if (bytes[i] < 32 || bytes[i] > 127) {
      sprintf(buf, "\\x%02X", bytes[i]);
      res += buf;
    } else {
      res += bytes[i];
    }
  }
  res += '"';
  ESP_LOGD(TAG, "%s", res.c_str());
  delay(10);
}

void UARTDebug::log_int(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
  std::string res;
  size_t len = bytes.size();
  if (direction == UART_DIRECTION_RX) {
    res += "<<< ";
  } else {
    res += ">>> ";
  }
  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      res += separator;
    }
    res += to_string(bytes[i]);
  }
  ESP_LOGD(TAG, "%s", res.c_str());
  delay(10);
}

void UARTDebug::log_binary(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator) {
  std::string res;
  size_t len = bytes.size();
  if (direction == UART_DIRECTION_RX) {
    res += "<<< ";
  } else {
    res += ">>> ";
  }
  char buf[20];
  for (size_t i = 0; i < len; i++) {
    if (i > 0) {
      res += separator;
    }
    sprintf(buf, "0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(bytes[i]), bytes[i]);
    res += buf;
  }
  ESP_LOGD(TAG, "%s", res.c_str());
  delay(10);
}

}  // namespace uart
}  // namespace esphome
#endif
