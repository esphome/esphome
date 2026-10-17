#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include "midi_parser.h"

#include <esp_gattc_api.h>

#include <cstdint>
#include <vector>

namespace esphome::ble_midi {

namespace espbt = esphome::esp32_ble_tracker;

/// Client for devices implementing the standard BLE-MIDI service, e.g. wireless
/// MIDI keyboards and pad controllers. Incoming messages are exposed as
/// automation triggers; outgoing messages are sent with the ble_midi.send_*
/// actions.
///
/// Specification: "Specification for MIDI over Bluetooth Low Energy
/// (BLE-MIDI)", MIDI Association, document M1-2017-11-01.
class BLEMidi final : public Component, public ble_client::BLEClientNode {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

  void set_max_sysex_size(size_t size) { this->max_sysex_size_ = size; }
  void set_pair(bool pair) { this->pair_ = pair; }

  /// True once the MIDI characteristic has been discovered and notifications
  /// are enabled.
  bool is_ready() const { return this->notify_registered_; }

  // Channel is 0-15, note/velocity/controller/value/program are 0-127.
  void send_note_on(uint8_t note, uint8_t velocity, uint8_t channel);
  void send_note_off(uint8_t note, uint8_t velocity, uint8_t channel);
  void send_control_change(uint8_t control_number, uint8_t value, uint8_t channel);
  void send_program_change(uint8_t program, uint8_t channel);
  /// Pitch bend value is -8192 to 8191, centered on zero.
  void send_pitch_bend(int16_t value, uint8_t channel);
  /// Send a System Exclusive message. The 0xF0/0xF7 wrappers are added if the
  /// payload does not already carry them.
  void send_sysex(const std::vector<uint8_t> &payload);
  /// Send raw MIDI bytes (status byte followed by its data bytes).
  void send_raw(const std::vector<uint8_t> &bytes);

  void add_on_connect_trigger(Trigger<> *trigger) { this->connect_triggers_.push_back(trigger); }
  void add_on_disconnect_trigger(Trigger<> *trigger) { this->disconnect_triggers_.push_back(trigger); }
  void add_on_note_on_trigger(Trigger<uint8_t, uint8_t, uint8_t> *trigger) {
    this->note_on_triggers_.push_back(trigger);
  }
  void add_on_note_off_trigger(Trigger<uint8_t, uint8_t, uint8_t> *trigger) {
    this->note_off_triggers_.push_back(trigger);
  }
  void add_on_control_change_trigger(Trigger<uint8_t, uint8_t, uint8_t> *trigger) {
    this->control_change_triggers_.push_back(trigger);
  }
  void add_on_program_change_trigger(Trigger<uint8_t, uint8_t> *trigger) {
    this->program_change_triggers_.push_back(trigger);
  }
  void add_on_pitch_bend_trigger(Trigger<int16_t, uint8_t> *trigger) { this->pitch_bend_triggers_.push_back(trigger); }
  void add_on_sysex_trigger(Trigger<std::vector<uint8_t>> *trigger) { this->sysex_triggers_.push_back(trigger); }
  void add_on_message_trigger(Trigger<std::vector<uint8_t>> *trigger) { this->message_triggers_.push_back(trigger); }

 protected:
  void handle_message_(const MidiMessage &message);
  /// Wrap MIDI bytes in a BLE-MIDI packet and write them to the MIDI
  /// characteristic.
  bool write_midi_(const uint8_t *midi_bytes, size_t length);

  // Longest MIDI message that can be transmitted in a single BLE-MIDI packet,
  // header and timestamp excluded.
  static constexpr size_t MAX_TX_MIDI_BYTES = 128;

  MidiParser parser_;
  size_t max_sysex_size_{256};
  uint16_t characteristic_handle_{0};
  bool notify_registered_{false};
  // Write without response has lower latency and is preferred when the
  // characteristic supports it.
  bool write_without_response_{false};
  bool pair_{false};

  std::vector<Trigger<> *> connect_triggers_;
  std::vector<Trigger<> *> disconnect_triggers_;
  std::vector<Trigger<uint8_t, uint8_t, uint8_t> *> note_on_triggers_;
  std::vector<Trigger<uint8_t, uint8_t, uint8_t> *> note_off_triggers_;
  std::vector<Trigger<uint8_t, uint8_t, uint8_t> *> control_change_triggers_;
  std::vector<Trigger<uint8_t, uint8_t> *> program_change_triggers_;
  std::vector<Trigger<int16_t, uint8_t> *> pitch_bend_triggers_;
  std::vector<Trigger<std::vector<uint8_t>> *> sysex_triggers_;
  std::vector<Trigger<std::vector<uint8_t>> *> message_triggers_;
};

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
