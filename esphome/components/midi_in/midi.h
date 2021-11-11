#pragma once

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
namespace esphome {
namespace midi_in {

const uint8_t MASK_COMMAND = 0xF0;
const uint8_t MASK_CHANNEL = 0x0F;

// can use definitions from:
// https://github.com/FortySevenEffects/arduino_midi_library/blob/d9149d19df867abbf78cfcd37ef9d0e14dedca7f/src/midi_Defs.h
enum MidiCommand : uint8_t {
  NOTE_OFF = 0x80,
  NOTE_ON = 0x90,
  AFTERTOUCH = 0xA0,
  CONTROLLER = 0xB0,
  PATCH_CHANGE = 0xC0,
  CHANNEL_PRESSURE = 0xD0,
  PITCH_BEND = 0xE0,
  SYSTEM = 0xF0,
  SYSTEM_EXCLUSIVE = 0xF0,
  SYSTEM_EXCLUSIVE_END = 0xF7,
  SYSTEM_TIMING_CLOCK = 0xF8,
  SYSTEM_START = 0xFA,
  SYSTEM_CONTINUE = 0xFB,
  SYSTEM_STOP = 0xFC,
  SYSTEM_ACTIVE_SENSING = 0xFE,
  SYSTEM_RESET = 0xFF,
};

enum MidiController : uint8_t {
  // Value: 0-127 (GM2)
  BANK_SELECT = 0x00,
  // Channel volume (coarse) (formerly main volume). Value:  0-127
  CHANNEL_VOLUME = 0x07,
  // Value: 0-127 (GM2)
  BANK_SELECT_FINE = 0x20,
  // Value: (on/off) = 64 is on
  SUSTAIN = 0x40,
  // Value: (on/off) = 64 is on
  SUSTENUTO = 0x42,
  // Value: (on/off) = 64 is on
  SOFT_PEDAL = 0x43,
  ALL_SOUND_OFF = 0x78,
  ALL_NOTES_OFF = 0x7B,
  // Poly operation and all notes off
  POLY_MODE_ON = 0x7F
};

struct MidiVoiceMessage {
  MidiCommand command;
  uint8_t channel;
  uint8_t param1;
  uint8_t param2;
};

struct MidiSystemMessage {
  MidiCommand command;
};
}  // namespace midi_in
}  // namespace esphome
