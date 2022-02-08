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
class UARTDebugger : public Component, public Trigger<UARTDirection, std::vector<uint8_t>> {
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
  /// When the constructued byte sequence is found in the data stream,
  /// logging will be triggered.
  void add_delimiter_byte(uint8_t byte) { this->after_delimiter_.push_back(byte); }

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
  static void log_hex(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator);

  /// Log the bytes as string values, escaping unprintable characters.
  static void log_string(UARTDirection direction, std::vector<uint8_t> bytes);

  /// Log the bytes as integer values, separated by the provided separator
  /// character.
  static void log_int(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator);

  /// Log the bytes as '<binary> (<hex>)' values, separated by the provided
  /// separator.
  static void log_binary(UARTDirection direction, std::vector<uint8_t> bytes, uint8_t separator);
};

}  // namespace uart
}  // namespace esphome
#endif
