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
  ESP_LOGCONFIG(TAG, "  Channel: %i", this->channel_);
  LOG_BINARY_SENSOR("  ", "Connection", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Playback", this->playback_binary_sensor_);
  this->check_uart_settings(31250);
}

void MidiInComponent::setup() { this->midi_->begin(); }

void MidiInComponent::loop() {
  while (available()) {
    if (this->midi_->read(this->channel_)) {
      this->last_activity_time_ = millis();

      midi::MidiType message_type = this->midi_->getType();

      this->log_message_(message_type);

      if (this->midi_->isChannelMessage(message_type)) {
        auto msg = MidiChannelMessage{.type = message_type,
                                      .channel = static_cast<uint8_t>(this->midi_->getChannel()),
                                      .data1 = static_cast<uint8_t>(this->midi_->getData1()),
                                      .data2 = static_cast<uint8_t>(this->midi_->getData2())};

        switch (message_type) {
          case midi::MidiType::NoteOff:
            if (this->note_velocities_[msg.data1] > 0) {
              this->note_velocities_[msg.data1] = 0;
              this->keys_on_--;
            }
            break;
          case midi::MidiType::NoteOn:
            if (msg.data2 > 0) {
              if (this->note_velocities_[msg.data1] == 0) {
                this->keys_on_++;
              }
              this->note_velocities_[msg.data1] = msg.data2;
            } else {
              // this is actualy NOTE OFF
              if (this->note_velocities_[msg.data1] > 0) {
                this->note_velocities_[msg.data1] = 0;
                this->keys_on_--;
              }
            }
            break;
          case midi::MidiType::ProgramChange:
            this->program_ = msg.data1;
            break;
          case midi::MidiType::ControlChange:
            this->process_controller_message_(msg);
            break;
          default:
            break;
        }

        this->channel_message_callback_.call(msg);
      } else {
        this->system_message_callback_.call(MidiSystemMessage{.type = message_type});
      }
    }
  }

  this->update_connected_binary_sensor_();
  this->update_playback_binary_sensor_();
}

void MidiInComponent::process_controller_message_(const MidiChannelMessage &msg) {
  this->control_values_[msg.data1] = msg.data2;
  switch (msg.data1) {
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

void MidiInComponent::log_message_(midi::MidiType type) {
  const LogString *midi_type_s = midi_type_to_string(type);
  switch (type) {
    case midi::MidiType::Tick:
    case midi::MidiType::ActiveSensing:
      ESP_LOGVV(TAG, "%s", LOG_STR_ARG(midi_type_s));
      break;
    case midi::MidiType::ControlChange: {
      const LogString *midi_control_s =
          midi_controller_to_string(static_cast<midi::MidiControlChangeNumber>(this->midi_->getData1()));
      ESP_LOGV(TAG, "%s[%i]: %#04x", LOG_STR_ARG(midi_control_s), this->midi_->getChannel(), this->midi_->getData2());
      break;
    }
    default:
      if (this->midi_->isChannelMessage(type)) {
        ESP_LOGV(TAG, "%s[%i]: %#04x %#04x", LOG_STR_ARG(midi_type_s), this->midi_->getChannel(),
                 this->midi_->getData1(), this->midi_->getData2());
      } else {
        ESP_LOGV(TAG, "%s: %#04x %#04x", LOG_STR_ARG(midi_type_s), this->midi_->getData1(), this->midi_->getData2());
      }
      break;
  }
}

void MidiInComponent::all_notes_off_() {
  this->keys_on_ = 0;
  std::fill(this->note_velocities_.begin(), this->note_velocities_.end(), 0);
}

void MidiInComponent::reset_controllers_() {
  std::fill(this->control_values_.begin(), this->control_values_.end(), 0);
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
