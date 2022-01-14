#include "samsung36_protocol.h"
#include "helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.samsung36";

static const uint32_t HEADER_MARK_US = 4500;
static const uint32_t HEADER_SPACE_US = 4500;
static const uint32_t BIT_MARK_US = 500;
static const uint32_t BIT_ONE_SPACE_US = 1500;
static const uint32_t BIT_ZERO_SPACE_US = 500;
static const uint32_t MIDDLE_MARK_US = 500;
static const uint32_t MIDDLE_SPACE_US = 4500;
static const uint32_t FOOTER_MARK_US = 500;
static const size_t REMOTE_DATA_SIZE = 2 + 2 * 16 + 2 + 2 * 20 + 2;

USE_SPACE_MSB_CODEC(codec)

void Samsung36Protocol::encode(RemoteTransmitData *dst, const Samsung36Data &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(REMOTE_DATA_SIZE);
  // send header
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  // send first 16 bits
  codec::encode(dst, data.address);
  // send middle header
  dst->item(MIDDLE_MARK_US, MIDDLE_SPACE_US);
  // send last 20 bits
  codec::encode(dst, data.command, 20);
  // footer
  dst->mark(FOOTER_MARK_US);
}

optional<Samsung36Data> Samsung36Protocol::decode(RemoteReceiveData src) {
  Samsung36Data out{
      .address = 0,
      .command = 0,
  };

  // check if we have enough bits
  if (!src.has_size(REMOTE_DATA_SIZE))
    return {};

  // check if header matches
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};

  // get the first 16 bits
  if (codec::decode(src, out.address) != 16)
    return {};

  // check if the middle mark matches
  if (!src.expect_item(MIDDLE_MARK_US, MIDDLE_SPACE_US))
    return {};

  // get the last 20 bits
  if (codec::decode(src, out.command, 20) != 20)
    return {};

  // check footer
  if (!src.expect_mark(BIT_MARK_US))
    return {};

  return out;
}

void Samsung36Protocol::dump(const Samsung36Data &data) {
  ESP_LOGD(TAG, "Received Samsung36: address=0x%04X, command=0x%08X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
