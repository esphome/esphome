#include "dish_protocol.h"
#include "helpers.h"
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

DECLARE_SPACE_CODEC(codec)

void DishProtocol::encode(RemoteTransmitData *dst, const DishData &data) {
  dst->set_carrier_frequency(57600);
  dst->reserve(REMOTE_DATA_SIZE);
  // HEADER
  dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  //  Typically a DISH device needs to get a command a total of
  //  at least 4 times to accept it.
  for (unsigned i = 0; i < 4; i++) {
    // COMMAND (function, 6 bits, in MSB)
    codec::msb::encode(dst, data.command, 6);
    // ADDRESS (unit code, 4 bits, in LSB)
    codec::lsb::encode(dst, data.address - 1, 4);
    // PADDING (6 zero bits)
    codec::msb::encode(dst, 0, 6);
    // FOOTER
    dst->item(HEADER_MARK_US, HEADER_SPACE_US);
  }
}

static bool decode_data(RemoteReceiveData &src, DishData &dst) {
  return codec::msb::decode(src, dst.command, 6) == 6 && codec::lsb::decode(src, dst.address, 4) == 4 &&
         codec::msb::equal(src, 0, 6);
}

optional<DishData> DishProtocol::decode(RemoteReceiveData src) {
  DishData data{
      .address = 0,
      .command = 0,
  };
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US) || !decode_data(src, data) || !src.expect_mark(HEADER_MARK_US))
    return {};
  data.address++;
  return data;
}

void DishProtocol::dump(const DishData &data) {
  ESP_LOGD(TAG, "Received Dish: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
