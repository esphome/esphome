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

template<typename... Ts> class MidiInNoteOnCondition : public Condition<Ts...> {
 public:
  explicit MidiInNoteOnCondition(MidiInComponent *parent) : parent_(parent) {}

  void set_note(uint8_t note) { this->note_ = note; }
  bool check(Ts... x) override { return this->parent_->note_velocity(this->note_) > 0; }

 protected:
  MidiInComponent *parent_;
  uint8_t note_;
};

template<typename... Ts> class MidiInControlInRangeCondition : public Condition<Ts...> {
 public:
  explicit MidiInControlInRangeCondition(MidiInComponent *parent) : parent_(parent) {}

  void set_control(uint8_t control) { this->control_ = static_cast<midi::MidiControlChangeNumber>(control); }
  void set_min(uint8_t min) { this->min_ = min; }
  void set_max(uint8_t max) { this->max_ = max; }
  bool check(Ts... x) override {
    const uint8_t value = this->parent_->control_value(this->control_);
    if (this->min_ == unspecified_) {
      return value <= this->max_;
    } else if (this->max_ == unspecified_) {
      return value >= this->min_;
    } else {
      return this->min_ <= value && value <= this->max_;
    }
  }

 protected:
  MidiInComponent *parent_;
  midi::MidiControlChangeNumber control_;
  const static uint8_t unspecified_ = 0xFF;
  uint8_t min_{unspecified_};
  uint8_t max_{unspecified_};
};

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
