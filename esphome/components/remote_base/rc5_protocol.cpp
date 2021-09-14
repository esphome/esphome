#include "rc5_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.rc5";

static const uint32_t BIT_TIME_US = 889;
static const uint8_t NBITS = 14;

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
  RC5Data out{
      .address = 0,
      .command = 0,
  };
  uint8_t field_bit;

  if (src.expect_space(BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
    field_bit = 1;
  } else if (src.expect_space(2 * BIT_TIME_US)) {
    field_bit = 0;
  } else {
    return {};
  }

  if (!(((src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US)) ||
         (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) &&
        (((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
          (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) ||
         ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
          (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US)))))) {
    return {};
  }

  uint32_t out_data = 0;
  for (int bit = NBITS - 4; bit >= 1; bit--) {
    if ((src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) &&
        (src.expect_mark(BIT_TIME_US) || src.peek_mark(2 * BIT_TIME_US))) {
      out_data |= 0 << bit;
    } else if ((src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) &&
               (src.expect_space(BIT_TIME_US) || src.peek_space(2 * BIT_TIME_US))) {
      out_data |= 1 << bit;
    } else {
      return {};
    }
  }
  if (src.expect_space(BIT_TIME_US) || src.expect_space(2 * BIT_TIME_US)) {
    out_data |= 0;
  } else if (src.expect_mark(BIT_TIME_US) || src.expect_mark(2 * BIT_TIME_US)) {
    out_data |= 1;
  }

  out.command = (uint8_t)(out_data & 0x3F) + (1 - field_bit) * 64u;
  out.address = (out_data >> 6) & 0x1F;
  return out;
}
void RC5Protocol::dump(const RC5Data &data) {
  ESP_LOGD(TAG, "Received RC5: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
