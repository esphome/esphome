#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <MIDI.h>
// #include <midi_Defs.h>

namespace esphome { 
namespace midi_in {

const uint8_t MASK_COMMAND = 0xF0;
const uint8_t MASK_CHANNEL = 0x0F;

class UARTSerialPort {
  public:
    UARTSerialPort(uart::UARTDevice *uart) {
      uart_ = uart;
    }

    void begin(const long baudRate) {
      ESP_LOGD("UARTSerialPort", "begin(%i)", baudRate);
    }
    void end() {
      ESP_LOGD("UARTSerialPort", "end");
    }
    bool beginTransmission(midi::MidiType)
    {
      return true;
    };

    void write(byte value)
    {
      uart_->write(value);
    };

    void endTransmission()
    {
    };

    byte read()
    {
      byte result = uart_->read();
      ESP_LOGVV("UARTSerialPort", "read: %#04x", result);
      return result;
    };

    unsigned available()
    {
      if (uart_->available()) {
        ESP_LOGV("UARTSerialPort", "available");
        return true;
      } else {
        ESP_LOGV("UARTSerialPort", "not available");
        return false;
      }
    };

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
