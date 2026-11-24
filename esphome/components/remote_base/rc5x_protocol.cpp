#include "rc5x_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.rc5x");

static constexpr uint32_t HALF_BIT_US = 889;  // a Manchester bit is two half-bits
// Start, field and toggle bits plus the address, then a pause of 4 half-bits, then command and extension
static constexpr uint8_t RC5_TOGGLE_BIT = 1;
static constexpr uint8_t RC5_ADDRESS_BITS = 5;
static constexpr uint8_t FIRST_PART_NBITS = 8;
static constexpr uint8_t RC5_COMMAND_BITS = 6;
static constexpr uint8_t RC5_EXT_BITS = 6;
static constexpr uint8_t NBITS = 20;
static constexpr uint8_t PAUSE_HALFBITS = 4;
// Half-bits in a frame, counting the pause; the pause starts after the first part's half-bits
static constexpr uint8_t NFRAMEHALFBITS = NBITS * 2 + PAUSE_HALFBITS;
static constexpr uint8_t PAUSE_START = FIRST_PART_NBITS * 2;

void RC5XProtocol::encode(RemoteTransmitData *dst, const RC5XData &data) {
  static bool toggle = false;
  dst->set_carrier_frequency(36000);
  dst->reserve(NFRAMEHALFBITS);
  // Start bit, then the field bit, which is clear for commands 64 to 127
  const uint8_t command = data.command & 0x7F;  // bit 6 travels in the field bit, higher bits have no place
  const uint32_t start_field = command >= 64 ? 0b10 : 0b11;
  const uint32_t out_data = start_field << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(toggle) << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(data.address & 0x1F) << (RC5_COMMAND_BITS + RC5_EXT_BITS) |
                            uint32_t(command & 0x3F) << RC5_EXT_BITS | (data.extension & 0x3F);

  for (uint8_t bit = NBITS; bit-- > 0;) {
    if (bit == NBITS - 1 - FIRST_PART_NBITS) {
      for (uint8_t i = 0; i < PAUSE_HALFBITS; i++)
        dst->space(HALF_BIT_US);
    }
    if (out_data & (1UL << bit)) {
      dst->space(HALF_BIT_US);
      dst->mark(HALF_BIT_US);
    } else {
      dst->mark(HALF_BIT_US);
      dst->space(HALF_BIT_US);
    }
  }
  toggle = !toggle;
}

optional<RC5XData> RC5XProtocol::decode(RemoteReceiveData src) {
  // Expand the runs into half-bit levels (true = mark), as the RC5 decoder does. Each run is one
  // half-bit (HALF_BIT_US) or two, except the pause, which is PAUSE_HALFBITS half-bits at the idle
  // level plus up to one merged half on each side (the end of bit 12 and the start of bit 11).
  //
  // halfbits[0] is reserved for the leading half-bit, which is always dropped: S1 is 1, so its
  // first half sits at the idle level and merges into the pre-frame idle.
  bool halfbits[NFRAMEHALFBITS + 2];
  uint8_t n = 1;
  bool pause_seen = false;
  for (uint32_t i = 0; n <= NFRAMEHALFBITS && src.is_valid(i); i++) {
    if (src.peek_mark(HALF_BIT_US, i)) {
      halfbits[n++] = true;
    } else if (src.peek_space(HALF_BIT_US, i)) {
      halfbits[n++] = false;
    } else if (src.peek_mark(2 * HALF_BIT_US, i)) {
      halfbits[n++] = true;
      halfbits[n++] = true;
    } else if (src.peek_space(2 * HALF_BIT_US, i)) {
      halfbits[n++] = false;
      halfbits[n++] = false;
    } else {
      // The pause must start right after bit 12, or one half earlier when bit 12 ends idle
      if (pause_seen || (n != PAUSE_START && n != PAUSE_START - 1))
        break;
      const int32_t value = src.peek(i);
      const bool level = value > 0;
      const uint32_t halves = (static_cast<uint32_t>(value > 0 ? value : -value) + HALF_BIT_US / 2) / HALF_BIT_US;
      if (level == halfbits[1] || halves < PAUSE_HALFBITS || halves > PAUSE_HALFBITS + 2)
        break;
      for (uint32_t h = 0; h < halves; h++)
        halfbits[n++] = level;
      // It may also run one half into bit 11, but must cover the whole pause
      if (n != PAUSE_START + PAUSE_HALFBITS && n != PAUSE_START + PAUSE_HALFBITS + 1)
        return {};
      pause_seen = true;
    }
  }

  // A full frame, or one half short when the last bit ends idle and its trailing half is dropped.
  // A dropped edge half is the inverse of its partner, since a Manchester bit always changes mid-bit.
  if (!pause_seen || (n != NFRAMEHALFBITS && n != NFRAMEHALFBITS - 1)) {
    return {};
  }
  halfbits[0] = !halfbits[1];
  if (n == NFRAMEHALFBITS - 1) {
    halfbits[n] = !halfbits[n - 1];
  }

  const bool carrier = halfbits[1];
  uint32_t bits = 0;
  for (uint8_t i = 0; i < NBITS; i++) {
    const uint8_t h = 2 * i + (i < FIRST_PART_NBITS ? 0 : PAUSE_HALFBITS);
    const bool first = halfbits[h];
    const bool second = halfbits[h + 1];
    if (first == second) {
      return {};  // no midpoint transition -> not a valid Manchester bit
    }
    bits = (bits << 1) | (second == carrier ? 1 : 0);
  }

  // [18 field] [17 toggle] [16-12 address] [11-6 command] [5-0 extension]
  const bool field_bit = bits & (1UL << (NBITS - 2));  // S2: the inverted 7th command bit
  return RC5XData{
      .address = static_cast<uint8_t>((bits >> (RC5_COMMAND_BITS + RC5_EXT_BITS)) & 0x1F),
      .command = static_cast<uint8_t>(((bits >> RC5_EXT_BITS) & 0x3F) | (field_bit ? 0 : 0x40)),
      .extension = static_cast<uint8_t>(bits & 0x3F),
  };
}

void RC5XProtocol::dump(const RC5XData &data) {
  ESP_LOGI(TAG, "Received RC5X (Marantz): address=0x%02X, command=0x%02X, extension=0x%02X", data.address, data.command,
           data.extension);
}

}  // namespace esphome::remote_base
