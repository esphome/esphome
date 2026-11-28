#pragma once

#include "esphome/components/geolocation/geolocation.h"
#include <cstddef>
#include <cstdint>

/// u-blox UBX protocol: frames and the NAV messages this platform reads.
namespace esphome::ubx {

static constexpr uint8_t UBX_SYNC_1 = 0xB5;
static constexpr uint8_t UBX_SYNC_2 = 0x62;
// sync (2), class, id, length (2) and checksum (2)
static constexpr size_t UBX_FRAME_OVERHEAD = 8;
// Payload bytes kept per frame: every NAV message read fits, and MON-VER keeps its first extensions
static constexpr uint16_t UBX_MAX_PAYLOAD = 256;
// A longer length is taken as a false sync; the longest messages (NAV-SAT, MON-VER) stay well under it
static constexpr uint16_t UBX_MAX_FRAME_PAYLOAD = 2048;

static constexpr uint8_t UBX_CLASS_NAV = 0x01;
static constexpr uint8_t UBX_CLASS_ACK = 0x05;
static constexpr uint8_t UBX_CLASS_CFG = 0x06;
static constexpr uint8_t UBX_CLASS_MON = 0x0A;

static constexpr uint8_t UBX_NAV_POSLLH = 0x02;
static constexpr uint8_t UBX_NAV_DOP = 0x04;
static constexpr uint8_t UBX_NAV_SOL = 0x06;
static constexpr uint8_t UBX_NAV_PVT = 0x07;
static constexpr uint8_t UBX_NAV_VELNED = 0x12;
static constexpr uint8_t UBX_NAV_TIMEUTC = 0x21;
static constexpr uint8_t UBX_ACK_NAK = 0x00;
static constexpr uint8_t UBX_ACK_ACK = 0x01;
static constexpr uint8_t UBX_CFG_PRT = 0x00;
static constexpr uint8_t UBX_CFG_MSG = 0x01;
static constexpr uint8_t UBX_CFG_RATE = 0x08;
static constexpr uint8_t UBX_CFG_NAV5 = 0x24;
static constexpr uint8_t UBX_MON_VER = 0x04;

/// NAV-PVT exists from protocol 14.00 (u-blox 7); older receivers report the same in separate messages.
static constexpr uint16_t UBX_PROTVER_NAV_PVT = 1400;

/// Write a frame with `len` bytes of `payload` to `out`, which holds at least len + UBX_FRAME_OVERHEAD bytes.
/// Returns the frame length.
size_t build_frame(uint8_t msg_class, uint8_t msg_id, const uint8_t *payload, uint16_t len, uint8_t *out);

/// Collects frames from a byte stream, skipping anything between them (NMEA before it is switched off).
/// Frames longer than UBX_MAX_PAYLOAD are checked in full and keep their first UBX_MAX_PAYLOAD bytes.
class FrameParser {
 public:
  /// True when `byte` completes a frame with a valid checksum.
  bool feed(uint8_t byte);
  uint8_t msg_class() const { return this->header_[0]; }
  uint8_t msg_id() const { return this->header_[1]; }
  /// Payload bytes kept, at most UBX_MAX_PAYLOAD; the frame itself may be longer.
  uint16_t length() const { return this->length_; }
  const uint8_t *payload() const { return this->payload_; }

 protected:
  uint8_t payload_[UBX_MAX_PAYLOAD];
  uint8_t header_[4];  // class, id, length
  uint16_t frame_length_{0};
  uint16_t length_{0};
  uint16_t pos_{0};
  uint8_t ck_a_{0};
  uint8_t ck_b_{0};
  uint8_t state_{0};
};

/// The protocol version from a MON-VER payload as 100 * major + minor (1800 for 18.00), 0 when it has none (u-blox 6).
uint16_t parse_protocol_version(const uint8_t *payload, uint16_t len);

/// Decode a NAV message into `data`. Position, altitude, speed and course are written even without a fix; `fix` is
/// updated by the messages that carry the fix status (NAV-PVT, NAV-SOL). UTC is set only when the receiver marks it
/// valid, leap seconds included. Returns whether the message was one of the decoded ones and long enough.
bool decode_nav(uint8_t msg_id, const uint8_t *payload, uint16_t len, uint64_t now_ms,
                geolocation::GeolocationData &data, bool &fix);

}  // namespace esphome::ubx
