#include "nec_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.nec";

static const int32_t NUM_ITEMS = 68;
static const int32_t TICK_US = 560;
static const int32_t HEADER_MARK_US = 16 * TICK_US;
static const int32_t HEADER_SPACE_US = 8 * TICK_US;
static const int32_t BIT_MARK_US = 1 * TICK_US;
static const int32_t BIT_ONE_SPACE_US = 3 * TICK_US;
static const int32_t BIT_ZERO_SPACE_US = 1 * TICK_US;

void NECProtocol::encode(RemoteTransmitData *dst, const NECData &data) {
  dst->reserve(NUM_ITEMS);
  dst->set_carrier_frequency(38000);
  // Header
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  // Address
  for (uint16_t mask = 1; mask; mask <<= 1)
    dst->item(BIT_MARK_US, (data.address & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  // Command
  for (uint8_t mask = 1; mask; mask <<= 1)
    dst->item(BIT_MARK_US, (data.command & mask) ? BIT_ONE_SPACE_US : BIT_ZERO_SPACE_US);
  // ~Command
  for (uint8_t mask = 1; mask; mask <<= 1)
    dst->item(BIT_MARK_US, (data.command & mask) ? BIT_ZERO_SPACE_US : BIT_ONE_SPACE_US);
  // Final
  dst->mark(BIT_MARK_US);
}

optional<NECData> NECProtocol::decode(RemoteReceiveData src) {
  NECData data{
      .address = 0,
      .command = 0,
  };
  if (src.size() != NUM_ITEMS)
    return {};
  // Header
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US))
    return {};
  // Address
  for (uint16_t mask = 1; mask; mask <<= 1) {
    if (!src.expect_mark(BIT_MARK_US))
      return {};
    if (src.expect_space(BIT_ONE_SPACE_US)) {
      data.address |= mask;
    } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
      return {};
    }
  }
  // Command
  for (uint8_t mask = 1; mask; mask <<= 1) {
    if (!src.expect_mark(BIT_MARK_US))
      return {};
    if (src.expect_space(BIT_ONE_SPACE_US)) {
      data.command |= mask;
    } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
      return {};
    }
  }
  // Check for inverse command
  for (uint8_t mask = 1; mask; mask <<= 1) {
    if (!src.expect_item(BIT_MARK_US, (data.command & mask) ? BIT_ZERO_SPACE_US : BIT_ONE_SPACE_US))
      return {};
  }
  if (!src.expect_mark(BIT_MARK_US))
    return {};
  return data;
}

void NECProtocol::dump(const NECData &data) {
  ESP_LOGD(TAG, "Received NEC: address=0x%04X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
