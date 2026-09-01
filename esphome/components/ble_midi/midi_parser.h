#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace esphome::ble_midi {

// Decoded MIDI message types emitted by MidiParser.
// Status bytes in MIDI have the high bit set; the low nibble encodes the
// channel for channel-voice messages (0x80-0xEF), while system messages
// (0xF0-0xFF) are channel-less.
enum class MessageType : uint8_t {
  NOTE_OFF,            // 0x80
  NOTE_ON,             // 0x90
  POLY_AFTERTOUCH,     // 0xA0
  CONTROL_CHANGE,      // 0xB0
  PROGRAM_CHANGE,      // 0xC0
  CHANNEL_AFTERTOUCH,  // 0xD0
  PITCH_BEND,          // 0xE0
  SYSEX,               // 0xF0 ... 0xF7, possibly fragmented across BLE packets
  SYSTEM_COMMON,       // 0xF1-0xF6, e.g. quarter frame, song position
  SYSTEM_REAL_TIME,    // 0xF8-0xFF, e.g. clock, start, stop
  UNKNOWN,             // malformed/undecodable byte
};

/// Short human-readable name of a message type, for logging.
const char *message_type_name(MessageType type);

/// A decoded MIDI message.
///
/// `data` points into storage owned by the MidiParser and is only valid for the
/// duration of the parser callback; consumers that need to keep the bytes must
/// copy them.
struct MidiMessage {
  MessageType type{MessageType::UNKNOWN};
  // Full message bytes: status byte followed by its data bytes. For SysEx this
  // includes the 0xF0/0xF7 wrappers.
  const uint8_t *data{nullptr};
  size_t length{0};

  // Accessors below are only meaningful for the matching message type.
  uint8_t status() const { return this->length > 0 ? this->data[0] : 0; }
  uint8_t channel() const { return this->length > 0 ? (this->data[0] & 0x0F) : 0; }

  // NOTE_ON / NOTE_OFF / POLY_AFTERTOUCH
  uint8_t note() const { return this->length > 1 ? this->data[1] : 0; }
  uint8_t velocity() const { return this->length > 2 ? this->data[2] : 0; }

  // CONTROL_CHANGE
  uint8_t control_number() const { return this->length > 1 ? this->data[1] : 0; }
  uint8_t control_value() const { return this->length > 2 ? this->data[2] : 0; }

  // PROGRAM_CHANGE / CHANNEL_AFTERTOUCH
  uint8_t program() const { return this->length > 1 ? this->data[1] : 0; }

  // PITCH_BEND: 14-bit value re-centered on zero (-8192..8191).
  int16_t pitch_bend() const {
    if (this->length < 3)
      return 0;
    uint16_t raw = (static_cast<uint16_t>(this->data[2]) << 7) | this->data[1];
    return static_cast<int16_t>(raw) - 8192;
  }
};

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
/// A timestamp byte precedes every new status byte, except where running status
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
  /// Allocate the buffer that reassembles SysEx messages. Must be called once
  /// before feed(); SysEx payloads longer than `size` bytes are truncated.
  void init_sysex_buffer(size_t size);

  /// Drop all parser state. Call on disconnect so running status and partial
  /// messages do not leak into the next session.
  void reset();

  /// Feed one BLE notification payload, invoking `callback` once per decoded
  /// message. Undecodable bytes are reported as MessageType::UNKNOWN so the
  /// caller can log them.
  void feed(const uint8_t *data, size_t length, const std::function<void(const MidiMessage &)> &callback);

 protected:
  // Number of data bytes that follow a channel-voice status byte.
  static uint8_t expected_data_bytes(uint8_t status);
  // Number of data bytes that follow a system-common status byte.
  static uint8_t expected_system_common_data_bytes(uint8_t status);

  void emit_current_(const std::function<void(const MidiMessage &)> &callback);
  void emit_single_(MessageType type, uint8_t byte, const std::function<void(const MidiMessage &)> &callback);
  void emit_sysex_(const std::function<void(const MidiMessage &)> &callback);
  void sysex_push_(uint8_t byte);
  void reset_current_();

  // Last channel-voice status byte, sticky across bytes and packets until a new
  // status byte or a system-common message clears it (MIDI running status).
  uint8_t running_status_{0};

  // SysEx reassembly across packets.
  std::unique_ptr<uint8_t[]> sysex_buffer_;
  size_t sysex_capacity_{0};
  size_t sysex_length_{0};
  bool sysex_active_{false};

  // Message currently being assembled: status byte plus up to two data bytes.
  uint8_t current_message_[3]{};
  uint8_t current_length_{0};
  uint8_t current_expected_{0};

  // Scratch storage for single-byte messages (real-time, undecodable bytes).
  uint8_t single_byte_{0};
};

}  // namespace esphome::ble_midi
