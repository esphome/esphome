#include "esphome/core/defines.h"
#ifdef USE_UART_DEBUGGER

#include <algorithm>
#include "uart_debugger.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::uart {

ESPHOME_LOG_TAG(TAG, "uart_debug");

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
  if (this->after_delimiter_len_ == 0 || !this->has_buffered_bytes_()) {
    return;
  }
  if (progmem_read_byte(this->after_delimiter_ + this->after_delimiter_pos_) != byte) {
    this->after_delimiter_pos_ = 0;
    return;
  }
  this->after_delimiter_pos_++;
  if (this->after_delimiter_pos_ == this->after_delimiter_len_) {
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
  this->fire(this->last_direction_, this->bytes_, this->debug_prefix_);
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

// The logger cuts a longer line anyway; capped so a large tx_buffer_size costs no more stack
#ifdef ESPHOME_LOGGER_TX_BUFFER_SIZE
static constexpr size_t DEBUG_LINE_SIZE = std::min<size_t>(ESPHOME_LOGGER_TX_BUFFER_SIZE, 512);
#else
static constexpr size_t DEBUG_LINE_SIZE = 512;
#endif

// Escape letters for bytes 7 to 13
static constexpr uint8_t STRING_ESCAPES[] PROGMEM = {'a', 'b', 't', 'n', 'v', 'f', 'r'};

namespace {
// Appends into a fixed line, dropping what does not fit.
// Characters, not string literals: those would sit in RAM on ESP8266.
class DebugLine {
 public:
  explicit DebugLine(UARTDirection direction) {
    const char arrow = direction == UART_DIRECTION_RX ? '<' : '>';
    this->append(arrow, arrow, arrow, ' ');
  }
  template<typename... Rest> void append(char c, Rest... rest) {
    if (this->pos_ < sizeof(this->buf_))
      this->buf_[this->pos_++] = c;
    if constexpr (sizeof...(rest) > 0)
      this->append(rest...);
  }
  void append_hex(uint8_t byte) {
    this->append(format_hex_pretty_char(byte >> 4), format_hex_pretty_char(byte & 0x0F));
  }
  /// Separator before every item but the first.
  void next_item(uint8_t separator) {
    if (this->pos_ > HEADER_LEN)
      this->append(static_cast<char>(separator));
  }
  void log(StringRef prefix) const {
    ESP_LOGD(TAG, "%s%.*s", prefix.c_str(), static_cast<int>(this->pos_), this->buf_);
    delay(10);
  }

 protected:
  static constexpr size_t HEADER_LEN = 4;  // "<<< " or ">>> "
  char buf_[DEBUG_LINE_SIZE];
  size_t pos_{0};
};
}  // namespace

void UARTDebug::log_hex(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator, StringRef prefix) {
  DebugLine line(direction);
  for (uint8_t byte : bytes) {
    line.next_item(separator);
    line.append_hex(byte);
  }
  line.log(prefix);
}

void UARTDebug::log_string(UARTDirection direction, std::span<const uint8_t> bytes, StringRef prefix) {
  DebugLine line(direction);
  line.append('"');
  for (uint8_t byte : bytes) {
    if (byte >= 7 && byte <= 13) {
      line.append('\\', static_cast<char>(progmem_read_byte(&STRING_ESCAPES[byte - 7])));
    } else if (byte == 27) {
      line.append('\\', 'e');
    } else if (byte == '"' || byte == '\'' || byte == '\\') {
      line.append('\\', static_cast<char>(byte));
    } else if (byte < 32 || byte > 127) {
      line.append('\\', 'x');
      line.append_hex(byte);
    } else {
      line.append(static_cast<char>(byte));
    }
  }
  line.append('"');
  line.log(prefix);
}

void UARTDebug::log_int(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator, StringRef prefix) {
  DebugLine line(direction);
  char num[UINT32_MAX_STR_SIZE];
  for (uint8_t byte : bytes) {
    line.next_item(separator);
    const size_t len = uint32_to_str(num, byte);
    for (size_t i = 0; i < len; i++)
      line.append(num[i]);
  }
  line.log(prefix);
}

void UARTDebug::log_binary(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator,
                           StringRef prefix) {
  DebugLine line(direction);
  for (uint8_t byte : bytes) {
    line.next_item(separator);
    line.append('0', 'b');
    for (int bit = 7; bit >= 0; bit--)
      line.append((byte >> bit) & 1 ? '1' : '0');
    line.append(' ', '(', '0', 'x');
    line.append_hex(byte);
    line.append(')');
  }
  line.log(prefix);
}

}  // namespace esphome::uart
#endif
