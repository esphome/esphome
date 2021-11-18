#pragma once

#ifdef USE_ARDUINO

#include "esphome/core/component.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "midi.h"
namespace esphome {
namespace midi_in {
class MidiInComponent : public Component, public uart::UARTDevice {
 public:
  MidiInComponent(uart::UARTComponent *uart);

  void set_connected_binary_sensor(binary_sensor::BinarySensor *connected_binary_sensor) {
    connected_binary_sensor_ = connected_binary_sensor;
  }

  void set_playback_binary_sensor(binary_sensor::BinarySensor *playback_binary_sensor) {
    playback_binary_sensor_ = playback_binary_sensor;
  }

  void dump_config() override;

  void setup() override;
  void loop() override;

  void add_on_voice_message_callback(std::function<void(MidiChannelMessage)> &&callback) {
    this->channel_message_callback_.add(std::move(callback));
  }
  void add_on_system_message_callback(std::function<void(MidiSystemMessage)> &&callback) {
    this->system_message_callback_.add(std::move(callback));
  }

  uint8_t patch = 0;
  uint8_t soft_pedal = 0;
  uint8_t mid_pedal = 0;
  uint8_t sustain_pedal = 0;
  std::vector<uint8_t> note_velocities = std::vector<uint8_t>(128, 0);

 protected:
  std::unique_ptr<UARTSerialPort> serial_port_;
  std::unique_ptr<midi::SerialMIDI<UARTSerialPort>> serial_midi_;
  std::unique_ptr<midi::MidiInterface<midi::SerialMIDI<UARTSerialPort>>> midi_;

 protected:
  uint32_t last_activity_time_;
  uint32_t keys_on_;  // to track number of pressed keys to playback detection

  binary_sensor::BinarySensor *connected_binary_sensor_{nullptr};
  binary_sensor::BinarySensor *playback_binary_sensor_{nullptr};

  CallbackManager<void(MidiChannelMessage)> channel_message_callback_{};
  CallbackManager<void(MidiSystemMessage)> system_message_callback_{};

 private:
  void process_controller_message_(const MidiChannelMessage &msg);
  void log_message_(midi::MidiType type);

  void all_notes_off_();
  void reset_controllers_();

  void update_connected_binary_sensor_();
  void update_playback_binary_sensor_();
};

}  // namespace midi_in
}  // namespace esphome

#endif  // USE_ARDUINO
