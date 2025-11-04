#pragma once
#include "esphome/core/defines.h"
#ifdef USE_UART_DEBUGGER

#include <vector>
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "uart.h"
#include "uart_component.h"

namespace esphome {
namespace uart {

/// The UARTDebugger class adds debugging support to a UART bus.
///
/// It accumulates bytes that travel over the UART bus and triggers one or
/// more actions that can log the data at an appropriate time. What
/// 'appropriate time' means exactly, is determined by a number of
/// configurable constraints. E.g. when a given number of bytes is gathered
/// and/or when no more data has been seen for a given time interval.
class UARTDebugger : public Component, public Trigger<UARTDirection, std::vector<uint8_t>, std::string> {
 public:
  explicit UARTDebugger(UARTComponent *parent);
  void loop() override;

  /// Set the direction in which to inspect the bytes: incoming, outgoing
  /// or both. When debugging in both directions, logging will be triggered
  /// when the direction of the data stream changes.
  void set_direction(UARTDirection direction) { this->for_direction_ = direction; }

  /// Set the maximum number of bytes to accumulate. When the number of bytes
  /// is reached, logging will be triggered.
  void set_after_bytes(size_t size) { this->after_bytes_ = size; }

  /// Set a timeout for the data stream. When no new bytes are seen during
  /// this timeout, logging will be triggered.
  void set_after_timeout(uint32_t timeout) { this->after_timeout_ = timeout; }

  /// Add a delimiter byte. This can be called multiple times to setup a
  /// multi-byte delimiter (a typical example would be '\r\n').
  /// When the constructed byte sequence is found in the data stream,
  /// logging will be triggered.
  void add_delimiter_byte(uint8_t byte) { this->after_delimiter_.push_back(byte); }

#ifdef UART_DEBUGGER_ADD_SETTINGS
  void reload();

  static std::string get_debug_prefix(std::string debug_prefix, bool debug_add_settings, uint32_t baud_rate,
                                      uint8_t data_bits, uint8_t stop_bits, uint8_t parity) {
    if (!debug_add_settings)
      return debug_prefix;
    std::string res = "|" + std::to_string(baud_rate);
    res += ":" + std::to_string(data_bits);
    res += ":" + std::to_string(stop_bits);
    switch (parity) {
      case UART_CONFIG_PARITY_NONE:
        res += ":NONE";
        break;
      case UART_CONFIG_PARITY_EVEN:
        res += ":EVEN";
        break;
      case UART_CONFIG_PARITY_ODD:
        res += ":ODD";
        break;
      default:
        res += ":UNKNOWN";
        break;
    }
    return res + "|" + debug_prefix;
  }
#endif

  // debug setting setters
  void set_debug_prefix(std::string debug_prefix) { this->debug_prefix_ = debug_prefix; }
  void set_debug_add_settings(bool debug_add_settings) {
    this->debug_add_settings_ = debug_add_settings;
#ifdef UART_DEBUGGER_ADD_SETTINGS
    if (debug_add_settings)
      this->final_debug_prefix_ = get_debug_prefix(this->debug_prefix_, this->debug_add_settings_, this->baud_rate_,
                                                   this->data_bits_, this->stop_bits_, this->parity_);
#endif
  }

 protected:
  UARTDirection for_direction_;
  UARTDirection last_direction_{};
  std::vector<uint8_t> bytes_{};
  size_t after_bytes_;
  uint32_t after_timeout_;
  uint32_t last_time_{};
  std::vector<uint8_t> after_delimiter_{};
  size_t after_delimiter_pos_{};
  bool is_triggering_{false};
  std::string debug_prefix_{""};
  bool debug_add_settings_{false};
#ifdef UART_DEBUGGER_ADD_SETTINGS
  std::string final_debug_prefix_{""};
  uint32_t baud_rate_;
  uint8_t stop_bits_;
  uint8_t data_bits_;
  UARTParityOptions parity_;
  UARTComponent *parent_;
#endif

  bool is_my_direction_(UARTDirection direction);
  bool is_recursive_();
  void store_byte_(UARTDirection direction, uint8_t byte);
  void trigger_after_direction_change_(UARTDirection direction);
  void trigger_after_delimiter_(uint8_t byte);
  void trigger_after_bytes_();
  void trigger_after_timeout_();
  bool has_buffered_bytes_();
  void fire_trigger_();
};

/// This UARTDevice is used by the serial debugger to read data from a
/// serial interface when the 'dummy_receiver' option is enabled.
/// The data are not stored, nor processed. This is most useful when the
/// debugger is used to reverse engineer a serial protocol, for which no
/// specific UARTDevice implementation exists (yet), but for which the
/// incoming bytes must be read to drive the debugger.
class UARTDummyReceiver : public Component, public UARTDevice {
 public:
  UARTDummyReceiver(UARTComponent *parent) : UARTDevice(parent) {}
  void loop() override;
};

/// This class contains some static methods, that can be used to easily
/// create a logging action for the debugger.
class UARTDebug {
 public:
  /// Log the bytes as hex values, separated by the provided separator
  /// character.
  static void log_hex(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator,
                      std::string debug_prefix = "");

  /// Log the bytes as string values, escaping unprintable characters.
  static void log_string(UARTDirection direction, std::vector<uint8_t> bytes, std::string debug_prefix = "");

  /// Log the bytes as integer values, separated by the provided separator
  /// character.
  static void log_int(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator,
                      std::string debug_prefix = "");

  /// Log the bytes as '<binary> (<hex>)' values, separated by the provided
  /// separator.
  static void log_binary(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator,
                         std::string debug_prefix = "");
};

}  // namespace uart
}  // namespace esphome
#endif
