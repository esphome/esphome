#ifdef USE_ARDUINO

#include "midi_in.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midi_in {

static const char *const TAG = "midi_in";

MidiInComponent::MidiInComponent(uart::UARTComponent *uart) : uart::UARTDevice(uart) {
  this->serial_port_ = make_unique<UARTSerialPort>(this);
  this->serial_midi_ = make_unique<midi::SerialMIDI<UARTSerialPort>>(*this->serial_port_);
  this->midi_ = make_unique<midi::MidiInterface<midi::SerialMIDI<UARTSerialPort>>>(*this->serial_midi_);
}

void MidiInComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MIDI In");
  LOG_BINARY_SENSOR("  ", "Connection", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Playback", this->playback_binary_sensor_);
  this->check_uart_settings(31250);
}

void MidiInComponent::setup() { this->midi_->begin(); }

void MidiInComponent::loop() {
  while (this->midi_->read()) {
    this->last_activity_time_ = millis();

    auto msg = MidiVoiceMessage{.command = this->midi_->getType(),
                                .channel = static_cast<uint8_t>(this->midi_->getChannel()),
                                .param1 = static_cast<uint8_t>(this->midi_->getData1()),
                                .param2 = static_cast<uint8_t>(this->midi_->getData2())};

    switch (msg.command) {
      case midi::MidiType::NoteOff:
        if (this->note_velocities[msg.param1] > 0) {
          this->note_velocities[msg.param1] = 0;
          this->keys_on_--;
        }
        this->voice_message_callback_.call(msg);
        break;
      case midi::MidiType::NoteOn:
        if (msg.param2 > 0) {
          if (this->note_velocities[msg.param1] == 0) {
            this->keys_on_++;
          }
          this->note_velocities[msg.param1] = msg.param2;
        } else {
          // this is actualy NOTE OFF
          if (this->note_velocities[msg.param1] > 0) {
            this->note_velocities[msg.param1] = 0;
            this->keys_on_--;
          }
        }
        this->voice_message_callback_.call(msg);
        break;
      case midi::MidiType::ControlChange:
        this->process_controller_message_(msg);
        this->voice_message_callback_.call(msg);
        break;
      default:
        break;
    }

    this->log_message_(msg);
  }

  this->update_connected_binary_sensor_();
  this->update_playback_binary_sensor_();
}

void MidiInComponent::process_controller_message_(const MidiVoiceMessage &msg) {
  switch (msg.param1) {
    case midi::MidiControlChangeNumber::Sustain:
      this->sustain_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::Sostenuto:
      this->mid_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::SoftPedal:
      this->soft_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::AllSoundOff:
      this->all_notes_off_();
      break;
    case midi::MidiControlChangeNumber::ResetAllControllers:
      this->reset_controllers_();
      break;
    case midi::MidiControlChangeNumber::AllNotesOff:
      this->all_notes_off_();
      break;
    case midi::MidiControlChangeNumber::PolyModeOn:
      // Poly operation and all notes off
      this->all_notes_off_();
      break;
    default:
      break;
  }
}

void MidiInComponent::log_message_(const MidiVoiceMessage &msg) {
  const LogString *midi_type_s = midi_type_to_string(msg.command);
  switch (msg.command) {
  case midi::MidiType::Tick:
  case midi::MidiType::ActiveSensing:
    ESP_LOGVV(TAG, "%s", LOG_STR_ARG(midi_type_s));
    break;
  case midi::MidiType::ControlChange: {
    const LogString *midi_control_s = midi_controller_to_string(static_cast<midi::MidiControlChangeNumber>(msg.param1));
    ESP_LOGV(TAG, "%s[%i]: %#04x", LOG_STR_ARG(midi_control_s), msg.channel, msg.param2);
    break;
  }
  default:
    if (this->midi_->isChannelMessage(msg.command)) {
      ESP_LOGV(TAG, "%s[%i]: %#04x %#04x", LOG_STR_ARG(midi_type_s), msg.channel, msg.param1, msg.param2);
    } else {
      ESP_LOGV(TAG, "%s: %#04x %#04x", msg.channel, LOG_STR_ARG(midi_type_s), msg.param1, msg.param2);
    }
    break;
  }   
}

void MidiInComponent::all_notes_off_() {
  this->keys_on_ = 0;
  std::fill(this->note_velocities.begin(), this->note_velocities.end(), 0);
}

void MidiInComponent::reset_controllers_() {
  this->soft_pedal = 0;
  this->mid_pedal = 0;
  this->sustain_pedal = 0;
}

void MidiInComponent::update_connected_binary_sensor_() {
  if (this->connected_binary_sensor_) {
    uint32_t millis_since_last_active_sense = millis() - this->last_activity_time_;
    // normal active sense interval is 300ms
    if (millis_since_last_active_sense >= 500) {      
      // disconnected
      this->reset_controllers_();
      this->all_notes_off_();
      if (this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(false);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->connected_binary_sensor_->state = false;  
      }
    } else {
      // connected
      if (!this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(true);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->connected_binary_sensor_->state = true;  
      }
    }
  }
}

void MidiInComponent::update_playback_binary_sensor_() {
  if (this->playback_binary_sensor_) {
    if (this->keys_on_ > 0 || this->soft_pedal > 0 || this->mid_pedal > 0 || this->sustain_pedal > 0) {
      // playing
      if (!this->playback_binary_sensor_->state) {
        this->playback_binary_sensor_->publish_state(true);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->playback_binary_sensor_->state = true;
      }
    } else {
      // not playing
      if (this->playback_binary_sensor_->state) {
        this->playback_binary_sensor_->publish_state(false);
        // this is needed to stop multiple publish calls, because publish is delayed:
        this->playback_binary_sensor_->state = false;
      }
    }
  }
}

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
