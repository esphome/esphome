#include "haier_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.haier";

constexpr uint32_t HEADER_LOW_US = 3100;
constexpr uint32_t HEADER_HIGH_US = 4400;
constexpr uint32_t BIT_MARK_US = 540;
constexpr uint32_t BIT_ONE_SPACE_US = 1650;
constexpr uint32_t BIT_ZERO_SPACE_US = 580;
constexpr unsigned int HAIER_IR_PACKET_BIT_SIZE = 112;

void HaierProtocol::encode_byte_(RemoteTransmitData *dst, uint8_t item) {
  for (uint8_t mask = 1 << 7; mask != 0; mask >>= 1) {
    if (item & mask) {
      dst->space(BIT_ONE_SPACE_US);
    } else {
      dst->space(BIT_ZERO_SPACE_US);
    }
    dst->mark(BIT_MARK_US);
  }
}

void HaierProtocol::encode(RemoteTransmitData *dst, const HaierData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(5 + ((data.data.size() + 1) * 2));
  dst->mark(HEADER_LOW_US);
  dst->space(HEADER_LOW_US);
  dst->mark(HEADER_LOW_US);
  dst->space(HEADER_HIGH_US);
  dst->mark(BIT_MARK_US);
  uint8_t checksum = 0;
  for (uint8_t item : data.data) {
    this->encode_byte_(dst, item);
    checksum += item;
  }
  this->encode_byte_(dst, checksum);
}

optional<HaierData> HaierProtocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(HEADER_LOW_US, HEADER_LOW_US) || !src.expect_item(HEADER_LOW_US, HEADER_HIGH_US)) {
    return {};
  }
  if (!src.expect_mark(BIT_MARK_US)) {
    return {};
  }
  size_t size = src.size() - src.get_index() - 1;
  if (size < HAIER_IR_PACKET_BIT_SIZE * 2)
    return {};
  size = HAIER_IR_PACKET_BIT_SIZE * 2;
  uint8_t checksum = 0;
  HaierData out;
  while (size > 0) {
    uint8_t data = 0;
    for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
      if (src.expect_space(BIT_ONE_SPACE_US)) {
        data |= mask;
      } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
        return {};
      }
      if (!src.expect_mark(BIT_MARK_US)) {
        return {};
      }
      size -= 2;
    }
    if (size > 0) {
      checksum += data;
      out.data.push_back(data);
    } else if (checksum != data) {
      return {};
    }
  }
  return out;
}

void HaierProtocol::dump(const HaierData &data) {
  ESP_LOGI(TAG, "Received Haier: %s", format_hex_pretty(data.data).c_str());
}

}  // namespace remote_base
}  // namespace esphome
