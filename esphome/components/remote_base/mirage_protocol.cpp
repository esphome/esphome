#include "mirage_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.mirage";

constexpr uint32_t HEADER_MARK_US = 8360;
constexpr uint32_t HEADER_SPACE_US = 4248;
constexpr uint32_t BIT_MARK_US = 554;
constexpr uint32_t BIT_ONE_SPACE_US = 1592;
constexpr uint32_t BIT_ZERO_SPACE_US = 545;

constexpr unsigned int MIRAGE_IR_PACKET_BIT_SIZE = 120;

void MirageProtocol::encode(RemoteTransmitData *dst, const MirageData &data) {
  ESP_LOGI(TAG, "Transive Mirage: %s", format_hex_pretty(data.data).c_str());
  dst->set_carrier_frequency(38000);
  dst->reserve(5 + ((data.data.size() + 1) * 2));
  dst->mark(HEADER_MARK_US);
  dst->space(HEADER_SPACE_US);
  dst->mark(BIT_MARK_US);
  uint8_t checksum = 0;
  for (uint8_t item : data.data) {
    this->encode_byte_(dst, item);
    checksum += (item >> 4) + (item & 0xF);
  }
  this->encode_byte_(dst, checksum);
}

void MirageProtocol::encode_byte_(RemoteTransmitData *dst, uint8_t item) {
  for (uint8_t b = 0; b < 8; b++) {
    if (item & (1UL << b)) {
      dst->space(BIT_ONE_SPACE_US);
    } else {
      dst->space(BIT_ZERO_SPACE_US);
    }
    dst->mark(BIT_MARK_US);
  }
}

optional<MirageData> MirageProtocol::decode(RemoteReceiveData src) {
  if (!src.expect_item(HEADER_MARK_US, HEADER_SPACE_US)) {
    return {};
  }
  if (!src.expect_mark(BIT_MARK_US)) {
    return {};
  }
  size_t size = src.size() - src.get_index() - 1;
  if (size < MIRAGE_IR_PACKET_BIT_SIZE * 2)
    return {};
  size = MIRAGE_IR_PACKET_BIT_SIZE * 2;
  uint8_t checksum = 0;
  MirageData out;
  while (size > 0) {
    uint8_t data = 0;
    for (uint8_t b = 0; b < 8; b++) {
      if (src.expect_space(BIT_ONE_SPACE_US)) {
        data |= (1UL << b);
      } else if (!src.expect_space(BIT_ZERO_SPACE_US)) {
        return {};
      }
      if (!src.expect_mark(BIT_MARK_US)) {
        return {};
      }
      size -= 2;
    }
    if (size > 0) {
      checksum += (data >> 4) + (data & 0xF);
      out.data.push_back(data);
    } else if (checksum != data) {
      return {};
    }
  }
  return out;
}

void MirageProtocol::dump(const MirageData &data) {
  ESP_LOGI(TAG, "Received Mirage: %s", format_hex_pretty(data.data).c_str());
}

}  // namespace remote_base
}  // namespace esphome
