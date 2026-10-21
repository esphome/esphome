#include "midi_parser.h"

#include <cstring>

namespace esphome::ble_midi {

static MessageType type_for_status(uint8_t status) {
  if (status >= 0xF8)
    return MessageType::SYSTEM_REAL_TIME;
  if (status >= 0xF1 && status <= 0xF6)
    return MessageType::SYSTEM_COMMON;
  switch (status & 0xF0) {
    case 0x80:
      return MessageType::NOTE_OFF;
    case 0x90:
      return MessageType::NOTE_ON;
    case 0xA0:
      return MessageType::POLY_AFTERTOUCH;
    case 0xB0:
      return MessageType::CONTROL_CHANGE;
    case 0xC0:
      return MessageType::PROGRAM_CHANGE;
    case 0xD0:
      return MessageType::CHANNEL_AFTERTOUCH;
    case 0xE0:
      return MessageType::PITCH_BEND;
    default:
      return MessageType::UNKNOWN;
  }
}

// Data bytes that follow a channel-voice or system-common status byte
uint8_t MidiParser::expected_data_bytes(uint8_t status) {
  if (status >= 0xF0) {
    switch (status) {
      case 0xF1:  // MIDI Time Code Quarter Frame
      case 0xF3:  // Song Select
        return 1;
      case 0xF2:  // Song Position Pointer
        return 2;
      default:  // 0xF4, 0xF5 undefined, 0xF6 Tune Request
        return 0;
    }
  }
  switch (status & 0xF0) {
    case 0xC0:  // Program Change
    case 0xD0:  // Channel Aftertouch
      return 1;
    default:
      return 2;
  }
}

void MidiParser::reset() {
  this->running_status_ = 0;
  this->sysex_active_ = false;
  this->sysex_length_ = 0;
  this->current_length_ = 0;
}

void MidiParser::sysex_push_(uint8_t byte) {
  if (this->sysex_length_ < this->sysex_capacity_) {
    this->sysex_buffer_[this->sysex_length_++] = byte;
  } else {
    this->sysex_overflow_ = true;
  }
}

// Ends the SysEx being assembled; it is complete when 0xF7 closed it and every byte fit the buffer
void MidiParser::end_sysex_(bool complete, MidiMessageHandler handler, void *ctx) {
  this->sysex_active_ = false;
  this->running_status_ = 0;
  this->emit_(complete && !this->sysex_overflow_ ? MessageType::SYSEX : MessageType::SYSEX_INCOMPLETE,
              this->sysex_buffer_, this->sysex_length_, handler, ctx);
}

void MidiParser::emit_current_(MidiMessageHandler handler, void *ctx) {
  const uint8_t status = this->current_message_[0];
  MessageType type = type_for_status(status);
  // A Note On with velocity zero is canonically a Note Off.
  if (type == MessageType::NOTE_ON && this->current_message_[2] == 0)
    type = MessageType::NOTE_OFF;
  // System Common clears running status; channel-voice messages set it.
  this->running_status_ = status >= 0xF0 ? 0 : status;
  const uint16_t length = this->current_length_;
  this->current_length_ = 0;
  this->emit_(type, this->current_message_, length, handler, ctx);
}

// Takes a data byte for the message being assembled, or starts one from the running status; true once a message
// is complete, so the next byte with bit 7 set is a timestamp
bool MidiParser::take_data_(const uint8_t *byte, MidiMessageHandler handler, void *ctx) {
  if (this->current_length_ == 0) {
    if (this->running_status_ == 0) {
      // Data byte without any status context.
      this->emit_(MessageType::UNKNOWN, byte, 1, handler, ctx);
      return true;
    }
    this->current_message_[0] = this->running_status_;
    this->current_length_ = 1;
    this->current_expected_ = expected_data_bytes(this->running_status_);
  }
  this->current_message_[this->current_length_++] = *byte;
  if (this->current_length_ <= this->current_expected_)
    return false;
  this->emit_current_(handler, ctx);
  return true;
}

void MidiParser::feed(const uint8_t *data, size_t length, MidiMessageHandler handler, void *ctx) {
  // A BLE-MIDI packet is at least a header byte plus one timestamp or SysEx
  // continuation byte, and the header byte always has bit 7 set. Anything else
  // is malformed.
  if (length < 2 || (data[0] & 0x80) == 0) {
    for (size_t i = 0; i < length; i++)
      this->emit_(MessageType::UNKNOWN, data + i, 1, handler, ctx);
    return;
  }

  // A timestamp byte precedes every status byte, so after a complete message or a SysEx data byte the next byte
  // with bit 7 set is a timestamp, and the one after it is a status byte
  bool expect_timestamp = true;
  for (size_t i = 1; i < length; i++) {
    const uint8_t byte = data[i];
    if ((byte & 0x80) == 0) {
      // A data byte continues the SysEx, the message being assembled, or the running status
      if (this->sysex_active_) {
        this->sysex_push_(byte);
        expect_timestamp = true;
      } else {
        expect_timestamp = this->take_data_(data + i, handler, ctx);
      }
      continue;
    }
    if (expect_timestamp) {
      expect_timestamp = false;
      continue;
    }
    expect_timestamp = true;
    if (byte >= 0xF8) {
      // Real-time messages are single bytes that interrupt anything, a SysEx included, without affecting it
      this->emit_(MessageType::SYSTEM_REAL_TIME, data + i, 1, handler, ctx);
      continue;
    }
    if (byte == STATUS_SYSEX_END) {
      if (this->sysex_active_) {
        this->sysex_push_(STATUS_SYSEX_END);
        this->end_sysex_(true, handler, ctx);
      } else {
        this->emit_(MessageType::UNKNOWN, data + i, 1, handler, ctx);
      }
      continue;
    }
    // Any other status byte ends a SysEx still open and discards a message still being assembled
    if (this->sysex_active_)
      this->end_sysex_(false, handler, ctx);
    if (byte == STATUS_SYSEX_START) {
      this->sysex_active_ = true;
      this->sysex_length_ = 0;
      this->sysex_overflow_ = false;
      this->sysex_push_(STATUS_SYSEX_START);
      continue;
    }
    this->current_message_[0] = byte;
    this->current_length_ = 1;
    this->current_expected_ = expected_data_bytes(byte);
    // A status byte without data bytes, e.g. Tune Request, is a message on its own
    if (this->current_expected_ == 0) {
      this->emit_current_(handler, ctx);
    } else {
      expect_timestamp = false;
    }
  }
}

size_t frame_sysex(const uint8_t *payload, size_t length, uint8_t *out, size_t out_size) {
  if (length == 0)
    return 0;
  const size_t skip_start = payload[0] == STATUS_SYSEX_START ? 1 : 0;
  const size_t skip_end = payload[length - 1] == STATUS_SYSEX_END ? 1 : 0;
  const size_t body = length - skip_start - skip_end;
  // 0xF0, the body, then the timestamp before 0xF7
  const size_t framed = body + 3;
  if (framed > out_size)
    return 0;
  out[0] = STATUS_SYSEX_START;
  std::memcpy(out + 1, payload + skip_start, body);
  out[body + 1] = PACKET_TIMESTAMP;
  out[body + 2] = STATUS_SYSEX_END;
  return framed;
}

}  // namespace esphome::ble_midi
