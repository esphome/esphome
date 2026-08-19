#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "esphome/components/pzem6l24/pzem6l24.h"

namespace esphome::pzem6l24::testing {

// Sizes of the frames exchanged with the meter, mirrored from pzem6l24.cpp.
static constexpr uint8_t REGISTER_COUNT = 64;
static constexpr size_t PAYLOAD_SIZE = REGISTER_COUNT * 2;

// Mirrored from pzem6l24.cpp: consecutive failed polls before the readings are blanked.
static constexpr int FAILURES_BEFORE_BLANKING = 3;

// The request PDU update() puts on the wire: read 64 input registers from 0x0000.
static constexpr uint8_t READ_REQUEST_PDU[] = {0x04, 0x00, 0x00, 0x00, REGISTER_COUNT};
// The request PDU reset_energy_() puts on the wire: function 0x42, reserved byte, phase selector.
static constexpr uint8_t RESET_REQUEST_PDU[] = {0x42, 0x00, 0x0F};

// Builds the 128-byte register payload the meter returns, writing each quantity in the
// little-endian byte order documented in pzem6l24.cpp's register map.
class PayloadBuilder {
 public:
  PayloadBuilder &u8(size_t offset, uint8_t value) {
    this->data_[offset] = value;
    return *this;
  }
  PayloadBuilder &u16(size_t offset, uint16_t value) {
    this->data_[offset] = static_cast<uint8_t>(value & 0xFF);
    this->data_[offset + 1] = static_cast<uint8_t>(value >> 8);
    return *this;
  }
  PayloadBuilder &u32(size_t offset, uint32_t value) {
    this->u16(offset, static_cast<uint16_t>(value & 0xFFFF));
    this->u16(offset + 2, static_cast<uint16_t>(value >> 16));
    return *this;
  }
  PayloadBuilder &i32(size_t offset, int32_t value) { return this->u32(offset, static_cast<uint32_t>(value)); }

  // Wraps the payload in a read-input-registers response PDU: function code, byte count, data.
  std::vector<uint8_t> response_pdu() const {
    std::vector<uint8_t> pdu{0x04, static_cast<uint8_t>(PAYLOAD_SIZE)};
    pdu.insert(pdu.end(), this->data_.begin(), this->data_.end());
    return pdu;
  }

 protected:
  std::array<uint8_t, PAYLOAD_SIZE> data_{};
};

}  // namespace esphome::pzem6l24::testing
