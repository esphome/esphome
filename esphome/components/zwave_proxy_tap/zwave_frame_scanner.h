#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::zwave_proxy_tap {

static constexpr uint8_t ZWAVE_SOF_BYTE = 0x01;
static constexpr uint8_t ZWAVE_ACK_BYTE = 0x06;

// TYPE, COMMAND ID, CHECKSUM
static constexpr uint8_t ZWAVE_MIN_FRAME_LENGTH = 3;
static constexpr uint32_t ZWAVE_FRAME_TIMEOUT_MS = 1500;

// A frame is SOF, LENGTH, then LENGTH more bytes, even when LENGTH is too short to be valid
class ZWaveFrameScanner {
 public:
  // Returns true when the byte completes a valid data frame. `now` is in ms.
  bool feed(uint8_t byte, uint32_t now);
  void reset() { this->state_ = State::WAIT_SOF; }

 protected:
  enum class State : uint8_t {
    WAIT_SOF,
    WAIT_LENGTH,
    WAIT_BODY,
  };

  uint32_t frame_start_{0};
  uint8_t length_{0};
  uint8_t remaining_{0};
  uint8_t checksum_{0};  // XOR of 0xFF, LENGTH and every byte after it; zero when the frame is valid
  State state_{State::WAIT_SOF};
};

}  // namespace esphome::zwave_proxy_tap
