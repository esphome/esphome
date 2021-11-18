#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include <MIDI.h>

namespace esphome {
namespace midi_in {

const uint8_t MASK_COMMAND = 0xF0;
const uint8_t MASK_CHANNEL = 0x0F;

class UARTSerialPort {
 public:
  UARTSerialPort(uart::UARTDevice *uart) { uart_ = uart; }

  void begin(const int32_t baud_rate) {}
  
  void end() {}

  void write(uint8_t value) { uart_->write(value); };

  uint8_t read() { return uart_->read(); };

  unsigned available() { return uart_->available(); };

 private:
  uart::UARTDevice *uart_;
};

struct MidiVoiceMessage {
  midi::MidiType command;
  uint8_t channel;
  uint8_t param1;
  uint8_t param2;
};

struct MidiSystemMessage {
  midi::MidiType command;
};
}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
