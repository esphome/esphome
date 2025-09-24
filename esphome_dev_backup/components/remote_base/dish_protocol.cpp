#include "dish_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.dish";

static const uint32_t HEADER_HIGH_US = 400;
static const uint32_t HEADER_LOW_US = 6100;
static const uint32_t BIT_HIGH_US = 400;
static const uint32_t BIT_ONE_LOW_US = 1700;
static const uint32_t BIT_ZERO_LOW_US = 2800;

void DishProtocol::encode(RemoteTransmitData *dst, const DishData &data) {
  dst->reserve(138);
  dst->set_carrier_frequency(57600);

  // HEADER
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  //  Typically a DISH device needs to get a command a total of
  //  at least 4 times to accept it.
  for (uint i = 0; i < 4; i++) {
    // COMMAND (function, in MSB)
    for (uint8_t mask = 1UL << 5; mask; mask >>= 1) {
      if (data.command & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }

    // ADDRESS (unit code, in LSB)
    for (uint8_t mask = 1UL; mask < 1UL << 4; mask <<= 1) {
      if ((data.address - 1) & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }
    // PADDING
    for (uint j = 0; j < 6; j++)
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);

    // FOOTER
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  }
}
optional<DishData> DishProtocol::decode(RemoteReceiveData src) {
  DishData data{
      .address = 0,
      .command = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint8_t mask = 1UL << 5; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.command |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.command &= ~mask;
    } else {
      return {};
    }
  }

  for (uint8_t mask = 1UL; mask < 1UL << 5; mask <<= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.address |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.address &= ~mask;
    } else {
      return {};
    }
  }
  for (uint j = 0; j < 6; j++) {
    if (!src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      return {};
    }
  }
  data.address++;

  src.expect_item(HEADER_HIGH_US, HEADER_LOW_US);

  return data;
}

void DishProtocol::dump(const DishData &data) {
  ESP_LOGI(TAG, "Received Dish: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
