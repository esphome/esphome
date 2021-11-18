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

    switch (this->midi_->getType()) {
      case midi::MidiType::NoteOff:
        // ESP_LOGD(TAG, "NOTE OFF: %#04x (channel %i)", msg.param1, msg.channel);
        if (this->note_velocities[msg.param1] > 0) {
          this->note_velocities[msg.param1] = 0;
          this->keys_on_--;
        }
        this->voice_message_callback_.call(msg);
        break;
      case midi::MidiType::NoteOn:
        // ESP_LOGD(TAG, "NOTE ON: %#04x (velocity %#04x, channel %i)", msg.param1, msg.param2, msg.channel);
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
        // MIDI controller message tells a MIDI device that at a certain time the value of some controller should
        // change. https://www.recordingblogs.com/wiki/midi-controller-message
        this->process_controller_message_(msg);
        this->voice_message_callback_.call(msg);
        break;
      default:
        break;
    }
  }
  //   if ((byte1 & MASK_COMMAND) == midi::MidiType::SystemExclusive) {
  //     this->process_system_message_(byte1);
  //     this->system_message_callback_.call(MidiSystemMessage{.command = static_cast<midi::MidiType>(byte1)});
  //   } else {
  //     auto msg = MidiVoiceMessage{.command = static_cast<midi::MidiType>(byte1 & MASK_COMMAND),
  //                                 .channel = static_cast<uint8_t>(byte1 & MASK_CHANNEL)};

  //       case midi::MidiType::AfterTouchPoly:
  //         msg.param1 = read();
  //         msg.param2 = read();
  //         // ESP_LOGD(TAG, "AFTERTOUCH: %#04x (touch %#04x, channel %i)", msg.param1, msg.param2, msg.channel);
  //         this->voice_message_callback_.call(msg);

  //       case midi::MidiType::ProgramChange:
  //         msg.param1 = read();
  //         ESP_LOGD(TAG, "PATCH CHANGE: %#02x (channel %i)", msg.param1, msg.channel);
  //         this->patch = msg.param1;
  //         this->voice_message_callback_.call(msg);
  //         break;
  //       case midi::MidiType::AfterTouchChannel:
  //         msg.param1 = read();
  //         // ESP_LOGD(TAG, "CHANNEL PRESSURE: %#04x (channel: %i)", msg.param1, msg.channel);
  //         this->voice_message_callback_.call(msg);
  //         break;
  //       case midi::MidiType::PitchBend:
  //         // parametersparam 1      param 2
  //         // 2        lsb (7 bits)msb (7 bits)
  //         msg.param1 = read();
  //         msg.param2 = read();
  //         // ESP_LOGD(TAG, "PITCH BEND: %#04x %#04x (channel: %i)", msg.param1, msg.param2, msg.channel);
  //         this->voice_message_callback_.call(msg);
  //         break;
  //       default:
  //         ESP_LOGW(TAG, "Unexpected data: %#08x", byte1);
  //         break;
  //     }
  //   }
  // }

  this->update_connected_binary_sensor_();
  this->update_playback_binary_sensor_();
}

void MidiInComponent::process_controller_message_(const MidiVoiceMessage &msg) {
  switch (msg.param1) {
    case midi::MidiControlChangeNumber::BankSelect:
      ESP_LOGD(TAG, "Bank select (coarse): %#04x (channel %i)", msg.param2, msg.channel);
      break;
    case midi::MidiControlChangeNumber::ChannelVolume:
      ESP_LOGD(TAG, "Main volume: %#04x (channel %i)", msg.param2, msg.channel);
      break;
    // case midi::MidiControlChangeNumber::BANK_SELECT_FINE:
    //   ESP_LOGD(TAG, "Bank select (fine): %#04x (channel %i)", msg.param2, msg.channel);
    //   break;
    case midi::MidiControlChangeNumber::Sustain:
      // ESP_LOGD(TAG, "Sustain pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->sustain_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::Sostenuto:
      // ESP_LOGD(TAG, "Mid pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->mid_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::SoftPedal:
      // ESP_LOGD(TAG, "Soft pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->soft_pedal = msg.param2;
      break;
    case midi::MidiControlChangeNumber::AllSoundOff:
      ESP_LOGD(TAG, "All sounds off (channel %i)", msg.channel);
      this->all_notes_off_();
      break;
    case midi::MidiControlChangeNumber::ResetAllControllers:
      ESP_LOGD(TAG, "Reset all controllers (channel %i)", msg.channel);
      this->reset_controllers_();
      break;
    case midi::MidiControlChangeNumber::AllNotesOff:
      ESP_LOGD(TAG, "All notes off (channel %i)", msg.channel);
      this->all_notes_off_();
      break;
    case midi::MidiControlChangeNumber::PolyModeOn:
      // Poly operation and all notes off
      ESP_LOGD(TAG, "Poly operation and all notes off (channel %i)", msg.channel);
      this->all_notes_off_();
      break;
    default:
      ESP_LOGD(TAG, "Unknown continuous controller: %#04x %#04x (channel %i)", msg.param1, msg.param2, msg.channel);
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
    if (millis_since_last_active_sense >= 500) {  // normal active sense interval is 300ms
      // disconnected
      this->reset_controllers_();
      this->all_notes_off_();
      if (this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(false);
        this->connected_binary_sensor_->state =
            false;  // this is needed to stop multiple publish calls, because publish is delayed
      }
    } else {
      // connected
      if (!this->connected_binary_sensor_->state) {
        this->connected_binary_sensor_->publish_state(true);
        this->connected_binary_sensor_->state =
            true;  // this is needed to stop multiple publish calls, because publish is delayed
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
        this->playback_binary_sensor_->state =
            true;  // this is needed to stop multiple publish calls, because publish is delayed
      }
    } else {
      // not playing
      if (this->playback_binary_sensor_->state) {
        this->playback_binary_sensor_->publish_state(false);
        this->playback_binary_sensor_->state =
            false;  // this is needed to stop multiple publish calls, because publish is delayed
      }
    }
  }
}

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
