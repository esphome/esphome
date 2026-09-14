#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"

#include "ble_midi.h"

#include <cstdint>
#include <vector>

namespace esphome::ble_midi {

/// Triggered once the MIDI characteristic is found and notifications are
/// enabled, which is also when the device can be sent to.
class ConnectTrigger final : public Trigger<> {
 public:
  explicit ConnectTrigger(BLEMidi *parent) { parent->add_on_connect_trigger(this); }
};

/// Triggered when an established MIDI connection is lost.
class DisconnectTrigger final : public Trigger<> {
 public:
  explicit DisconnectTrigger(BLEMidi *parent) { parent->add_on_disconnect_trigger(this); }
};

/// Triggered with (note, velocity, channel).
class NoteOnTrigger final : public Trigger<uint8_t, uint8_t, uint8_t> {
 public:
  explicit NoteOnTrigger(BLEMidi *parent) { parent->add_on_note_on_trigger(this); }
};

/// Triggered with (note, velocity, channel). Note On with velocity zero is
/// reported here as well.
class NoteOffTrigger final : public Trigger<uint8_t, uint8_t, uint8_t> {
 public:
  explicit NoteOffTrigger(BLEMidi *parent) { parent->add_on_note_off_trigger(this); }
};

/// Triggered with (control_number, value, channel).
class ControlChangeTrigger final : public Trigger<uint8_t, uint8_t, uint8_t> {
 public:
  explicit ControlChangeTrigger(BLEMidi *parent) { parent->add_on_control_change_trigger(this); }
};

/// Triggered with (program, channel).
class ProgramChangeTrigger final : public Trigger<uint8_t, uint8_t> {
 public:
  explicit ProgramChangeTrigger(BLEMidi *parent) { parent->add_on_program_change_trigger(this); }
};

/// Triggered with (value, channel), value being -8192 to 8191.
class PitchBendTrigger final : public Trigger<int16_t, uint8_t> {
 public:
  explicit PitchBendTrigger(BLEMidi *parent) { parent->add_on_pitch_bend_trigger(this); }
};

/// Triggered with the SysEx payload, without the 0xF0/0xF7 wrappers.
class SysexTrigger final : public Trigger<std::vector<uint8_t>> {
 public:
  explicit SysexTrigger(BLEMidi *parent) { parent->add_on_sysex_trigger(this); }
};

/// Triggered with the raw bytes of every decoded message.
class MessageTrigger final : public Trigger<std::vector<uint8_t>> {
 public:
  explicit MessageTrigger(BLEMidi *parent) { parent->add_on_message_trigger(this); }
};

template<typename... Ts> class NoteOnAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(uint8_t, note)
  TEMPLATABLE_VALUE(uint8_t, velocity)
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override {
    this->parent_->send_note_on(this->note_.value(x...), this->velocity_.value(x...), this->channel_.value(x...));
  }
};

template<typename... Ts> class NoteOffAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(uint8_t, note)
  TEMPLATABLE_VALUE(uint8_t, velocity)
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override {
    this->parent_->send_note_off(this->note_.value(x...), this->velocity_.value(x...), this->channel_.value(x...));
  }
};

template<typename... Ts> class ControlChangeAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(uint8_t, control_number)
  TEMPLATABLE_VALUE(uint8_t, value)
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override {
    this->parent_->send_control_change(this->control_number_.value(x...), this->value_.value(x...),
                                       this->channel_.value(x...));
  }
};

template<typename... Ts> class ProgramChangeAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(uint8_t, program)
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override {
    this->parent_->send_program_change(this->program_.value(x...), this->channel_.value(x...));
  }
};

template<typename... Ts> class PitchBendAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(int16_t, value)
  TEMPLATABLE_VALUE(uint8_t, channel)

  void play(Ts... x) override { this->parent_->send_pitch_bend(this->value_.value(x...), this->channel_.value(x...)); }
};

template<typename... Ts> class SysexAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, payload)

  void play(Ts... x) override { this->parent_->send_sysex(this->payload_.value(x...)); }
};

template<typename... Ts> class RawAction final : public Action<Ts...>, public Parented<BLEMidi> {
 public:
  TEMPLATABLE_VALUE(std::vector<uint8_t>, data)

  void play(Ts... x) override { this->parent_->send_raw(this->data_.value(x...)); }
};

template<typename... Ts> class BLEMidiConnectedCondition final : public Condition<Ts...>, public Parented<BLEMidi> {
 public:
  bool check(Ts... x) override { return this->parent_->is_ready(); }
};

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
