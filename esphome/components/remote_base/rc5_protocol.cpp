#include "rc5_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

static const char *const TAG = "remote.rc5";

static constexpr uint32_t BIT_TIME_US = 889;
static constexpr uint8_t NBITS = 14;
static constexpr uint8_t NHALFBITS = NBITS * 2;

void RC5Protocol::encode(RemoteTransmitData *dst, const RC5Data &data) {
  static bool toggle = false;
  dst->set_carrier_frequency(36000);

  uint64_t out_data = 0;
  uint8_t command = data.command;
  if (data.command >= 64) {
    out_data |= 0b10 << 12;
    command = command - 64;
  } else {
    out_data |= 0b11 << 12;
  }
  out_data |= toggle << 11;
  out_data |= data.address << 6;
  out_data |= command;

  for (uint64_t mask = 1UL << (NBITS - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->space(BIT_TIME_US);
      dst->mark(BIT_TIME_US);
    } else {
      dst->mark(BIT_TIME_US);
      dst->space(BIT_TIME_US);
    }
  }
  toggle = !toggle;
}

namespace {
// Turn 28 Manchester half-bit levels (true = mark) into an RC5 frame.
//
// RC5 is bi-phase: each bit always transitions at its midpoint -- low->high is
// a '1', high->low a '0'. The first start bit S1 is always 1, so if it decodes
// as 0 the capture polarity was inverted (every bit flips) and we invert all 14
// bits. This is what lets the same decoder handle a signal received at either
// polarity (leading mark or leading space).
optional<RC5Data> decode_halfbits(const bool *halfbits) {
  uint16_t bits = 0;
  for (uint8_t i = 0; i < NBITS; i++) {
    const bool first = halfbits[2 * i];
    const bool second = halfbits[2 * i + 1];
    if (first == second)
      return {};  // no midpoint transition -> not a valid Manchester bit
    bits = (bits << 1) | (!first && second ? 1 : 0);
  }

  // S1 (MSB) is always 1; invert the whole frame if the polarity was flipped.
  if (!(bits & (1 << 13)))
    bits = static_cast<uint16_t>(~bits) & 0x3FFF;
  if (!(bits & (1 << 13)))
    return {};

  const bool field_bit = bits & (1 << 12);  // S2: the inverted 7th command bit
  return RC5Data{
      .address = static_cast<uint8_t>((bits >> 6) & 0x1F),
      .command = static_cast<uint8_t>((bits & 0x3F) | (field_bit ? 0 : 0x40)),
      .toggle = static_cast<bool>(bits & (1 << 11)),
  };
}
}  // namespace

optional<RC5Data> RC5Protocol::decode(RemoteReceiveData src) {
  // Expand the runs into half-bit levels (true = mark). Each run is exactly one
  // half-bit (BIT_TIME_US) or two (2 * BIT_TIME_US); stop at anything else.
  bool halfbits[NHALFBITS + 2];
  uint8_t n = 0;
  for (uint32_t i = 0; n <= NHALFBITS && src.is_valid(i); i++) {
    if (src.peek_mark(BIT_TIME_US, i)) {
      halfbits[n++] = true;
    } else if (src.peek_space(BIT_TIME_US, i)) {
      halfbits[n++] = false;
    } else if (src.peek_mark(2 * BIT_TIME_US, i)) {
      halfbits[n++] = true;
      halfbits[n++] = true;
    } else if (src.peek_space(2 * BIT_TIME_US, i)) {
      halfbits[n++] = false;
      halfbits[n++] = false;
    } else {
      break;
    }
  }

  // The leading half-bit is always dropped: S1 is 1, so its first half is the
  // idle level (at either polarity) and merges into the pre-frame idle, giving
  // 27 captured halves -- or 26 when the final bit also ends on idle and its
  // trailing half is dropped too. A dropped edge half is the inverse of its
  // partner (a Manchester bit always transitions mid-bit), so reconstruct the
  // missing half-bits to recover the full 28-half-bit frame.
  if (n != NHALFBITS - 1 && n != NHALFBITS - 2)
    return {};
  bool frame[NHALFBITS];
  frame[0] = !halfbits[0];  // leading half (always dropped)
  for (uint8_t i = 0; i < n; i++)
    frame[i + 1] = halfbits[i];
  if (n == NHALFBITS - 2)  // final bit ended on idle -> trailing half dropped too
    frame[NHALFBITS - 1] = !halfbits[n - 1];
  return decode_halfbits(frame);
}

void RC5Protocol::dump(const RC5Data &data) {
  ESP_LOGI(TAG, "Received RC5: address=0x%02X, command=0x%02X, toggle=%s", data.address, data.command,
           YESNO(data.toggle));
}

}  // namespace esphome::remote_base
