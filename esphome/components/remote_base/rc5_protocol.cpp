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
      .toggle = 0,
  };

  enum States { MARK, SPACE, EMPTY };

  States state = src.expect_mark(BIT_TIME_US * 2) ? MARK : src.expect_mark(BIT_TIME_US) ? EMPTY : SPACE;
  if (state == SPACE)
    return {};  // invalid initial state

  uint8_t bits_in = 1;
  uint32_t out_data = 1;
  bool gotmark = false;

  // move 1 bit / full clock cycle
  while (bits_in < NBITS) {
    if (state == EMPTY) {
      if ((gotmark = src.expect_mark(BIT_TIME_US)) || (src.expect_space(BIT_TIME_US))) {
        state = gotmark ? MARK : SPACE;
      } else
        break;
    }
    out_data = (out_data << 1) | (state != MARK);
    bits_in++;
    state = (state == SPACE) && src.expect_mark(BIT_TIME_US * 2) ? MARK
            : src.expect_space(2 * BIT_TIME_US)                  ? SPACE
                                                                 : EMPTY;

    if ((state == EMPTY) && !(src.expect_mark(BIT_TIME_US) || src.expect_space(BIT_TIME_US)))
      break;
  }

  if (bits_in != NBITS)
    return {};

  out.command = (uint8_t) (out_data & 0x3F) | ((~out_data & 0x1000) >> 6);
  out.address = (out_data >> 6) & 0x1F;
  out.toggle = (out_data & 0x0800) != 0;

  return out;
}

void RC5Protocol::dump(const RC5Data &data) {
  ESP_LOGI(TAG, "Received RC5: address=0x%02X, command=0x%02X, toggle=0x%02X", data.address, data.command, data.toggle);
}

}  // namespace remote_base
}  // namespace esphome
