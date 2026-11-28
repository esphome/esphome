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

#ifdef ESPHOME_LOGGER_TX_BUFFER_SIZE
// The logger's header "\033[0;36m[D][uart_debug:NNN]: " plus its colour reset and terminator take 33 bytes of
// tx_buffer_size (3-digit line number, checked in flush_()); a longer message is cut, so a frame continues on the
// next line instead
static constexpr size_t LOG_LINE_ROOM = ESPHOME_LOGGER_TX_BUFFER_SIZE - 33;
#else
static constexpr size_t LOG_LINE_ROOM = 479;
#endif
// Capped so a large tx_buffer_size costs no more stack
static constexpr size_t DEBUG_LINE_SIZE = std::min<size_t>(LOG_LINE_ROOM, 512);

// Escape letters for bytes 7 to 13
static constexpr uint8_t STRING_ESCAPES[] PROGMEM = {'a', 'b', 't', 'n', 'v', 'f', 'r'};

namespace {
// Builds a frame's log text; a part that does not fit one logger line is logged and the text continues on the next.
// Characters, not string literals: those would sit in RAM on ESP8266.
class DebugLine {
 public:
  // A prefix gets at most half the line, so a frame always has room
  DebugLine(UARTDirection direction, StringRef prefix)
      : prefix_(prefix),
        prefix_len_(std::min(prefix.size(), DEBUG_LINE_SIZE / 2)),
        limit_(DEBUG_LINE_SIZE - prefix_len_) {
    const char arrow = direction == UART_DIRECTION_RX ? '<' : '>';
    this->append(arrow, arrow, arrow, ' ');
  }
  template<typename... Rest> void append(char c, Rest... rest) {
    if (this->pos_ == this->limit_)
      this->flush_();
    this->buf_[this->pos_++] = c;
    if constexpr (sizeof...(rest) > 0)
      this->append(rest...);
  }
  void append_hex(uint8_t byte) {
    this->append(format_hex_pretty_char(byte >> 4), format_hex_pretty_char(byte & 0x0F));
  }
  /// Separator before every item but the first; an item of up to item_len chars that would not fit starts a new line.
  void next_item(uint8_t separator, size_t item_len) {
    if (!this->first_item_) {
      this->append(static_cast<char>(separator));
      if (this->pos_ + item_len > this->limit_)
        this->flush_();
    }
    this->first_item_ = false;
  }
  void log() {
    this->flush_();
    delay(10);
  }

 protected:
  void flush_() {
    static_assert(__LINE__ < 1000, "LOG_LINE_ROOM assumes the ESP_LOGD below has a 3-digit line number");
    ESP_LOGD(TAG, "%.*s%.*s", static_cast<int>(this->prefix_len_), this->prefix_.c_str(), static_cast<int>(this->pos_),
             this->buf_);
    this->pos_ = 0;
  }

  StringRef prefix_;
  size_t prefix_len_;
  size_t limit_;
  char buf_[DEBUG_LINE_SIZE];
  size_t pos_{0};
  bool first_item_{true};
};
}  // namespace

void UARTDebug::log_hex(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator, StringRef prefix) {
  DebugLine line(direction, prefix);
  for (uint8_t byte : bytes) {
    line.next_item(separator, 2);
    line.append_hex(byte);
  }
  line.log();
}

void UARTDebug::log_string(UARTDirection direction, std::span<const uint8_t> bytes, StringRef prefix) {
  DebugLine line(direction, prefix);
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
  line.log();
}

void UARTDebug::log_int(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator, StringRef prefix) {
  DebugLine line(direction, prefix);
  char num[UINT32_MAX_STR_SIZE];
  for (uint8_t byte : bytes) {
    const size_t len = uint32_to_str(num, byte);
    line.next_item(separator, len);
    for (size_t i = 0; i < len; i++)
      line.append(num[i]);
  }
  line.log();
}

void UARTDebug::log_binary(UARTDirection direction, std::span<const uint8_t> bytes, uint8_t separator,
                           StringRef prefix) {
  DebugLine line(direction, prefix);
  for (uint8_t byte : bytes) {
    line.next_item(separator, 17);
    line.append('0', 'b');
    for (int bit = 7; bit >= 0; bit--)
      line.append((byte >> bit) & 1 ? '1' : '0');
    line.append(' ', '(', '0', 'x');
    line.append_hex(byte);
    line.append(')');
  }
  line.log();
}

}  // namespace esphome::uart
#endif
