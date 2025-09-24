#include "byronsx_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.byronsx";

static const uint32_t BIT_TIME_US = 333;
static const uint8_t NBITS_ADDRESS = 8;
static const uint8_t NBITS_COMMAND = 4;
static const uint8_t NBITS_START_BIT = 1;
static const uint8_t NBITS_DATA = NBITS_ADDRESS + NBITS_COMMAND /*+ NBITS_COMMAND*/;

/*
ByronSX Protocol
Each transmitted packet appears to consist of thirteen bits of PWM encoded
data. Each bit period of aprox 1ms consists of a transmitter OFF period
followed by a transmitter ON period. The 'on' and 'off' periods are either
short (approx 332us) or long (664us).

A short 'off' followed by a long 'on' represents a '1' bit.
A long 'off' followed by a short 'on' represents a '0' bit.

A the beginning of each packet is and initial 'off' period of approx 5.6ms
followed by a short 'on'.

The data payload consists of twelve bits which appear to be an eight bit
address floowied by a 4 bit chime number.

SAAAAAAAACCCC

Whese:
S = the initial short start pulse
A = The eight address bits
C - The four chime bits

--------------------

I have also used 'RFLink' software (RLink Firmware Version: 1.1 Revision: 48)
to capture these packets, eg:

20;19;Byron;ID=004f;SWITCH=02;CMD=ON;CHIME=02;

This module transmits and interprets packets in the same way as RFLink.

marshn

*/

void ByronSXProtocol::encode(RemoteTransmitData *dst, const ByronSXData &data) {
  uint32_t out_data = 0x0;

  ESP_LOGD(TAG, "Send ByronSX: address=%04x command=%03x", data.address, data.command);

  out_data = data.address;
  out_data <<= NBITS_COMMAND;
  out_data |= data.command;

  ESP_LOGV(TAG, "Send ByronSX: out_data %03" PRIx32, out_data);

  // Initial Mark start bit
  dst->mark(1 * BIT_TIME_US);

  for (uint32_t mask = 1UL << (NBITS_DATA - 1); mask != 0; mask >>= 1) {
    if (out_data & mask) {
      dst->space(2 * BIT_TIME_US);
      dst->mark(1 * BIT_TIME_US);
    } else {
      dst->space(1 * BIT_TIME_US);
      dst->mark(2 * BIT_TIME_US);
    }
  }
  // final space at end of packet
  dst->space(17 * BIT_TIME_US);
}

optional<ByronSXData> ByronSXProtocol::decode(RemoteReceiveData src) {
  ByronSXData out{
      .address = 0,
      .command = 0,
  };

  if (src.size() != (NBITS_DATA + NBITS_START_BIT) * 2) {
    return {};
  }

  // Skip start bit
  if (!src.expect_mark(BIT_TIME_US)) {
    return {};
  }

  ESP_LOGVV(TAG,
            "%3" PRId32 ": %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
            " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
            " %" PRId32 " %" PRId32 " %" PRId32,
            src.size(), src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4), src.peek(5), src.peek(6),
            src.peek(7), src.peek(8), src.peek(9), src.peek(10), src.peek(11), src.peek(12), src.peek(13), src.peek(14),
            src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));

  ESP_LOGVV(TAG, "     %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32, src.peek(20),
            src.peek(21), src.peek(22), src.peek(23), src.peek(24), src.peek(25));

  // Read data bits
  uint32_t out_data = 0;
  int8_t bit = NBITS_DATA;
  while (--bit >= 0) {
    if (src.expect_space(2 * BIT_TIME_US) && src.expect_mark(BIT_TIME_US)) {
      out_data |= 1 << bit;
    } else if (src.expect_space(BIT_TIME_US) && src.expect_mark(2 * BIT_TIME_US)) {
      out_data |= 0 << bit;
    } else {
      ESP_LOGV(TAG, "Decode ByronSX: Fail 2, %2d %08" PRIx32, bit, out_data);
      return {};
    }
    ESP_LOGVV(TAG, "Decode ByronSX: Data, %2d %08" PRIx32, bit, out_data);
  }

  // last bit followed by a long space
  if (!src.peek_space_at_least(BIT_TIME_US)) {
    ESP_LOGV(TAG, "Decode ByronSX: Fail 4 ");
    return {};
  }

  out.command = (uint8_t) (out_data & 0xF);
  out_data >>= NBITS_COMMAND;
  out.address = (uint16_t) (out_data & 0xFF);

  return out;
}

void ByronSXProtocol::dump(const ByronSXData &data) {
  ESP_LOGD(TAG, "Received ByronSX: address=0x%08X, command=0x%02x", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
