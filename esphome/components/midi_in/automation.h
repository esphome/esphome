#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "midi_in.h"

namespace esphome {
namespace midi_in {

class MidiInOnVoiceMessageTrigger : public Trigger<MidiVoiceMessage> {
 public:
  MidiInOnVoiceMessageTrigger(MidiInComponent *parent) {
    parent->add_on_voice_message_callback([this](MidiVoiceMessage msg) { this->trigger(msg); });
  }
};

class MidiInOnSystemMessageTrigger : public Trigger<MidiSystemMessage> {
 public:
  MidiInOnSystemMessageTrigger(MidiInComponent *parent) {
    parent->add_on_system_message_callback([this](MidiSystemMessage msg) { this->trigger(msg); });
  }
};

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
