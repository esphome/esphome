#include "samsung36_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.samsung36";

static const uint8_t NBITS = 78;

static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 500;
static const uint32_t BIT_ONE_LOW_US = 1500;
static const uint32_t BIT_ZERO_LOW_US = 500;
static const uint32_t MIDDLE_HIGH_US = 500;
static const uint32_t MIDDLE_LOW_US = 4500;
static const uint32_t FOOTER_HIGH_US = 500;
static const uint32_t FOOTER_LOW_US = 59000;

void Samsung36Protocol::encode(RemoteTransmitData *dst, const Samsung36Data &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(NBITS);

  // send header
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  // send first 16 bits
  for (uint32_t mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  // send middle header
  dst->item(MIDDLE_HIGH_US, MIDDLE_LOW_US);

  // send last 20 bits
  for (uint32_t mask = 1UL << 19; mask != 0; mask >>= 1) {
    if (data.command & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  // footer
  dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
}

optional<Samsung36Data> Samsung36Protocol::decode(RemoteReceiveData src) {
  Samsung36Data out{
      .address = 0,
      .command = 0,
  };

  // check if header matches
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  // check if we have enough bits
  if (src.size() != NBITS)
    return {};

  // get the first 16 bits
  for (uint8_t i = 0; i < 16; i++) {
    out.address <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.address |= 1UL;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.address |= 0UL;
    } else {
      return {};
    }
  }

  // check if the middle mark matches
  if (!src.expect_item(MIDDLE_HIGH_US, MIDDLE_LOW_US)) {
    return {};
  }

  // get the last 20 bits
  for (uint8_t i = 0; i < 20; i++) {
    out.command <<= 1UL;
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.command |= 1UL;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.command |= 0UL;
    } else {
      return {};
    }
  }

  return out;
}
void Samsung36Protocol::dump(const Samsung36Data &data) {
  ESP_LOGD(TAG, "Received Samsung36: address=0x%04X, command=0x%08X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
