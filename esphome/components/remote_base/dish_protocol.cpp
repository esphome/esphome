#include "dish_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.dish";

static const uint32_t HEADER_HIGH_US = 400;
static const uint32_t HEADER_LOW_US = 6100;
static const uint32_t BIT_MARK_US = 400;
static const uint32_t BIT_ONE_SPACE_US = 1700;
static const uint32_t BIT_ZERO_SPACE_US = 2800;
static const size_t REMOTE_DATA_SIZE = 2 + 4 * (2 * 6 + 2 * 4 + 2 * 6 + 2);

void DishProtocol::encode(RemoteTransmitData *dst, const DishData &data) {
  dst->set_carrier_frequency(57600);
  dst->reserve(REMOTE_DATA_SIZE);

  // HEADER
  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  //  Typically a DISH device needs to get a command a total of
  //  at least 4 times to accept it.
  for (unsigned i = 0; i < 4; i++) {
    // COMMAND (function, 6 bits, in MSB)
    encode_data_msb<uint8_t, 6, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.command);

    // ADDRESS (unit code, 4 bits, in LSB)
    encode_data_lsb<uint8_t, 4, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(dst, data.address - 1);

    // PADDING (6 zeroes)
    for (unsigned j = 0; j < 6; j++)
      dst->item(BIT_MARK_US, BIT_ZERO_SPACE_US);

    // FOOTER
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  }
}

static bool decode_data(RemoteReceiveData &src, DishData &dst) {
  return decode_data_msb<uint8_t, 6, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.command) &&
         decode_data_lsb<uint8_t, 4, BIT_MARK_US, BIT_ONE_SPACE_US, BIT_ZERO_SPACE_US>(src, dst.address);
}

optional<DishData> DishProtocol::decode(RemoteReceiveData src) {
  DishData data{
      .address = 0,
      .command = 0,
  };

  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US) || !decode_data(src, data))
    return {};

  for (unsigned j = 0; j < 6; j++)
    if (!src.expect_item(BIT_MARK_US, BIT_ZERO_SPACE_US))
      return {};

  if (!src.expect_mark(HEADER_HIGH_US))
    return {};

  data.address++;
  return data;
}

void DishProtocol::dump(const DishData &data) {
  ESP_LOGD(TAG, "Received Dish: address=0x%02X, command=0x%02X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
