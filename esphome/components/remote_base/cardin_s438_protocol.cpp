#include "cardin_s438_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.cardin_s438");

namespace {

/*
 * Cardin S438 wire format
 *
 * Frame: 6400 us sync mark + 40 data bits (5 bytes, LSB-first).
 *   0 = 1460 us space, 750 us mark
 *   1 =  750 us space, 1460 us mark
 *
 * Bytes:
 *   [0..2] Encoded pin fields and seed
 *   [2]    (seed + marker 0x87), modulo 256
 *          Carries propagate into bytes 1 and 0.
 *   [3..4] Sum of bytes [0..2], little-endian
 *
 * Pin code: 14 bits, MSB-first, mapped to X/Y fields:
 *   Y7,Y6,X7,X6,Y5,X5,Y4,X4,Y3,X3,Y2,Y1,Y0,X2
 *   Channels in X bits 1:0: A=10, B=00, C=11, D=01.
 *
 * Three frames per transmission, with 63650 us LOW gaps.
 * Seed chosen randomly; no pattern found in captures.
 */

// Frame format and timing, in microseconds.
constexpr uint8_t MARKER = 0x87;
constexpr uint16_t SYNC_MARK_US = 6400;
constexpr uint16_t SHORT_US = 750;
constexpr uint16_t LONG_US = 1460;
constexpr uint32_t FRAME_GAP_US = 63650;

// Maps pincode bits, most-significant first, into the X and Y fields.
struct PinBit {
  bool is_y;
  uint8_t mask;
};

constexpr PinBit PINCODE_MAP[14] = {
    {true, 0x80},   // Y7
    {true, 0x40},   // Y6
    {false, 0x80},  // X7
    {false, 0x40},  // X6
    {true, 0x20},   // Y5
    {false, 0x20},  // X5
    {true, 0x10},   // Y4
    {false, 0x10},  // X4
    {true, 0x08},   // Y3
    {false, 0x08},  // X3
    {true, 0x04},   // Y2
    {true, 0x02},   // Y1
    {true, 0x01},   // Y0
    {false, 0x04},  // X2
};

// Convert the pincode and channel to the two decoded pin fields.
void encode_pin_fields(uint16_t pincode, uint8_t channel, uint8_t &plain0, uint8_t &plain1) {
  uint8_t x = channel;
  uint8_t y = 0;

  for (uint8_t i = 0; i < 14; ++i) {
    if (((pincode >> (13 - i)) & 1U) == 0)
      continue;

    if (PINCODE_MAP[i].is_y)
      y |= PINCODE_MAP[i].mask;
    else
      x |= PINCODE_MAP[i].mask;
  }

  plain1 = ~y;
  plain0 = plain1 - x + 1;
}

// Construct the wire bytes for one seed.
void build_wire(uint8_t wire[5], uint8_t seed, uint8_t plain0, uint8_t plain1) {
  uint16_t sum = MARKER + seed;
  wire[2] = sum;

  uint8_t key = seed + (sum >> 8);

  sum = plain1 + key;
  wire[1] = sum;
  key += sum >> 8;

  wire[0] = plain0 + key;

  // checksum over the first three bytes, little-endian.
  const uint16_t checksum = wire[0] + wire[1] + wire[2];
  wire[3] = checksum;
  wire[4] = checksum >> 8;
}

// Transmit one frame, least-significant bit first.
void transmit_frame(RemoteTransmitData *dst, const uint8_t wire[5]) {
  dst->mark(SYNC_MARK_US);

  for (uint8_t i = 0; i < 5; ++i) {
    const uint8_t byte = wire[i];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      const bool bit_is_zero = ((byte >> bit) & 1U) == 0;

      dst->space(bit_is_zero ? LONG_US : SHORT_US);
      dst->mark(bit_is_zero ? SHORT_US : LONG_US);
    }
  }
}

bool read_bit(RemoteReceiveData &src, bool &bit) {
  // Wire zero: long space, short mark.
  if (src.expect_space(LONG_US)) {
    if (!src.expect_mark(SHORT_US))
      return false;
    bit = false;
    return true;
  }

  // Wire one: short space, long mark.
  if (src.expect_space(SHORT_US)) {
    if (!src.expect_mark(LONG_US))
      return false;
    bit = true;
    return true;
  }

  return false;
}

}  // namespace

void CardinS438Protocol::encode(RemoteTransmitData *dst, const CardinS438Data &data) {
  uint8_t plain0;
  uint8_t plain1;

  encode_pin_fields(data.code & 0x3FFF, data.channel & 0x03, plain0, plain1);

  for (uint8_t frame = 0; frame < data.repeats; ++frame) {
    uint8_t wire[5];
    uint8_t seed = static_cast<uint8_t>(random_uint32() & 0xFF);

    build_wire(wire, seed, plain0, plain1);
    transmit_frame(dst, wire);
    dst->space(FRAME_GAP_US);
  }
}

optional<CardinS438Data> CardinS438Protocol::decode(RemoteReceiveData src) {
  while (src.is_valid()) {
    // Find the frame's long sync mark.
    if (!src.expect_mark(SYNC_MARK_US)) {
      src.advance();
      continue;
    }

    // Read five bytes, least-significant bit first.
    uint8_t wire[5]{};
    bool valid = true;

    for (uint8_t i = 0; i < 5 && valid; ++i) {
      for (uint8_t bit = 0; bit < 8; ++bit) {
        bool value;
        if (!read_bit(src, value)) {
          valid = false;
          break;
        }
        if (value)
          wire[i] |= 1U << bit;
      }
    }

    if (!valid)
      continue;

    // verify checksum
    uint16_t checksum = wire[0] + wire[1] + wire[2];
    if (wire[3] != (checksum & 0xFF) || wire[4] != (checksum >> 8))
      continue;

    // Recover the seed and undo the encoder's carry transform.
    uint8_t seed = wire[2] - MARKER;

    uint8_t key = seed;
    key += (seed + MARKER) >> 8;

    uint8_t plain1 = wire[1] - key;
    key += (plain1 + key) >> 8;

    uint8_t plain0 = wire[0] - key;

    // Recover the original X and Y fields.
    uint8_t x = plain1 - plain0 + 1;
    uint8_t y = ~plain1;

    // Reconstruct the 14-bit code in transmission order.
    uint16_t code = 0;
    for (const auto &mapping : PINCODE_MAP) {
      code <<= 1;
      if ((mapping.is_y ? y : x) & mapping.mask)
        code |= 1;
    }

    return CardinS438Data{
        .code = code,
        .channel = static_cast<uint8_t>(x & 0x03),
        .seed = seed,
    };
  }

  return {};
}

void CardinS438Protocol::dump(const CardinS438Data &data) {
  const char *channel;

  switch (data.channel & 0x03) {
    case 0b10:
      channel = "A";
      break;
    case 0b00:
      channel = "B";
      break;
    case 0b11:
      channel = "C";
      break;
    default:
      channel = "D";
      break;
  }

  ESP_LOGI(TAG, "Received Cardin S438: code=0x%04X channel=%s seed=0x%02X", data.code, channel, data.seed);
}

}  // namespace esphome::remote_base
