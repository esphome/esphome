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

optional<RC5Data> RC5Protocol::decode(RemoteReceiveData src) {
  // Expand the runs into half-bit levels (true = mark). Each run is exactly one
  // half-bit (BIT_TIME_US) or two (2 * BIT_TIME_US); stop at anything else.
  //
  // halfbits[0] is reserved for the leading half-bit, which is always dropped --
  // S1 is 1, so its first half sits at the idle level (at either polarity) and
  // merges into the pre-frame idle. Captured half-bits start at index 1.
  bool halfbits[NHALFBITS + 2];
  uint8_t n = 1;
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

  // Expect a full frame once the leading half is restored: 27 captured halves
  // (n == 28) or 26 when the final bit also ends on idle and its trailing half
  // is dropped too (n == 27). A dropped edge half is the inverse of its partner
  // (a Manchester bit always transitions mid-bit), so reconstruct the leading
  // half (always) and the trailing half (only when it was dropped).
  if (n != NHALFBITS && n != NHALFBITS - 1) {
    return {};
  }
  halfbits[0] = !halfbits[1];
  if (n == NHALFBITS - 1) {
    halfbits[n] = !halfbits[n - 1];
  }

  const bool carrier = halfbits[1];
  uint16_t bits = 0;
  for (uint8_t i = 0; i < NBITS; i++) {
    const bool first = halfbits[2 * i];
    const bool second = halfbits[2 * i + 1];
    if (first == second) {
      return {};  // no midpoint transition -> not a valid Manchester bit
    }
    bits = (bits << 1) | (second == carrier ? 1 : 0);
  }

  const bool field_bit = bits & (1 << 12);  // S2: the inverted 7th command bit
  return RC5Data{
      .address = static_cast<uint8_t>((bits >> 6) & 0x1F),
      .command = static_cast<uint8_t>((bits & 0x3F) | (field_bit ? 0 : 0x40)),
  };
}

void RC5Protocol::dump(const RC5Data &data) {
  ESP_LOGI(TAG, "Received RC5: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace esphome::remote_base
