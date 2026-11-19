#include "dooya_protocol.h"
#include "esphome/core/log.h"

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.dooya");

static constexpr uint32_t HEADER_HIGH_US = 5000;
static constexpr uint32_t HEADER_LOW_US = 1500;
static constexpr uint32_t SHORT_US = 350;
static constexpr uint32_t LONG_US = 750;
static constexpr BitTimings BIT_TIMINGS{.one_space = SHORT_US,
                                        .zero_space = LONG_US,
                                        .bit_order = BitOrder::BIT_ORDER_MSB_FIRST,
                                        .one_mark = LONG_US,
                                        .zero_mark = SHORT_US};

void DooyaProtocol::encode(RemoteTransmitData *dst, const DooyaData &data) {
  dst->set_carrier_frequency(0);
  dst->reserve(2 + 40 * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  // 24 bit id, 8 bit channel, 4 bit button, 4 bit check
  const uint8_t bytes[5] = {static_cast<uint8_t>(data.id >> 16), static_cast<uint8_t>(data.id >> 8),
                            static_cast<uint8_t>(data.id), data.channel,
                            static_cast<uint8_t>((data.button & 0x0F) << 4 | (data.check & 0x0F))};
  dst->item_bytes<BIT_TIMINGS>(bytes, sizeof(bytes));
}
optional<DooyaData> DooyaProtocol::decode(RemoteReceiveData src) {
  uint8_t bytes[5];
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US) || !src.expect_bytes_last_mark<BIT_TIMINGS>(bytes, sizeof(bytes)))
    return {};
  return DooyaData{
      .id = encode_uint24(bytes[0], bytes[1], bytes[2]),
      .channel = bytes[3],
      .button = static_cast<uint8_t>(bytes[4] >> 4),
      .check = static_cast<uint8_t>(bytes[4] & 0x0F),
  };
}
void DooyaProtocol::dump(const DooyaData &data) {
  ESP_LOGI(TAG, "Received Dooya: id=0x%08" PRIX32 ", channel=%d, button=%d, check=%d", data.id, data.channel,
           data.button, data.check);
}

}  // namespace esphome::remote_base
