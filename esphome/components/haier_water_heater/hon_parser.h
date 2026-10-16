#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace esphome::haier_water_heater::water_heater_protocol {

// Bounded receive parser for water-heater frames.
class FrameParser {
 public:
  void reset() {
    this->size_ = 0;
    this->separators_ = 0;
    this->escape_ = false;
    this->complete_ = false;
  }
  bool complete() const { return this->complete_; }
  const uint8_t *data() const { return this->buffer_.data(); }
  size_t size() const { return this->size_; }
  // The returned logical frame remains available until the next feed/reset.
  bool feed(uint8_t value) {
    if (this->complete_) {
      this->reset();
      this->complete_ = false;
    }
    if (this->separators_ < 2) {
      this->separators_ = value == 0xFF ? this->separators_ + 1 : 0;
      return false;
    }
    if (this->escape_) {
      this->escape_ = false;
      if (value == 0xFF) {
        this->size_ = 0;  // A new unescaped separator pair.
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
    if (this->size_ == 0 && (value < 8 || value > 0xF1)) {
      this->reset();
      return false;
    }
    this->buffer_[this->size_++] = value;
    if (this->size_ < 2)
      return false;
    const size_t length = this->buffer_[0];
    const bool crc = (this->buffer_[1] & 0x40) != 0;
    if (this->size_ != length + 1 + (crc ? 2 : 0))
      return false;
    this->complete_ = true;
    if ((this->buffer_[1] & ~0x40) != 0)
      return false;
    uint8_t checksum = 0;
    uint16_t calculated_crc = 0;
    for (size_t i = 0; i < length; i++) {
      const uint8_t byte = this->buffer_[i];
      checksum += byte;
      if (byte == 0xFF)
        checksum += 0x55;
      calculated_crc ^= byte;
      for (uint8_t bit = 0; bit < 8; bit++)
        calculated_crc = (calculated_crc >> 1) ^ ((calculated_crc & 1) ? 0xA001 : 0);
    }
    if (checksum != this->buffer_[length])
      return false;
    return !crc || calculated_crc == ((uint16_t(this->buffer_[length + 1]) << 8) | this->buffer_[length + 2]);
  }

 protected:
  std::array<uint8_t, 244> buffer_{};
  size_t size_{0};
  uint8_t separators_{0};
  bool escape_{false};
  bool complete_{false};
};

// Clear-text requests, including four-byte captured control payloads. Wire buffer must hold 28 bytes.
inline size_t encode_frame(uint8_t type, const uint8_t *payload, size_t size, uint8_t *wire) {
  if (size > 4)
    return 0;
  uint8_t core[12] = {uint8_t(8 + size), 0, 0, 0, 0, 0, 0, type, 0, 0};
  for (size_t i = 0; i < size; i++)
    core[8 + i] = payload[i];
  size_t count = 0;
  wire[count++] = 0xFF;
  wire[count++] = 0xFF;
  uint8_t checksum = 0;
  for (size_t i = 0; i < 8 + size; i++) {
    wire[count++] = core[i];
    checksum += core[i];
    if (core[i] == 0xFF) {
      wire[count++] = 0x55;
      checksum += 0x55;
    }
  }
  wire[count++] = checksum;
  if (checksum == 0xFF)
    wire[count++] = 0x55;
  return count;
}

enum class HonMode : uint8_t { HON_MODE_UNKNOWN, HON_MODE_NORMAL, HON_MODE_ECO, HON_MODE_BPS };

struct Status {
  uint8_t current_temperature;
  uint8_t target_temperature;
  uint8_t secondary_temperature;
  uint8_t estimated_heating_time;
  bool powered;
  bool heating;
  HonMode mode;
  uint8_t available_water;
};

inline bool decode_status(const uint8_t *frame, size_t size, Status &status) {
  if (size < 61 || frame[0] != 60 || (frame[7] != 0x02 && frame[7] != 0x06))
    return false;
  const uint8_t *payload = frame + 8;
  if (payload[0] != 0x6D || payload[1] != 0x01)
    return false;
  status.current_temperature = payload[2];
  status.secondary_temperature = payload[15];
  status.estimated_heating_time = payload[7];
  status.target_temperature = unsigned(payload[3]) + 30;
  status.powered = (payload[39] & 0x01) != 0;
  status.heating = (payload[41] & 0x01) != 0;
  const bool normal = (payload[38] & 0x10) && (payload[39] & 0x10);
  const bool eco = (payload[41] & 0x04) != 0;
  const bool bps = (payload[40] & 0x04) != 0;
  status.mode = (unsigned(normal) + unsigned(eco) + unsigned(bps) != 1) ? HonMode::HON_MODE_UNKNOWN
                : normal                                                ? HonMode::HON_MODE_NORMAL
                : eco                                                   ? HonMode::HON_MODE_ECO
                                                                        : HonMode::HON_MODE_BPS;
  status.available_water = payload[45];
  return payload[2] <= 100 && payload[3] <= 70;
}

}  // namespace esphome::haier_water_heater::water_heater_protocol
