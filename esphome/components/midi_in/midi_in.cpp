#include "midi_in.h"
#include "esphome/core/log.h"

namespace esphome {
namespace midi_in {

static const char *const TAG = "midi_in";

MidiInComponent::MidiInComponent(uart::UARTComponent *uart) : uart::UARTDevice(uart) {}

void MidiInComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MIDI In");
  LOG_BINARY_SENSOR("  ", "Connection", this->connected_binary_sensor_);
  LOG_BINARY_SENSOR("  ", "Playback", this->playback_binary_sensor_);
  this->check_uart_settings(31250);
}

void MidiInComponent::loop() {
  while (available()) {
    this->last_activity_time_ = millis();
    uint8_t byte1 = read();

    if ((byte1 & MASK_COMMAND) == MidiCommand::SYSTEM) {
      this->process_system_message_(byte1);
      this->system_message_callback_.call(MidiSystemMessage{.command = static_cast<MidiCommand>(byte1)});
    } else {
      auto msg = MidiVoiceMessage{.command = static_cast<MidiCommand>(byte1 & MASK_COMMAND),
                                  .channel = static_cast<uint8_t>(byte1 & MASK_CHANNEL)};

      switch (msg.command) {
        case MidiCommand::NOTE_OFF:
          msg.param1 = read();
          msg.param2 = read();
          // ESP_LOGD(TAG, "NOTE OFF: %#04x (channel %i)", msg.param1, msg.channel);
          if (this->note_velocities[msg.param1] > 0) {
            this->note_velocities[msg.param1] = 0;
            this->keys_on_--;
          }
          this->voice_message_callback_.call(msg);
          break;
        case MidiCommand::NOTE_ON:
          msg.param1 = read();
          msg.param2 = read();
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
        case MidiCommand::AFTERTOUCH:
          msg.param1 = read();
          msg.param2 = read();
          // ESP_LOGD(TAG, "AFTERTOUCH: %#04x (touch %#04x, channel %i)", msg.param1, msg.param2, msg.channel);
          this->voice_message_callback_.call(msg);
        case MidiCommand::CONTROLLER:
          // MIDI controller message tells a MIDI device that at a certain time the value of some controller should
          // change. https://www.recordingblogs.com/wiki/midi-controller-message
          msg.param1 = read();
          msg.param2 = read();
          this->process_controller_message_(msg);
          this->voice_message_callback_.call(msg);
          break;
        case MidiCommand::PATCH_CHANGE:
          msg.param1 = read();
          ESP_LOGD(TAG, "PATCH CHANGE: %#02x (channel %i)", msg.param1, msg.channel);
          this->patch = msg.param1;
          this->voice_message_callback_.call(msg);
          break;
        case MidiCommand::CHANNEL_PRESSURE:
          msg.param1 = read();
          // ESP_LOGD(TAG, "CHANNEL PRESSURE: %#04x (channel: %i)", msg.param1, msg.channel);
          this->voice_message_callback_.call(msg);
          break;
        case MidiCommand::PITCH_BEND:
          // parametersparam 1      param 2
          // 2        lsb (7 bits)msb (7 bits)
          msg.param1 = read();
          msg.param2 = read();
          // ESP_LOGD(TAG, "PITCH BEND: %#04x %#04x (channel: %i)", msg.param1, msg.param2, msg.channel);
          this->voice_message_callback_.call(msg);
          break;
        default:
          ESP_LOGW(TAG, "Unexpected data: %#08x", byte1);
          break;
      }
    }
  }

  this->update_connected_binary_sensor_();
  this->update_playback_binary_sensor_();
}

void MidiInComponent::process_system_message_(uint8_t command) {
  uint8_t data;
  switch (command) {
    case MidiCommand::SYSTEM_EXCLUSIVE:
      // The MIDI system exclusive message consists of virtually unlimited bytes of data.
      read();  // manufacturer id
      if (available()) {
        do {
          data = read();
        } while (data != MidiCommand::SYSTEM_EXCLUSIVE_END);
      }
      break;
    case MidiCommand::SYSTEM_TIMING_CLOCK:
      // Sent 24 times per quarter note when synchronization is required.
      break;
    case MidiCommand::SYSTEM_START:
      ESP_LOGD(TAG, "System Start");
      break;
    case MidiCommand::SYSTEM_CONTINUE:
      ESP_LOGD(TAG, "System Continue");
      break;
    case MidiCommand::SYSTEM_STOP:
      ESP_LOGD(TAG, "System Stop");
      break;
    case MidiCommand::SYSTEM_ACTIVE_SENSING:
      // Active Sensing (Sys Realtime).
      // This message is intended to be sent repeatedly to tell the receiver that a connection is alive.
      // Use of this message is optional.
      // When initially received, the receiver will expect to receive another Active Sensing message each 300ms (max),
      // and if it does not then it will assume that the connection has been terminated.
      // At termination, the receiver will turn off all voices and return to normal (non-active sensing) operation.
      break;
    case MidiCommand::SYSTEM_RESET:
      // Reset.
      // Reset all receivers in the system to power-up status.
      // This should be used sparingly, preferably under manual control.
      // In particular, it should not be sent on power-up.
      ESP_LOGD(TAG, "System Reset");
      break;
    default:
      ESP_LOGD(TAG, "Unsupported System command: %#02x", command);
      break;
  }
}

void MidiInComponent::process_controller_message_(const MidiVoiceMessage &msg) {
  switch (msg.param1) {
    case MidiController::BANK_SELECT:
      ESP_LOGD(TAG, "Bank select (coarse): %#04x (channel %i)", msg.param2, msg.channel);
      break;
    case MidiController::CHANNEL_VOLUME:
      ESP_LOGD(TAG, "Main volume: %#04x (channel %i)", msg.param2, msg.channel);
      break;
    case MidiController::BANK_SELECT_FINE:
      ESP_LOGD(TAG, "Bank select (fine): %#04x (channel %i)", msg.param2, msg.channel);
      break;
    case MidiController::SUSTAIN:
      // ESP_LOGD(TAG, "Sustain pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->sustain_pedal = msg.param2;
      break;
    case MidiController::SUSTENUTO:
      // ESP_LOGD(TAG, "Mid pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->mid_pedal = msg.param2;
      break;
    case MidiController::SOFT_PEDAL:
      // ESP_LOGD(TAG, "Soft pedal: %#04x (channel %i)", msg.param2, msg.channel);
      this->soft_pedal = msg.param2;
      break;
    case MidiController::ALL_SOUND_OFF:
      ESP_LOGD(TAG, "All sounds off (channel %i)", msg.channel);
      this->all_notes_off_();
      break;
    case MidiController::RESET_ALL_CONTROLLERS:
      ESP_LOGD(TAG, "Reset all controllers (channel %i)", msg.channel);
      this->reset_controllers_();
      break;
    case MidiController::ALL_NOTES_OFF:
      ESP_LOGD(TAG, "All notes off (channel %i)", msg.channel);
      this->all_notes_off_();
      break;
    case MidiController::POLY_MODE_ON:
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
      // this->soft_pedal = 0;
      // this->mid_pedal = 0;
      // this->sustain_pedal = 0;
      // this->keys_on_ = 0;
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
