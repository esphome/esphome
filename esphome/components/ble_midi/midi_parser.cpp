#include "midi_parser.h"

#include <cstring>

namespace esphome::ble_midi {

const char *message_type_name(MessageType type) {
  switch (type) {
    case MessageType::NOTE_OFF:
      return "NOTE_OFF";
    case MessageType::NOTE_ON:
      return "NOTE_ON";
    case MessageType::POLY_AFTERTOUCH:
      return "POLY_AFTERTOUCH";
    case MessageType::CONTROL_CHANGE:
      return "CONTROL_CHANGE";
    case MessageType::PROGRAM_CHANGE:
      return "PROGRAM_CHANGE";
    case MessageType::CHANNEL_AFTERTOUCH:
      return "CHANNEL_AFTERTOUCH";
    case MessageType::PITCH_BEND:
      return "PITCH_BEND";
    case MessageType::SYSEX:
      return "SYSEX";
    case MessageType::SYSTEM_COMMON:
      return "SYSTEM_COMMON";
    case MessageType::SYSTEM_REAL_TIME:
      return "SYSTEM_REAL_TIME";
    case MessageType::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

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

void MidiParser::reset() {
  this->running_status_ = 0;
  this->sysex_active_ = false;
  this->sysex_length_ = 0;
  this->reset_current_();
}

void MidiParser::reset_current_() {
  this->current_message_[0] = 0;
  this->current_length_ = 0;
  this->current_expected_ = 0;
}

uint8_t MidiParser::expected_data_bytes(uint8_t status) {
  switch (status & 0xF0) {
    case 0x80:  // Note Off
    case 0x90:  // Note On
    case 0xA0:  // Polyphonic Aftertouch
    case 0xB0:  // Control Change
    case 0xE0:  // Pitch Bend
      return 2;
    case 0xC0:  // Program Change
    case 0xD0:  // Channel Aftertouch
      return 1;
    default:
      return 0;
  }
}

uint8_t MidiParser::expected_system_common_data_bytes(uint8_t status) {
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

void MidiParser::sysex_push_(uint8_t byte) {
  if (this->sysex_length_ < this->sysex_capacity_) {
    this->sysex_buffer_[this->sysex_length_++] = byte;
  } else {
    this->sysex_truncated_ = true;
  }
}

void MidiParser::emit_sysex_(const std::function<void(const MidiMessage &)> &callback) {
  MidiMessage message;
  message.type = MessageType::SYSEX;
  message.data = this->sysex_buffer_;
  message.length = this->sysex_length_;
  message.truncated = this->sysex_truncated_;
  this->sysex_active_ = false;
  this->sysex_length_ = 0;
  this->running_status_ = 0;
  callback(message);
}

void MidiParser::emit_current_(const std::function<void(const MidiMessage &)> &callback) {
  if (this->current_message_[0] == 0)
    return;
  uint8_t status = this->current_message_[0];
  MidiMessage message;
  message.type = type_for_status(status);
  // A Note On with velocity zero is canonically a Note Off.
  if (message.type == MessageType::NOTE_ON && this->current_length_ >= 3 && this->current_message_[2] == 0)
    message.type = MessageType::NOTE_OFF;
  message.data = this->current_message_;
  message.length = this->current_length_;
  // System Common clears running status; channel-voice messages set it.
  this->running_status_ = (status >= 0xF0 && status <= 0xF7) ? 0 : status;
  this->current_length_ = 0;
  this->current_expected_ = 0;
  callback(message);
  // The message bytes must stay valid for the duration of the callback, so the
  // status byte is cleared afterwards.
  this->current_message_[0] = 0;
}

void MidiParser::emit_single_(MessageType type, uint8_t byte,
                              const std::function<void(const MidiMessage &)> &callback) {
  this->single_byte_ = byte;
  MidiMessage message;
  message.type = type;
  message.data = &this->single_byte_;
  message.length = 1;
  callback(message);
}

void MidiParser::feed(const uint8_t *data, size_t length, const std::function<void(const MidiMessage &)> &callback) {
  // A BLE-MIDI packet is at least a header byte plus one timestamp or SysEx
  // continuation byte, and the header byte always has bit 7 set. Anything else
  // is malformed.
  if (length < 2 || (data[0] & 0x80) == 0) {
    for (size_t i = 0; i < length; i++)
      this->emit_single_(MessageType::UNKNOWN, data[i], callback);
    return;
  }

  size_t i = 1;
  bool expect_timestamp = true;

  while (i < length) {
    uint8_t byte = data[i];

    if (expect_timestamp) {
      // Either a timestamp byte introducing a new status byte, or a data byte
      // continuing a SysEx or running status.
      if ((byte & 0x80) == 0) {
        if (this->sysex_active_) {
          this->sysex_push_(byte);
          i++;
          continue;
        }
        // Running-status data byte: reprocess it below.
        expect_timestamp = false;
        continue;
      }
      i++;
      expect_timestamp = false;
      continue;
    }

    if ((byte & 0x80) == 0) {
      // Data byte belonging to the message being assembled, or to a
      // running-status message.
      if (this->current_message_[0] == 0) {
        if (this->running_status_ == 0) {
          // Data byte without any status context.
          this->emit_single_(MessageType::UNKNOWN, byte, callback);
          i++;
          continue;
        }
        this->current_message_[0] = this->running_status_;
        this->current_length_ = 1;
        this->current_expected_ = expected_data_bytes(this->running_status_);
      }
      if (this->current_length_ < sizeof(this->current_message_))
        this->current_message_[this->current_length_++] = byte;
      if (this->current_length_ >= this->current_expected_ + 1u) {
        this->emit_current_(callback);
        expect_timestamp = true;
      }
      i++;
      continue;
    }

    // Status byte.
    if (byte == 0xF0) {
      // Start of SysEx; the rest of the packet is SysEx data until 0xF7 or the
      // end of the packet.
      this->sysex_active_ = true;
      this->sysex_length_ = 0;
      this->sysex_truncated_ = false;
      this->sysex_push_(0xF0);
      i++;
      // Set when a status byte other than 0xF7 cut the SysEx short; that byte is then still unprocessed
      bool ended_early = false;
      while (i < length) {
        uint8_t sysex_byte = data[i];
        if ((sysex_byte & 0x80) == 0) {
          this->sysex_push_(sysex_byte);
          i++;
          continue;
        }
        // A byte with bit 7 set inside SysEx is a timestamp; what follows
        // decides its meaning.
        if (i + 1 >= length) {
          // Dangling timestamp at the end of the packet.
          i++;
          break;
        }
        uint8_t next = data[i + 1];
        if (next == 0xF7) {
          this->sysex_push_(0xF7);
          this->emit_sysex_(callback);
          i += 2;
          break;
        }
        if (next >= 0xF8) {
          // Real-time message interrupting the SysEx; SysEx resumes afterwards.
          this->emit_single_(MessageType::SYSTEM_REAL_TIME, next, callback);
          i += 2;
          continue;
        }
        // Any other status byte ends the SysEx without an end-of-exclusive
        // byte: malformed, but recoverable. Skip the timestamp and let the
        // outer loop process the status byte.
        this->emit_sysex_(callback);
        i++;
        ended_early = true;
        break;
      }
      expect_timestamp = !ended_early;
      continue;
    }

    if (byte == 0xF7) {
      // End of SysEx without a preceding timestamp byte.
      if (this->sysex_active_) {
        this->sysex_push_(0xF7);
        this->emit_sysex_(callback);
      } else {
        this->emit_single_(MessageType::UNKNOWN, byte, callback);
      }
      i++;
      expect_timestamp = true;
      continue;
    }

    if (byte >= 0xF8) {
      // Real-time message: a single byte that does not affect running status.
      this->emit_single_(MessageType::SYSTEM_REAL_TIME, byte, callback);
      i++;
      expect_timestamp = true;
      continue;
    }

    // Any other status byte starts a new message and discards an incomplete
    // one. A SysEx carried over from the previous packet ends here as well.
    if (this->sysex_active_)
      this->emit_sysex_(callback);
    this->reset_current_();
    this->current_message_[0] = byte;
    this->current_length_ = 1;
    this->current_expected_ =
        (byte >= 0xF1 && byte <= 0xF6) ? expected_system_common_data_bytes(byte) : expected_data_bytes(byte);
    if (this->current_expected_ == 0) {
      // The status byte is a complete message on its own, e.g. Tune Request.
      this->emit_current_(callback);
      expect_timestamp = true;
    } else {
      expect_timestamp = false;
    }
    i++;
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
