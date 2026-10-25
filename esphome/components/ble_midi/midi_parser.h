#pragma once

#include "esphome/core/progmem.h"

#include <cstddef>
#include <cstdint>

namespace esphome::ble_midi {

// Decoded MIDI message types. Status bytes have the high bit set; the low nibble of a channel-voice status byte
// (0x80-0xEF) is the channel, system messages (0xF0-0xFF) have none.
enum class MessageType : uint8_t {
  NOTE_OFF,            // 0x80
  NOTE_ON,             // 0x90
  POLY_AFTERTOUCH,     // 0xA0
  CONTROL_CHANGE,      // 0xB0
  PROGRAM_CHANGE,      // 0xC0
  CHANNEL_AFTERTOUCH,  // 0xD0
  PITCH_BEND,          // 0xE0
  SYSEX,               // 0xF0 ... 0xF7, possibly spread over several BLE packets
  SYSEX_INCOMPLETE,    // a SysEx cut short by another status byte or longer than the buffer
  SYSTEM_COMMON,       // 0xF1-0xF6, e.g. quarter frame, song position
  SYSTEM_REAL_TIME,    // 0xF8-0xFF, e.g. clock, start, stop
  INCOMPLETE,          // a channel or system common message cut short by another status byte
  UNKNOWN,             // malformed/undecodable byte
};

PROGMEM_STRING_TABLE(MessageTypeNames, "NOTE_OFF", "NOTE_ON", "POLY_AFTERTOUCH", "CONTROL_CHANGE", "PROGRAM_CHANGE",
                     "CHANNEL_AFTERTOUCH", "PITCH_BEND", "SYSEX", "SYSEX_INCOMPLETE", "SYSTEM_COMMON",
                     "SYSTEM_REAL_TIME", "INCOMPLETE", "UNKNOWN");

/// A decoded MIDI message. `data` points into the parser's storage or the packet being fed, so it is only valid
/// for the duration of the handler.
struct MidiMessage {
  // Status byte followed by its data bytes; a SysEx includes the 0xF0/0xF7 wrappers
  const uint8_t *data{nullptr};
  uint16_t length{0};
  MessageType type{MessageType::UNKNOWN};

  uint8_t status() const { return this->data[0]; }
  uint8_t channel() const { return this->data[0] & 0x0F; }
  // The data bytes of a complete channel-voice message: note and velocity, controller and value, or program
  uint8_t data1() const { return this->data[1]; }
  uint8_t data2() const { return this->data[2]; }
  // PITCH_BEND: 14-bit value re-centered on zero (-8192..8191).
  int16_t pitch_bend() const {
    return static_cast<int16_t>((static_cast<uint16_t>(this->data[2]) << 7) | this->data[1]) - 8192;
  }
};

/// Called once per decoded message with the context passed to feed().
using MidiMessageHandler = void (*)(void *ctx, const MidiMessage &message);

/// Decoder for the BLE-MIDI packet format (Apple/MMA "Bluetooth Low Energy
/// MIDI" specification, section 3).
///
/// Packet layout:
///   [header] [timestamp] [MIDI 0] ... ([timestamp] [MIDI n])*
///
/// - header    : 0b10hhhhhh, bit 7 set, bits 5-0 hold the high bits of the
/// timestamp
/// - timestamp : 0b1lllllll, bit 7 set, bits 6-0 hold the low bits of the
/// timestamp
///
/// A timestamp byte precedes every status byte, except where running status
/// applies. SysEx messages may span several BLE packets and may be interrupted
/// by real-time messages (0xF8-0xFF).
///
/// Timestamps are used for jitter reconstruction by MIDI hosts and are ignored
/// here.
///
/// This class holds no BLE or ESPHome state so that it can be exercised in
/// isolation.
class MidiParser final {
 public:
  /// `sysex_buffer` reassembles SysEx messages; one longer than `sysex_capacity` bytes is reported incomplete.
  MidiParser(uint8_t *sysex_buffer, uint16_t sysex_capacity)
      : sysex_buffer_(sysex_buffer), sysex_capacity_(sysex_capacity) {}

  uint16_t sysex_capacity() const { return this->sysex_capacity_; }

  /// Drop all parser state. Call on disconnect so running status and partial
  /// messages do not leak into the next session.
  void reset();

  /// Feed one BLE notification payload. Undecodable bytes are reported as MessageType::UNKNOWN so the
  /// caller can log them.
  void feed(const uint8_t *data, size_t length, MidiMessageHandler handler, void *ctx);

  /// Number of data bytes that follow a status byte other than SysEx.
  static uint8_t expected_data_bytes(uint8_t status);

 protected:
  void emit_(MessageType type, const uint8_t *data, uint16_t length, MidiMessageHandler handler, void *ctx) {
    handler(ctx, MidiMessage{data, length, type});
  }
  void emit_current_(MidiMessageHandler handler, void *ctx);
  void take_data_(const uint8_t *byte, MidiMessageHandler handler, void *ctx);
  void end_sysex_(bool complete, MidiMessageHandler handler, void *ctx);
  void sysex_push_(uint8_t byte);

  // SysEx reassembly across packets.
  uint8_t *const sysex_buffer_;
  const uint16_t sysex_capacity_;
  uint16_t sysex_length_{0};
  bool sysex_active_{false};
  bool sysex_overflow_{false};

  // Last channel-voice status byte, sticky across bytes and packets until a new
  // status byte or a system-common message clears it (MIDI running status).
  uint8_t running_status_{0};

  // Message being assembled: status byte plus up to two data bytes.
  uint8_t current_message_[3]{};
  uint8_t current_length_{0};
  uint8_t current_expected_{0};
};

// A BLE-MIDI packet starts with a header byte and a timestamp byte, both with bit 7 set. The remaining
// timestamp bits serve jitter reconstruction only and are left at zero when transmitting.
static constexpr uint8_t PACKET_HEADER = 0x80;
static constexpr uint8_t PACKET_TIMESTAMP = 0x80;
static constexpr uint8_t STATUS_SYSEX_START = 0xF0;
static constexpr uint8_t STATUS_SYSEX_END = 0xF7;

/// Frame a SysEx payload for one BLE-MIDI packet: 0xF0 and 0xF7 are added when the payload lacks them, with
/// the timestamp byte the specification requires before 0xF7. Returns the bytes written to `out`, or 0 when
/// the payload is empty, holds a byte with bit 7 set, or does not fit `out_size` once framed.
size_t frame_sysex(const uint8_t *payload, size_t length, uint8_t *out, size_t out_size);

}  // namespace esphome::ble_midi
