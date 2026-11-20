#include "rmt_one_wire_codec.h"

#ifdef USE_ONE_WIRE_RMT

namespace esphome::gpio::rmt_one_wire {

using one_wire::TransactionStatus;

// Sent values in us: inside TI TMP1826 (SBOSA45D, table 7.6, standard mode) and the DS18B20 AC table.
// The Low times are how long the master drives the line, the rise time comes on top.
static constexpr uint16_t RESET_LOW_US = 520;   // middle of tRSTL 480-560
static constexpr uint16_t RESET_HIGH_US = 500;  // tRSTH >= 480
static constexpr uint8_t WRITE1_LOW_US = 6;     // tWR1L 2-15, TI SPMA057E: "typical about 6 us"
static constexpr uint8_t READ_LOW_US = 4;       // tRL 2.5-5

// Accepted when receiving a reset, in us
static constexpr uint16_t PRESENCE_MIN_LOW_US = 15;
static constexpr uint32_t PRESENCE_LATEST_START_US = 300;  // after the release: tPDH(max) + tPDL(max)

/// The classes of a received symbol. L's duration1 is stale memory and must never be read.
enum class SymbolClass : uint8_t {
  TIME,         ///< T: Low, then High
  ENDS_ON_LOW,  ///< L: the Low phase reached the RX idle
  END_MARKER,   ///< M: empty symbol behind the end
  MALFORMED,
};

static SymbolClass classify(const Symbol &symbol) {
  if (symbol.level0 == 0)
    return symbol.duration0 == 0 || symbol.level1 == 0 ? SymbolClass::ENDS_ON_LOW : SymbolClass::TIME;
  const bool empty = symbol.duration0 == 0 && symbol.duration1 == 0 && symbol.level1 == 1;
  return empty ? SymbolClass::END_MARKER : SymbolClass::MALFORMED;
}

static Symbol slot_symbol(uint8_t low_us, const Timing &timing) {
  return Symbol{low_us, 0, static_cast<uint16_t>(timing.slot_us - low_us), 1};
}

Symbol reset_symbol() { return Symbol{RESET_LOW_US, 0, RESET_HIGH_US, 1}; }

size_t encode_write_slots(const uint8_t *data, size_t bit_offset, size_t num_bits, Symbol *out, const Timing &timing) {
  for (size_t i = 0; i < num_bits; i++) {
    const size_t bit = bit_offset + i;
    const bool one = (data[bit / 8] >> (bit % 8)) & 1;
    out[i] = slot_symbol(one ? WRITE1_LOW_US : timing.write0_low_us, timing);
  }
  return num_bits;
}

size_t encode_read_slots(size_t num_bits, Symbol *out, const Timing &timing) {
  for (size_t i = 0; i < num_bits; i++)
    out[i] = slot_symbol(READ_LOW_US, timing);
  return num_bits;
}

TransactionStatus decode_reset(const Symbol *capture, size_t count) {
  if (count == 0)
    return TransactionStatus::CAPTURE_ERROR;

  // The only allowed forms are T+ M* and T* L
  size_t times = 0;
  while (times < count && classify(capture[times]) == SymbolClass::TIME)
    times++;
  if (times == count - 1 && classify(capture[times]) == SymbolClass::ENDS_ON_LOW)
    return TransactionStatus::BUS_LOW;
  if (times == 0)
    return TransactionStatus::CAPTURE_ERROR;
  for (size_t i = times; i < count; i++) {
    if (classify(capture[i]) != SymbolClass::END_MARKER)
      return TransactionStatus::CAPTURE_ERROR;
  }

  uint32_t since_release = capture[0].duration1;
  for (size_t i = 1; i < times && since_release <= PRESENCE_LATEST_START_US; i++) {
    if (capture[i].duration0 >= PRESENCE_MIN_LOW_US)
      return TransactionStatus::OK;
    since_release += capture[i].duration0 + capture[i].duration1;
  }
  return TransactionStatus::NO_PRESENCE;
}

bool decode_read_slots(const Symbol *capture, size_t count, size_t num_bits, uint8_t *out, size_t bit_offset,
                       const Timing &timing) {
  // The only allowed form is T x num_bits, then M*
  if (count < num_bits)
    return false;
  for (size_t i = num_bits; i < count; i++) {
    if (classify(capture[i]) != SymbolClass::END_MARKER)
      return false;
  }
  for (size_t i = 0; i < num_bits; i++) {
    if (classify(capture[i]) != SymbolClass::TIME || capture[i].duration0 >= timing.slot_us)
      return false;
    const size_t bit = bit_offset + i;
    const uint8_t mask = 1u << (bit % 8);
    if (capture[i].duration0 < timing.read_threshold_us) {
      out[bit / 8] |= mask;
    } else {
      out[bit / 8] &= ~mask;
    }
  }
  return true;
}

}  // namespace esphome::gpio::rmt_one_wire

#endif  // USE_ONE_WIRE_RMT
