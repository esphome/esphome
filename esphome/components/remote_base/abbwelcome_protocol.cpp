#include "abbwelcome_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.abbwelcome";

static const uint32_t BIT_ONE_SPACE_US = 102;
static const uint32_t BIT_ZERO_MARK_US = 32;  // 18-44
static const uint32_t BIT_ZERO_SPACE_US = BIT_ONE_SPACE_US - BIT_ZERO_MARK_US;
static const uint16_t BYTE_SPACE_US = 210;

uint8_t ABBWelcomeData::calc_cs_() const {
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < this->size() - 1; i++) {
    uint16_t temp = checksum ^ (this->data_[i]);
    temp = temp ^ (uint16_t) (((uint32_t) temp << 0x11) >> 0x10) ^ (uint16_t) (((uint32_t) temp << 0x12) >> 0x10) ^
           (uint16_t) (((uint32_t) temp << 0x13) >> 0x10) ^ (uint16_t) (((uint32_t) temp << 0x14) >> 0x10) ^
           (uint16_t) (((uint32_t) temp << 0x15) >> 0x10) ^ (uint16_t) (((uint32_t) temp << 0x16) >> 0x10) ^
           (uint16_t) (((uint32_t) temp << 0x17) >> 0x10);
    checksum = (temp & 0xfe) ^ ((temp >> 8) & 1);
  }
  return ~checksum;
}

void ABBWelcomeProtocol::encode_byte_(RemoteTransmitData *dst, uint8_t data) const {
  // space = bus high, mark = activate bus pulldown
  dst->mark(BIT_ZERO_MARK_US);
  uint32_t next_space = BIT_ZERO_SPACE_US;
  for (uint8_t mask = 1 << 7; mask; mask >>= 1) {
    if (data & mask) {
      next_space += BIT_ONE_SPACE_US;
    } else {
      dst->space(next_space);
      dst->mark(BIT_ZERO_MARK_US);
      next_space = BIT_ZERO_SPACE_US;
    }
  }
  next_space += BYTE_SPACE_US;
  dst->space(next_space);
}

void ABBWelcomeProtocol::encode(RemoteTransmitData *dst, const ABBWelcomeData &src) {
  dst->set_carrier_frequency(0);
  uint32_t reserve_count = 0;
  for (size_t i = 0; i < src.size(); i++) {
    reserve_count += 2 * (9 - (src[i] & 1) - ((src[i] >> 1) & 1) - ((src[i] >> 2) & 1) - ((src[i] >> 3) & 1) -
                          ((src[i] >> 4) & 1) - ((src[i] >> 5) & 1) - ((src[i] >> 6) & 1) - ((src[i] >> 7) & 1));
  }
  dst->reserve(reserve_count);
  for (size_t i = 0; i < src.size(); i++)
    this->encode_byte_(dst, src[i]);
  ESP_LOGD(TAG, "Transmitting: %s", src.to_string().c_str());
}

bool ABBWelcomeProtocol::decode_byte_(RemoteReceiveData &src, bool &done, uint8_t &data) {
  if (!src.expect_mark(BIT_ZERO_MARK_US))
    return false;
  uint32_t next_space = BIT_ZERO_SPACE_US;
  for (uint8_t mask = 1 << 7; mask; mask >>= 1) {
    // if (!src.peek_space_at_least(next_space, 0))
    //   return false;
    if (src.expect_space(next_space)) {
      if (!src.expect_mark(BIT_ZERO_MARK_US))
        return false;
      next_space = BIT_ZERO_SPACE_US;
    } else {
      data |= mask;
      next_space += BIT_ONE_SPACE_US;
    }
  }
  next_space += BYTE_SPACE_US;
  // if (!src.peek_space_at_least(next_space, 0))
  //   return false;
  done = !(src.expect_space(next_space));
  return true;
}

optional<ABBWelcomeData> ABBWelcomeProtocol::decode(RemoteReceiveData src) {
  if (src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US) &&
      src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US + BIT_ONE_SPACE_US) &&
      src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US + BIT_ONE_SPACE_US) &&
      src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US + BIT_ONE_SPACE_US) &&
      src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US + BIT_ONE_SPACE_US + BYTE_SPACE_US) &&
      src.expect_item(BIT_ZERO_MARK_US, BIT_ZERO_SPACE_US + 8 * BIT_ONE_SPACE_US + BYTE_SPACE_US)) {
    ESP_LOGVV(TAG, "Received Header: 0x55FF");
    ABBWelcomeData out;
    out[0] = 0x55;
    out[1] = 0xff;
    bool done = false;
    uint8_t length = 10;
    uint8_t received_bytes = 2;
    for (; (received_bytes < length) && !done; received_bytes++) {
      uint8_t data = 0;
      if (!this->decode_byte_(src, done, data)) {
        ESP_LOGW(TAG, "Received incomplete packet: %s", out.to_string(received_bytes).c_str());
        return {};
      }
      if (received_bytes == 2) {
        length += std::min(static_cast<uint8_t>(data & DATA_LENGTH_MASK), MAX_DATA_LENGTH);
        if (data & 0x40) {
          length += 2;
        }
      }
      ESP_LOGVV(TAG, "Received Byte: 0x%02X", data);
      out[received_bytes] = data;
    }
    if (out.is_valid()) {
      ESP_LOGI(TAG, "Received: %s", out.to_string().c_str());
      return out;
    }
    ESP_LOGW(TAG, "Received malformed packet: %s", out.to_string(received_bytes).c_str());
  }
  return {};
}

void ABBWelcomeProtocol::dump(const ABBWelcomeData &data) {
  ESP_LOGD(TAG, "Received ABBWelcome: %s", data.to_string().c_str());
}

}  // namespace remote_base
}  // namespace esphome
