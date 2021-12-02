#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "midi_in.h"

namespace esphome {
namespace midi_in {

class MidiInOnChannelMessageTrigger : public Trigger<MidiChannelMessage> {
 public:
  MidiInOnChannelMessageTrigger(MidiInComponent *parent) {
    parent->add_on_voice_message_callback([this](MidiChannelMessage msg) { this->trigger(msg); });
  }
};

class MidiInOnSystemMessageTrigger : public Trigger<MidiSystemMessage> {
 public:
  MidiInOnSystemMessageTrigger(MidiInComponent *parent) {
    parent->add_on_system_message_callback([this](MidiSystemMessage msg) { this->trigger(msg); });
  }
};

template <typename... Ts>
class MidiInNoteOnCondition : public Condition<Ts...> {
public:
  explicit MidiInNoteOnCondition(MidiInComponent *parent) : parent_(parent) {}

  void set_note(uint8_t note) { this->note_ = note; }
  bool check(Ts... x) override
  {
    return this->parent_->note_velocity(this->note_) > 0;
  }

protected:
  MidiInComponent *parent_;
  uint8_t note_;
};
}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
