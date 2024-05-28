#include "keeloq_protocol.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.keeloq";

static const uint32_t BIT_TIME_US = 380;
static const uint8_t NBITS_PREAMBLE = 12;
static const uint8_t NBITS_REPEAT = 1;
static const uint8_t NBITS_VLOW = 1;
static const uint8_t NBITS_SERIAL = 28;
static const uint8_t NBITS_BUTTONS = 4;
static const uint8_t NBITS_DISC = 12;
static const uint8_t NBITS_SYNC_CNT = 16;

static const uint8_t NBITS_FIXED_DATA = NBITS_REPEAT + NBITS_VLOW + NBITS_BUTTONS + NBITS_SERIAL;
static const uint8_t NBITS_ENCRYPTED_DATA = NBITS_BUTTONS + NBITS_DISC + NBITS_SYNC_CNT;
static const uint8_t NBITS_DATA = NBITS_FIXED_DATA + NBITS_ENCRYPTED_DATA;

/*
KeeLoq Protocol

Coded using information from datasheet for Microchip HCS301 KeeLow Code Hopping Encoder

Encoder - Hopping code is generated at random.

Decoder - Hopping code is ignored and not checked when received. Serial number of
transmitter and nutton command is decoded.

*/

void KeeloqProtocol::encode(RemoteTransmitData *dst, const KeeloqData &data) {
  uint32_t out_data = 0x0;

  ESP_LOGD(TAG, "Send Keeloq: address=%07" PRIx32 " command=%03x encrypted=%08" PRIx32, data.address, data.command,
           data.encrypted);
  ESP_LOGV(TAG, "Send Keeloq: data bits (%d + %d)", NBITS_ENCRYPTED_DATA, NBITS_FIXED_DATA);

  // Preamble = '01' x 12
  for (uint8_t cnt = NBITS_PREAMBLE; cnt; cnt--) {
    dst->space(BIT_TIME_US);
    dst->mark(BIT_TIME_US);
  }

  // Header = 10 bit space
  dst->space(10 * BIT_TIME_US);

  // Encrypted field
  out_data = data.encrypted;

  ESP_LOGV(TAG, "Send Keeloq: Encrypted data %04" PRIx32, out_data);

  for (uint32_t mask = 1, cnt = 0; cnt < NBITS_ENCRYPTED_DATA; cnt++, mask <<= 1) {
    if (out_data & mask) {
      dst->mark(1 * BIT_TIME_US);
      dst->space(2 * BIT_TIME_US);
    } else {
      dst->mark(2 * BIT_TIME_US);
      dst->space(1 * BIT_TIME_US);
    }
  }

  // first 32 bits of fixed portion
  out_data = (data.command & 0x0f);
  out_data <<= NBITS_SERIAL;
  out_data |= data.address;
  ESP_LOGV(TAG, "Send Keeloq: Fixed data %04" PRIx32, out_data);

  for (uint32_t mask = 1, cnt = 0; cnt < (NBITS_FIXED_DATA - 2); cnt++, mask <<= 1) {
    if (out_data & mask) {
      dst->mark(1 * BIT_TIME_US);
      dst->space(2 * BIT_TIME_US);
    } else {
      dst->mark(2 * BIT_TIME_US);
      dst->space(1 * BIT_TIME_US);
    }
  }

  // low battery flag
  if (data.vlow) {
    dst->mark(1 * BIT_TIME_US);
    dst->space(2 * BIT_TIME_US);
  } else {
    dst->mark(2 * BIT_TIME_US);
    dst->space(1 * BIT_TIME_US);
  }

  // repeat flag - always sent as a '1'
  dst->mark(1 * BIT_TIME_US);
  dst->space(2 * BIT_TIME_US);

  // Guard time  at end of packet
  dst->space(39 * BIT_TIME_US);
}

optional<KeeloqData> KeeloqProtocol::decode(RemoteReceiveData src) {
  KeeloqData out{
      .encrypted = 0,
      .address = 0,
      .command = 0,
      .repeat = false,
      .vlow = false,

  };

  if (src.size() != (NBITS_PREAMBLE + NBITS_DATA) * 2) {
    return {};
  }

  ESP_LOGVV(TAG,
            "%2" PRId32 ": %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
            " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
            " %" PRId32 " %" PRId32 " %" PRId32,
            src.size(), src.peek(0), src.peek(1), src.peek(2), src.peek(3), src.peek(4), src.peek(5), src.peek(6),
            src.peek(7), src.peek(8), src.peek(9), src.peek(10), src.peek(11), src.peek(12), src.peek(13), src.peek(14),
            src.peek(15), src.peek(16), src.peek(17), src.peek(18), src.peek(19));

  // Check preamble bits
  int8_t bit = NBITS_PREAMBLE - 1;
  while (--bit >= 0) {
    if (!src.expect_mark(BIT_TIME_US) || !src.expect_space(BIT_TIME_US)) {
      ESP_LOGV(TAG, "Decode KeeLoq: Fail 1, %d %" PRId32, bit + 1, src.peek());
      return {};
    }
  }
  if (!src.expect_mark(BIT_TIME_US) || !src.expect_space(10 * BIT_TIME_US)) {
    ESP_LOGV(TAG, "Decode KeeLoq: Fail 1, %d %" PRId32, bit + 1, src.peek());
    return {};
  }

  // Read encrypted bits
  uint32_t out_data = 0;
  for (bit = 0; bit < NBITS_ENCRYPTED_DATA; bit++) {
    if (src.expect_mark(2 * BIT_TIME_US) && src.expect_space(BIT_TIME_US)) {
      out_data |= 0 << bit;
    } else if (src.expect_mark(BIT_TIME_US) && src.expect_space(2 * BIT_TIME_US)) {
      out_data |= 1 << bit;
    } else {
      ESP_LOGV(TAG, "Decode KeeLoq: Fail 2, %" PRIu32 " %" PRId32, src.get_index(), src.peek());
      return {};
    }
  }
  ESP_LOGVV(TAG, "Decode KeeLoq: Data, %d %08" PRIx32, bit, out_data);
  out.encrypted = out_data;

  // Read Serial Number and Button Status
  out_data = 0;
  for (bit = 0; bit < NBITS_SERIAL + NBITS_BUTTONS; bit++) {
    if (src.expect_mark(2 * BIT_TIME_US) && src.expect_space(BIT_TIME_US)) {
      out_data |= 0 << bit;
    } else if (src.expect_mark(BIT_TIME_US) && src.expect_space(2 * BIT_TIME_US)) {
      out_data |= 1 << bit;
    } else {
      ESP_LOGV(TAG, "Decode KeeLoq: Fail 3, %" PRIu32 " %" PRId32, src.get_index(), src.peek());
      return {};
    }
  }
  ESP_LOGVV(TAG, "Decode KeeLoq: Data, %2d %08" PRIx32, bit, out_data);
  out.command = (out_data >> 28) & 0xf;
  out.address = out_data & 0xfffffff;

  // Read Vlow bit
  if (src.expect_mark(2 * BIT_TIME_US) && src.expect_space(BIT_TIME_US)) {
    out.vlow = false;
  } else if (src.expect_mark(BIT_TIME_US) && src.expect_space(2 * BIT_TIME_US)) {
    out.vlow = true;
  } else {
    ESP_LOGV(TAG, "Decode KeeLoq: Fail 4, %" PRId32, src.peek());
    return {};
  }

  // Read Repeat bit
  if (src.expect_mark(2 * BIT_TIME_US) && src.peek_space_at_least(BIT_TIME_US)) {
    out.repeat = false;
  } else if (src.expect_mark(BIT_TIME_US) && src.peek_space_at_least(2 * BIT_TIME_US)) {
    out.repeat = true;
  } else {
    ESP_LOGV(TAG, "Decode KeeLoq: Fail 5, %" PRId32, src.peek());
    return {};
  }

  return out;
}

void KeeloqProtocol::dump(const KeeloqData &data) {
  ESP_LOGD(TAG, "Received Keeloq: address=0x%08" PRIx32 ", command=0x%02x", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
