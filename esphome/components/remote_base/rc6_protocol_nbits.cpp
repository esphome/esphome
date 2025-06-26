#include "rc6_protocol_nbits.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const RC6NBITS_TAG = "remote.rc6nbits";

static const uint16_t RC6NBITS_FREQ = 36000;
static const uint16_t RC6NBITS_UNIT = 444;
static const uint16_t RC6NBITS_HEADER_MARK = (6 * RC6NBITS_UNIT);
static const uint16_t RC6NBITS_HEADER_SPACE = (2 * RC6NBITS_UNIT);
static const uint16_t RC6NBITS_MODE_MASK = 0x07;
static const uint32_t RC6NBITS_FOOTER_SPACE = (187 * RC6NBITS_UNIT);

void RC6NbitsProtocol::encode(RemoteTransmitData *dst, const RC6NbitsData &data) {
  dst->reserve(4 + 2 * data.nbits);
  dst->set_carrier_frequency(RC6NBITS_FREQ);

  // Encode header
  dst->item(RC6NBITS_HEADER_MARK, RC6NBITS_HEADER_SPACE);

  // Encode startbit
  dst->item(RC6NBITS_UNIT, RC6NBITS_UNIT);

  int i = 1;
  int32_t next = 0;

  for (uint64_t mask = 1ULL << (data.nbits - 1) ; mask; i++, mask >>= 1) {
      uint16_t bit_time = (i == 4) ? 2 * RC6NBITS_UNIT : RC6NBITS_UNIT; // traditionally, toggle bit
      if (data.code & mask) {
        dst->mark(bit_time);
        dst->space(bit_time);
      } else {
        dst->space(bit_time);
        dst->mark(bit_time);
      }
  }

  dst->space(RC6NBITS_FOOTER_SPACE);

}

optional<RC6NbitsData> RC6NbitsProtocol::decode(RemoteReceiveData src) {
  RC6NbitsData data{
      .nbits = 0,
      .code = 0,
  };

  // Check if header matches
  if (!src.expect_item(RC6NBITS_HEADER_MARK, RC6NBITS_HEADER_SPACE)) {
    return {};
  }

  // Check if the start bit matches. (Should be 1)
  if (src.peek() < 0) {
    return {};
  }
  src.advance();
  if (src.peek_space(RC6NBITS_UNIT))
    src.advance();

  // Data
  uint8_t bit{0};
  uint8_t offset{0};
  uint64_t buffer{0};

  while (offset < 64) {
    if (src.get_index() == src.size() -1)
      break;
    bit = src.peek() > 0;
    buffer = (buffer << 1) | bit;
    src.advance();
    offset ++;

    uint16_t bit_time = (offset == 4) ? RC6NBITS_UNIT * 2: RC6NBITS_UNIT;
    uint16_t next_bit_time = (offset == 3) ? RC6NBITS_UNIT * 2: RC6NBITS_UNIT;

    if (src.get_index() == src.size() - 1)
      break;

    if (src.peek_mark(bit_time) || src.peek_space(bit_time)) {
      src.advance();
    } else if (!src.peek_mark(bit_time + next_bit_time) && !src.peek_space(bit_time + next_bit_time)) {
      return {};
    }
  }

  data.code = buffer;
  data.nbits = offset;

  return data;
}

void RC6NbitsProtocol::dump(const RC6NbitsData &data) {
  ESP_LOGI(RC6NBITS_TAG, "Received RC6Nbits: nbits=%d, code=0x%llX", data.nbits, data.code);
}

}  // namespace remote_base
}  // namespace esphome
