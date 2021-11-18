#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <MIDI.h>

namespace esphome {
namespace midi_in {

class UARTSerialPort {
 public:
  UARTSerialPort(uart::UARTDevice *uart) : uart_(uart) {}

  void begin(const int32_t baud_rate) {}

  void end() {}

  void write(uint8_t value) { uart_->write(value); };

  uint8_t read() { return uart_->read(); };

  unsigned available() { return uart_->available(); };

 private:
  uart::UARTDevice *uart_;
};

struct MidiChannelMessage {
  midi::MidiType type;
  uint8_t channel;
  uint8_t data1;
  uint8_t data2;
};

struct MidiSystemMessage {
  midi::MidiType type;
};

/// Convert the given MidiType to a human-readable string.
const LogString *midi_type_to_string(midi::MidiType type);

/// Convert the given MidiControlChangeNumber to a human-readable string.
const LogString *midi_controller_to_string(midi::MidiControlChangeNumber controller);

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
