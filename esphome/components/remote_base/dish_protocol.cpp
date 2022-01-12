#include "dish_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.dish";

static const uint32_t HEADER_MARK_US = 400;
static const uint32_t HEADER_SPACE_US = 6100;
static const uint32_t BIT_MARK_US = 400;
static const uint32_t BIT_ONE_SPACE_US = 1700;
static const uint32_t BIT_ZERO_SPACE_US = 2800;
static const size_t REMOTE_DATA_SIZE = 2 + 4 * (2 * 6 + 2 * 4 + 2 * 6 + 2);

void DishProtocol::encode(RemoteTransmitData *dst, const DishData &data) {
  dst->set_carrier_frequency(57600);
  dst->reserve(REMOTE_DATA_SIZE);
  // HEADER
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  //  Typically a DISH device needs to get a command a total of
  //  at least 4 times to accept it.
  for (unsigned i = 0; i < 4; i++) {
    // COMMAND (function, 6 bits, in MSB)
    msb::encode<6, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.command);
    // ADDRESS (unit code, 4 bits, in LSB)
    lsb::encode<4, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.address - 1);
    // PADDING (6 zero bits)
    msb::encode<6, BIT_MARK_US, 0, BIT_ZERO_SPACE_US>(dst, 0);
    // FOOTER
    dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  }
}

static bool decode_data(RemoteReceiveData &src, DishData &dst) {
  return msb::decode<6, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.command) &&
         lsb::decode<4, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.address);
}

optional<DishData> DishProtocol::decode(RemoteReceiveData src) {
  DishData data{
      .address = 0,
      .command = 0,
  };
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) || !decode_data(src, data))
    return {};
  if (!msb::equal<6, BIT_MARK_US, 0, BIT_ZERO_SPACE_US>(src, 0))
    return {};
  if (!src.expect_mark(HEADER_MARK_US))
    return {};
  data.address++;
  return data;
}

void DishProtocol::dump(const DishData &data) {
  ESP_LOGD(TAG, "Received Dish: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
