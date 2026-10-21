#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include "esphome/components/ble_client/ble_client.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include "midi_parser.h"

#include <esp_gattc_api.h>

#include <cstdint>
#include <span>

namespace esphome::ble_midi {

namespace espbt = esphome::esp32_ble_tracker;

/// The bytes of a received message, valid for the duration of the automation.
using ByteSpan = std::span<const uint8_t>;

/// Longest MIDI message that can be transmitted in a single BLE-MIDI packet, header and timestamp excluded.
static constexpr size_t MAX_TX_MIDI_BYTES = 128;

/// Client for devices implementing the standard BLE-MIDI service, e.g. wireless
/// MIDI keyboards and pad controllers. Incoming messages are exposed as
/// automation triggers; outgoing messages are sent with the ble_midi.send_*
/// actions.
///
/// Specification: "Specification for MIDI over Bluetooth Low Energy
/// (BLE-MIDI)", MIDI Association, document M1-2017-11-01.
class BLEMidi final : public Component, public ble_client::BLEClientNode {
 public:
  /// sysex_buffer holds sysex_capacity bytes; longer SysEx messages are truncated.
  BLEMidi(uint8_t *sysex_buffer, uint16_t sysex_capacity, bool pair)
      : parser_(sysex_buffer, sysex_capacity), pair_(pair) {}

  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                           esp_ble_gattc_cb_param_t *param) override;

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
  void send_sysex(const uint8_t *payload, size_t length);
  /// Send raw MIDI bytes (status byte followed by its data bytes).
  void send_raw(const uint8_t *bytes, size_t length);

  template<typename F> void add_on_connect_callback(F &&callback) {
    this->connect_callback_.add(std::forward<F>(callback));
  }
  template<typename F> void add_on_disconnect_callback(F &&callback) {
    this->disconnect_callback_.add(std::forward<F>(callback));
  }
  /// (note, velocity, channel)
  template<typename F> void add_on_note_on_callback(F &&callback) {
    this->note_on_callback_.add(std::forward<F>(callback));
  }
  /// (note, velocity, channel); a Note On with velocity zero is reported here as well
  template<typename F> void add_on_note_off_callback(F &&callback) {
    this->note_off_callback_.add(std::forward<F>(callback));
  }
  /// (control_number, value, channel)
  template<typename F> void add_on_control_change_callback(F &&callback) {
    this->control_change_callback_.add(std::forward<F>(callback));
  }
  /// (program, channel)
  template<typename F> void add_on_program_change_callback(F &&callback) {
    this->program_change_callback_.add(std::forward<F>(callback));
  }
  /// (value, channel), value being -8192 to 8191
  template<typename F> void add_on_pitch_bend_callback(F &&callback) {
    this->pitch_bend_callback_.add(std::forward<F>(callback));
  }
  /// The SysEx payload without the 0xF0/0xF7 wrappers
  template<typename F> void add_on_sysex_callback(F &&callback) {
    this->sysex_callback_.add(std::forward<F>(callback));
  }
  /// The raw bytes of every decoded message
  template<typename F> void add_on_message_callback(F &&callback) {
    this->message_callback_.add(std::forward<F>(callback));
  }

 protected:
  static void dispatch_message(void *ctx, const MidiMessage &message) {
    static_cast<BLEMidi *>(ctx)->handle_message_(message);
  }
  void handle_message_(const MidiMessage &message);
  /// Wrap MIDI bytes in a BLE-MIDI packet and write them to the MIDI characteristic.
  bool write_midi_(const uint8_t *midi_bytes, size_t length);
  /// Write a packet whose MIDI bytes already sit at packet + 2; the header and timestamp are filled in.
  bool write_packet_(uint8_t *packet, size_t midi_length);
  /// Channel-voice message: the status byte takes the channel, the data bytes are masked to 7 bits.
  void send_channel_(uint8_t status, uint8_t channel, uint8_t data1, uint8_t data2, size_t length);

  MidiParser parser_;
  LazyCallbackManager<void()> connect_callback_;
  LazyCallbackManager<void()> disconnect_callback_;
  LazyCallbackManager<void(uint8_t, uint8_t, uint8_t)> note_on_callback_;
  LazyCallbackManager<void(uint8_t, uint8_t, uint8_t)> note_off_callback_;
  LazyCallbackManager<void(uint8_t, uint8_t, uint8_t)> control_change_callback_;
  LazyCallbackManager<void(uint8_t, uint8_t)> program_change_callback_;
  LazyCallbackManager<void(int16_t, uint8_t)> pitch_bend_callback_;
  LazyCallbackManager<void(ByteSpan)> sysex_callback_;
  LazyCallbackManager<void(ByteSpan)> message_callback_;
  uint16_t characteristic_handle_{0};
  bool notify_registered_{false};
  // Write without response has lower latency and is preferred when the
  // characteristic supports it.
  bool write_without_response_{false};
  const bool pair_;
};

}  // namespace esphome::ble_midi

#endif  // USE_ESP32
