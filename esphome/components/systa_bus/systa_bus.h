#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <span>

namespace esphome::systa_bus {

// A frame is a start byte, the payload length, the payload and an 8-bit checksum that makes all bytes sum to
// zero. The first two bytes together are the message type, so the type also fixes the frame length.
static constexpr uint8_t START_BYTE = 0xfc;
static constexpr uint8_t FRAME_OVERHEAD = 3;  // start byte, length, checksum

// SystaSolar Aqua sensor data, 25 bytes: [0..1] type, [2..3] not decoded, big-endian int16 in 0.1 degrees at
// [4..5] TSA, [6..7] TSE, [8..9] TWU and [10..11] TW2, pump speed in percent at [12], [13..23] not decoded.
static constexpr uint16_t MESSAGE_TYPE_AQUA_SENSOR_DATA = 0xfc16;

// The bus only buffers known message types, so the buffer is sized by the longest of them
static constexpr size_t MAX_MESSAGE_SIZE = (MESSAGE_TYPE_AQUA_SENSOR_DATA & 0xff) + FRAME_OVERHEAD;

inline uint16_t get_message_type(std::span<const uint8_t> message) { return encode_uint16(message[0], message[1]); }

class SystaBusListener {
 public:
  // Called with every frame that passed the checksum; listeners pick their message type
  virtual void handle_message(std::span<const uint8_t> message) = 0;
};

class SystaBus : public uart::UARTDevice, public Component {
 public:
  void dump_config() override;
  void loop() override;

#ifdef SYSTA_BUS_LISTENER_COUNT
  void register_listener(SystaBusListener *listener) { this->listeners_.push_back(listener); }
#endif

 protected:
#ifdef SYSTA_BUS_LISTENER_COUNT
  StaticVector<SystaBusListener *, SYSTA_BUS_LISTENER_COUNT> listeners_;
#endif
  StaticVector<uint8_t, MAX_MESSAGE_SIZE> buffer_;
};

}  // namespace esphome::systa_bus
