#include "midi_in.h"
#include "esphome/core/log.h"

namespace esphome
{
  namespace midi
  {

    static const char *const TAG = "midi_in";

    MidiInComponent::MidiInComponent(uart::UARTComponent *uart) : uart::UARTDevice(uart)
    {
    }

    void MidiInComponent::dump_config()
    {
      ESP_LOGCONFIG(TAG, "MIDI In");
      LOG_BINARY_SENSOR("  ", "Connection", this->connected_binary_sensor_);
      LOG_BINARY_SENSOR("  ", "Playback", this->playback_binary_sensor_);
      this->check_uart_settings(31250);
    }

    void MidiInComponent::loop()
    {
      while (available())
      {
        this->last_activity_time_ = millis();
        uint8_t byte1 = read();

        if ((byte1 & MASK_COMMAND) == MidiCommand::SYSTEM)
        {
          this->process_system_message_(byte1);
          // this->system_message_callback_.call(MidiSystemMessage{
          //   .command = byte1
          // });
        }
        else
        {

          auto msg = MidiVoiceMessage{
              .command = static_cast<MidiCommand>(byte1 & MASK_COMMAND),
              .channel = byte1 & MASK_CHANNEL};

          switch (msg.command)
          {
          case MidiCommand::NOTE_OFF:
            msg.param1 = read();
            msg.param2 = read();
            //ESP_LOGD(TAG, "NOTE OFF: %#04x (channel %i)", msg.param1, msg.channel);
            if (this->note_velocities[msg.param1] > 0)
            {
              this->note_velocities[msg.param1] = 0;
              this->keys_on_--;
            }
            this->voice_message_callback_.call(msg);
            break;
          case MidiCommand::NOTE_ON:
            // The NOTE ON message is structured as follows:
            // Status byte : 1001 CCCC
            // Data byte 1 : 0PPP PPPP
            // Data byte 2 : 0VVV VVVV
            msg.param1 = read();
            msg.param2 = read();
            //ESP_LOGD(TAG, "NOTE ON: %#04x (velocity %#04x, channel %i)", msg.param1, msg.param2, msg.channel);
            if (msg.param2 > 0)
            {
              if (this->note_velocities[msg.param1] == 0)
              {
                this->keys_on_++;
              }
              this->note_velocities[msg.param1] = msg.param2;
            }
            else
            {
              // this is actualy NOTE OFF
              if (this->note_velocities[msg.param1] > 0)
              {
                this->note_velocities[msg.param1] = 0;
                this->keys_on_--;
              }
            }
            this->voice_message_callback_.call(msg);
            break;
          case MidiCommand::AFTERTOUCH:
            msg.param1 = read();
            msg.param2 = read();
            //ESP_LOGD(TAG, "AFTERTOUCH: %#04x (touch %#04x, channel %i)", msg.param1, msg.param2, msg.channel);
            this->voice_message_callback_.call(msg);
          case MidiCommand::CONTROLLER:
            // parameters	param 1	      param 2
            // 2	        controller #	controller value
            // https://www.recordingblogs.com/wiki/midi-controller-message
            // MIDI controller message tells a MIDI device that at a certain time the value of some controller should change.
            msg.param1 = read();
            msg.param2 = read();
            this->process_controller_message_(msg);
            this->voice_message_callback_.call(msg);
            break;
          case MidiCommand::PATCH_CHANGE:
            // parameters	param 1	      param 2
            // 1	        instrument #
            msg.param1 = read();
            ESP_LOGD(TAG, "PATCH CHANGE: %#02x (channel %i)", msg.param1, msg.channel);
            this->patch = msg.param1;
            this->voice_message_callback_.call(msg);
            break;
          case MidiCommand::CHANNEL_PRESSURE:
            // Channel Pressure
            // parameters	param 1	      param 2
            // 1	        pressure
            msg.param1 = read();
            //ESP_LOGD(TAG, "CHANNEL PRESSURE: %#04x (channel: %i)", msg.param1, msg.channel);
            this->voice_message_callback_.call(msg);
            break;
          case MidiCommand::PITCH_BEND:
            // Pitch bend
            // parameters	param 1	      param 2
            // 2	        lsb (7 bits)	msb (7 bits)
            msg.param1 = read();
            msg.param2 = read();
            //ESP_LOGD(TAG, "PITCH BEND: %#04x %#04x (channel: %i)", msg.param1, msg.param2, msg.channel);
            this->voice_message_callback_.call(msg);
            break;
          default:
            ESP_LOGE(TAG, "Unexpected data: %#08x", byte1);
            break;
          }
        }
      }

      this->update_connected_binary_sensor_();
      this->update_playback_binary_sensor_();
    }

    void MidiInComponent::process_system_message_(const uint8_t command)
    {
      uint8_t data;
      switch (command)
      {
      case MidiCommand::SYSTEM_EXCLUSIVE:
        // The MIDI system exclusive message consists of virtually unlimited bytes of data.
        data = read(); // manufacturer id
        if (available())
        {
          do
          {
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
        this->last_active_sense_time_ = millis();
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

    void MidiInComponent::process_controller_message_(const MidiVoiceMessage &msg)
    {
      switch (msg.param1)
      {
      case 0x00:
        // Bank select (coarse)	0-127	GM2
        ESP_LOGD(TAG, "Bank select (coarse): %#04x (channel %i)", msg.param2, msg.channel);
        break;
      case 0x07:
        // Channel volume (coarse) (formerly main volume)	0-127	GM1, GM2
        ESP_LOGD(TAG, "Main volume: %#04x (channel %i)", msg.param2, msg.channel);
        break;
      case 0x20:
        // Bank select (fine)	0-127	GM2
        ESP_LOGD(TAG, "Bank select (fine): %#04x (channel %i)", msg.param2, msg.channel);
        break;
      case 0x40:
        // sustain pedal Hold (damper, sustain) pedal 1 (on/off)	= 64 is on
        //ESP_LOGD(TAG, "Sustain pedal: %#04x (channel %i)", msg.param2, msg.channel);
        this->sustain_pedal = msg.param2;
        break;
      case 0x42:
        // mid pedal (Sostenuto pedal (on/off)	= 64 is on)
        //ESP_LOGD(TAG, "Mid pedal: %#04x (channel %i)", msg.param2, msg.channel);
        this->mid_pedal = msg.param2;
        break;
      case 0x43:
        // Soft pedal (on/off)	= 64 is on
        //ESP_LOGD(TAG, "Soft pedal: %#04x (channel %i)", msg.param2, msg.channel);
        this->soft_pedal = msg.param2;
        break;
      case 0x7B:
        // All notes off	0	GM1, GM2
        this->keys_on_ = 0;
        ESP_LOGD(TAG, "All notes off (channel %i)", msg.channel);
        std::fill(this->note_velocities.begin(), this->note_velocities.end(), 0);
        break;
      case 0x7F:
        // Poly operation and all notes off
        this->keys_on_ = 0;
        ESP_LOGD(TAG, "Poly operation and all notes off (channel %i)", msg.channel);
        std::fill(this->note_velocities.begin(), this->note_velocities.end(), 0);
        break;
      default:
        ESP_LOGD(TAG, "Unknown continuous controller: %#04x %#04x (channel %i)", msg.param1, msg.param2, msg.channel);
        break;
      }
    }

    void MidiInComponent::update_connected_binary_sensor_()
    {
      if (this->connected_binary_sensor_)
      {
        uint32_t millis_since_last_active_sense = millis() - this->last_activity_time_; //last_active_sense_time_;
        if (millis_since_last_active_sense >= 500)
        { // normal active sense interval is 300ms
          // disconnected
          // this->soft_pedal = 0;
          // this->mid_pedal = 0;
          // this->sustain_pedal = 0;
          // this->keys_on_ = 0;
          if (this->connected_binary_sensor_->state)
          {
            this->connected_binary_sensor_->publish_state(false);
            this->connected_binary_sensor_->state = false; // this is needed to stop multiple publish calls, because publish is delayed
          }
        }
        else
        {
          // connected
          if (!this->connected_binary_sensor_->state)
          {
            this->connected_binary_sensor_->publish_state(true);
            this->connected_binary_sensor_->state = true; // this is needed to stop multiple publish calls, because publish is delayed
          }
        }
      }
    }

    void MidiInComponent::update_playback_binary_sensor_()
    {
      if (this->playback_binary_sensor_)
      {
        if (this->keys_on_ > 0 || this->soft_pedal > 0 || this->mid_pedal > 0 || this->sustain_pedal > 0)
        {
          // playing
          if (!this->playback_binary_sensor_->state)
          {
            this->playback_binary_sensor_->publish_state(true);
            this->playback_binary_sensor_->state = true; // this is needed to stop multiple publish calls, because publish is delayed
          }
        }
        else
        {
          // not playing
          if (this->playback_binary_sensor_->state)
          {
            this->playback_binary_sensor_->publish_state(false);
            this->playback_binary_sensor_->state = false; // this is needed to stop multiple publish calls, because publish is delayed
          }
        }
      }
    }

  } // namespace state_machine
} // namespace esphome