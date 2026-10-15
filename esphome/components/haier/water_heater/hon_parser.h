#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"

namespace esphome::haier::water_heater_protocol {

// A logical frame, after the FF FF separator and the FF 55 escapes are removed:
// [length][flags][5 reserved bytes][type][payload...][checksum][crc high][crc low]
// The length counts every byte before the checksum; the CRC is only present when its flag is set.
static constexpr uint8_t FRAME_FLAGS_OFFSET = 1;
static constexpr uint8_t FRAME_TYPE_OFFSET = 7;
static constexpr uint8_t FRAME_PAYLOAD_OFFSET = 8;
static constexpr uint8_t FRAME_FLAG_CRC = 0x40;
static constexpr uint8_t FRAME_FLAG_ENCRYPTED = 0x80;
static constexpr uint8_t MIN_FRAME_LENGTH = 8;
static constexpr uint8_t MAX_FRAME_LENGTH = 0xF1;
// The longest frame that is decoded is the 61-byte status report. Longer frames are still checked
// in full, but only their head is kept.
static constexpr size_t MAX_STORED_FRAME = 64;
static constexpr size_t MAX_PAYLOAD = 4;
static constexpr size_t MAX_WIRE_FRAME = 28;  // 2 separators, up to 12 escaped bytes, an escaped checksum

enum class FrameType : uint8_t {
  FRAME_TYPE_CONTROL = 0x01,
  FRAME_TYPE_STATUS = 0x02,
  FRAME_TYPE_INVALID = 0x03,  // the appliance rejected the last request
  FRAME_TYPE_CONFIRM = 0x05,
  FRAME_TYPE_REPORT = 0x06,  // an unsolicited status, which expects a CONFIRM back
  FRAME_TYPE_GET_VERSION = 0x61,
  FRAME_TYPE_VERSION = 0x62,
  FRAME_TYPE_GET_DEVICE_ID = 0x70,
  FRAME_TYPE_DEVICE_ID = 0x71,
  FRAME_TYPE_GET_NETWORK_STATUS = 0xF0,
  FRAME_TYPE_NETWORK_STATUS = 0xF7,
};

// Bounded receive parser. The checksum and CRC are kept running while the bytes arrive, so a frame
// longer than the stored head is validated without being stored.
class FrameParser {
 public:
  void reset() {
    this->size_ = 0;
    this->separators_ = 0;
    this->escape_ = false;
    this->complete_ = false;
    this->checksum_ = 0;
    this->crc_ = 0;
  }
  bool complete() const { return this->complete_; }
  /// The stored head of the frame, at most MAX_STORED_FRAME bytes; valid until the next feed or reset.
  const uint8_t *data() const { return this->buffer_.data(); }
  /// Logical bytes received, including the checksum and CRC; may exceed MAX_STORED_FRAME.
  size_t size() const { return this->size_; }
  size_t stored_size() const { return std::min<size_t>(this->size_, MAX_STORED_FRAME); }

  /// Returns true once a valid frame is complete. A complete but invalid frame leaves complete() set.
  bool feed(uint8_t value) {
    if (this->complete_) {
      this->reset();
    }
    if (this->separators_ < 2) {
      this->separators_ = value == 0xFF ? this->separators_ + 1 : 0;
      return false;
    }
    if (this->escape_) {
      this->escape_ = false;
      if (value == 0xFF) {
        // A new unescaped separator pair: start over without losing synchronisation.
        this->size_ = 0;
        this->checksum_ = 0;
        this->crc_ = 0;
        return false;
      }
      if (value != 0x55) {
        this->reset();
        return false;
      }
      value = 0xFF;
    } else if (value == 0xFF) {
      this->escape_ = true;
      return false;
    }
    if (this->size_ == 0 && (value < MIN_FRAME_LENGTH || value > MAX_FRAME_LENGTH)) {
      this->reset();
      return false;
    }
    const uint8_t index = this->size_++;
    if (index < MAX_STORED_FRAME) {
      this->buffer_[index] = value;
    }
    const uint8_t length = this->buffer_[0];
    if (index < length) {
      this->checksum_ += value;
      if (value == 0xFF) {
        this->checksum_ += 0x55;  // The escape byte counts towards the checksum.
      }
      this->crc_ = crc16(&value, 1, this->crc_, 0xA001, false, false);
    } else if (index == length) {
      this->received_checksum_ = value;
    } else if (index == length + 1) {
      this->received_crc_ = static_cast<uint16_t>(value) << 8;
    } else {
      this->received_crc_ |= value;
    }
    if (this->size_ < 2) {
      return false;
    }
    const bool crc = (this->buffer_[FRAME_FLAGS_OFFSET] & FRAME_FLAG_CRC) != 0;
    if (this->size_ != length + 1 + (crc ? 2 : 0)) {
      return false;
    }
    this->complete_ = true;
    if ((this->buffer_[FRAME_FLAGS_OFFSET] & ~FRAME_FLAG_CRC) != 0 || this->checksum_ != this->received_checksum_) {
      return false;
    }
    return !crc || this->crc_ == this->received_crc_;
  }

 protected:
  std::array<uint8_t, MAX_STORED_FRAME> buffer_{};
  uint16_t crc_{0};
  uint16_t received_crc_{0};
  uint8_t size_{0};
  uint8_t separators_{0};
  uint8_t checksum_{0};
  uint8_t received_checksum_{0};
  bool escape_{false};
  bool complete_{false};
};

/// Writes a clear-text frame with up to MAX_PAYLOAD bytes of payload into wire, which must hold
/// MAX_WIRE_FRAME bytes. Returns the number of bytes written, or 0 when the payload is too long.
inline size_t encode_frame(FrameType type, const uint8_t *payload, size_t size, uint8_t *wire) {
  if (size > MAX_PAYLOAD) {
    return 0;
  }
  uint8_t core[FRAME_PAYLOAD_OFFSET + MAX_PAYLOAD] = {static_cast<uint8_t>(FRAME_PAYLOAD_OFFSET + size)};
  core[FRAME_TYPE_OFFSET] = static_cast<uint8_t>(type);
  for (size_t i = 0; i < size; i++) {
    core[FRAME_PAYLOAD_OFFSET + i] = payload[i];
  }
  size_t count = 0;
  wire[count++] = 0xFF;
  wire[count++] = 0xFF;
  uint8_t checksum = 0;
  for (size_t i = 0; i < FRAME_PAYLOAD_OFFSET + size; i++) {
    wire[count++] = core[i];
    checksum += core[i];
    if (core[i] == 0xFF) {
      wire[count++] = 0x55;
      checksum += 0x55;
    }
  }
  wire[count++] = checksum;
  if (checksum == 0xFF) {
    wire[count++] = 0x55;
  }
  return count;
}

enum class HonMode : uint8_t { HON_MODE_UNKNOWN, HON_MODE_NORMAL, HON_MODE_ECO, HON_MODE_BPS };

// The appliance reports and takes target temperatures with this offset.
static constexpr uint8_t TARGET_TEMPERATURE_OFFSET = 30;
static constexpr uint8_t MAX_REPORTED_TEMPERATURE = 100;

struct Status {
  uint8_t current_temperature;
  uint8_t target_temperature;
  uint8_t secondary_temperature;
  uint8_t estimated_heating_time;
  uint8_t available_water;  // 0 to 3 segments
  bool powered;
  bool heating;
  HonMode mode;
};

// The mode is spread over three flag bytes; exactly one of the three indications is set in a known mode.
inline HonMode decode_mode(const uint8_t *payload) {
  const bool normal = (payload[38] & 0x10) && (payload[39] & 0x10);
  const bool eco = (payload[41] & 0x04) != 0;
  const bool bps = (payload[40] & 0x04) != 0;
  if (unsigned(normal) + unsigned(eco) + unsigned(bps) != 1) {
    return HonMode::HON_MODE_UNKNOWN;
  }
  if (normal) {
    return HonMode::HON_MODE_NORMAL;
  }
  return eco ? HonMode::HON_MODE_ECO : HonMode::HON_MODE_BPS;
}

static constexpr uint8_t STATUS_FRAME_LENGTH = 60;

/// Decodes a status report or a polled status; false for any other frame or an implausible one.
inline bool decode_status(const uint8_t *frame, size_t size, Status &status) {
  if (size <= STATUS_FRAME_LENGTH || frame[0] != STATUS_FRAME_LENGTH) {
    return false;
  }
  const auto type = static_cast<FrameType>(frame[FRAME_TYPE_OFFSET]);
  if (type != FrameType::FRAME_TYPE_STATUS && type != FrameType::FRAME_TYPE_REPORT) {
    return false;
  }
  const uint8_t *payload = frame + FRAME_PAYLOAD_OFFSET;
  if (payload[0] != 0x6D || payload[1] != 0x01) {
    return false;
  }
  status.current_temperature = payload[2];
  status.target_temperature = payload[3] + TARGET_TEMPERATURE_OFFSET;
  status.estimated_heating_time = payload[7];
  status.secondary_temperature = payload[15];
  status.powered = (payload[39] & 0x01) != 0;
  status.heating = (payload[41] & 0x01) != 0;
  status.mode = decode_mode(payload);
  status.available_water = payload[45];
  return payload[2] <= MAX_REPORTED_TEMPERATURE && payload[3] <= MAX_REPORTED_TEMPERATURE - TARGET_TEMPERATURE_OFFSET;
}

// A control write: one parameter of the 0x5D group and its value.
enum class Parameter : uint8_t {
  PARAMETER_POWER = 0x01,
  PARAMETER_TARGET_TEMPERATURE = 0x02,  // value is the temperature minus TARGET_TEMPERATURE_OFFSET
  PARAMETER_ECO = 0x05,                 // 1 selects ECO, 0 selects Normal
  PARAMETER_BPS = 0x17,
};
static constexpr uint8_t CONTROL_GROUP = 0x5D;

struct Command {
  Parameter parameter;
  uint8_t value;
};

/// Fills the four-byte control payload for a command.
inline void encode_control(const Command &command, uint8_t *payload) {
  payload[0] = CONTROL_GROUP;
  payload[1] = static_cast<uint8_t>(command.parameter);
  payload[2] = 0x00;
  payload[3] = command.value;
}

/// True when a status shows that the command took effect.
inline bool command_confirmed(const Command &command, const Status &status) {
  switch (command.parameter) {
    case Parameter::PARAMETER_POWER:
      return status.powered == (command.value != 0);
    case Parameter::PARAMETER_TARGET_TEMPERATURE:
      return status.target_temperature == command.value + TARGET_TEMPERATURE_OFFSET;
    case Parameter::PARAMETER_ECO:
      return status.mode == (command.value != 0 ? HonMode::HON_MODE_ECO : HonMode::HON_MODE_NORMAL);
    case Parameter::PARAMETER_BPS:
      return status.mode == HonMode::HON_MODE_BPS;
  }
  return false;
}

}  // namespace esphome::haier::water_heater_protocol
