#include "cardin_s438_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
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
constexpr uint32_t FRAME_GAP_US = 63650;
constexpr uint8_t WIRE_SIZE = 5;
// Each bit is a space then a mark, and both lengths carry it
constexpr BitTimings CARDIN_TIMINGS{
    .one_space = 750, .zero_space = 1460, .one_mark = 1460, .zero_mark = 750, .space_first = true};

// Maps pincode bits, most-significant first, to a bit of the X or Y field: PIN_Y marks Y, the low bits are the bit
// number. In flash, so it stays out of RAM on ESP8266.
constexpr uint8_t PIN_Y = 0x08;
constexpr uint8_t PINCODE_BITS = 14;
constexpr uint8_t PINCODE_MAP[PINCODE_BITS] PROGMEM = {
    PIN_Y | 7, PIN_Y | 6, 7, 6, PIN_Y | 5, 5, PIN_Y | 4, 4, PIN_Y | 3, 3, PIN_Y | 2, PIN_Y | 1, PIN_Y | 0, 2,
};

// Convert the pincode and channel to the two decoded pin fields.
void encode_pin_fields(uint16_t pincode, uint8_t channel, uint8_t &plain0, uint8_t &plain1) {
  uint8_t x = channel;
  uint8_t y = 0;

  for (uint8_t i = 0; i < PINCODE_BITS; ++i) {
    if (((pincode >> (PINCODE_BITS - 1 - i)) & 1U) == 0)
      continue;
    const uint8_t entry = progmem_read_byte(&PINCODE_MAP[i]);
    (entry & PIN_Y ? y : x) |= 1U << (entry & 0x07);
  }

  plain1 = ~y;
  plain0 = plain1 - x + 1;
}

// Construct the wire bytes for one seed.
void build_wire(uint8_t wire[WIRE_SIZE], uint8_t seed, uint8_t plain0, uint8_t plain1) {
  uint16_t sum = MARKER + seed;
  wire[2] = sum;

  uint8_t key = seed + (sum >> 8);

  sum = plain1 + key;
  wire[1] = sum;
  key += sum >> 8;

  wire[0] = plain0 + key;

  // checksum over the first three bytes, little-endian.
  const uint16_t checksum = sum16_bytes(wire, 3);
  wire[3] = checksum;
  wire[4] = checksum >> 8;
}

}  // namespace

void CardinS438Protocol::encode(RemoteTransmitData *dst, const CardinS438Data &data) {
  uint8_t plain0;
  uint8_t plain1;

  encode_pin_fields(data.code & 0x3FFF, data.channel & 0x03, plain0, plain1);

  for (uint8_t frame = 0; frame < data.repeats; ++frame) {
    uint8_t wire[WIRE_SIZE];
    uint8_t seed = static_cast<uint8_t>(random_uint32() & 0xFF);

    build_wire(wire, seed, plain0, plain1);
    dst->mark(SYNC_MARK_US);
    dst->item_bytes<CARDIN_TIMINGS>(wire, WIRE_SIZE);
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

    uint8_t wire[WIRE_SIZE];
    if (!src.expect_bytes<CARDIN_TIMINGS>(wire, WIRE_SIZE) ||
        encode_value_le<uint16_t>(wire + 3) != sum16_bytes(wire, 3)) {
      continue;
    }

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
    for (const uint8_t &slot : PINCODE_MAP) {
      const uint8_t entry = progmem_read_byte(&slot);
      code = (code << 1) | (((entry & PIN_Y ? y : x) >> (entry & 0x07)) & 1U);
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
  // The DIP switches as the config takes them; channels as CARDIN_S438_CHANNELS in __init__.py maps them
  char code[PINCODE_BITS + 1];
  for (uint8_t i = 0; i < PINCODE_BITS; ++i)
    code[i] = (data.code >> (PINCODE_BITS - 1 - i)) & 1U ? '1' : '0';
  code[PINCODE_BITS] = '\0';
  ESP_LOGI(TAG, "Received Cardin S438: code=%s channel=%c seed=0x%02X", code, "BDAC"[data.channel & 0x03], data.seed);
}

}  // namespace esphome::remote_base
