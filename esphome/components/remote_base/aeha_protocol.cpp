#include "aeha_protocol.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome::remote_base {

ESPHOME_LOG_TAG(TAG, "remote.aeha");

static constexpr uint16_t BITWISE = 425;
static constexpr uint16_t HEADER_HIGH_US = BITWISE * 8;
static constexpr uint16_t HEADER_LOW_US = BITWISE * 4;
static constexpr uint16_t BIT_HIGH_US = BITWISE;
static constexpr uint16_t BIT_ONE_LOW_US = BITWISE * 3;
static constexpr uint16_t BIT_ZERO_LOW_US = BITWISE;
static constexpr uint16_t TRAILER = BITWISE;

void AEHAProtocol::encode(RemoteTransmitData *dst, uint16_t address, const uint8_t *data, size_t len) {
  dst->reserve(2 + 32 + (len * 2) + 1);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint16_t mask = 1 << 15; mask != 0; mask >>= 1) {
    if (address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (size_t i = 0; i < len; i++) {
    const uint8_t bit = progmem_read_byte(data + i);
    for (uint8_t mask = 1 << 7; mask != 0; mask >>= 1) {
      if (bit & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }
  }

  dst->mark(TRAILER);
}
optional<AEHAData> AEHAProtocol::decode(RemoteReceiveData src) {
  AEHAData out{
      .address = 0,
      .data = {},
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint16_t mask = 1 << 15; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      out.address |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      out.address &= ~mask;
    } else {
      return {};
    }
  }

  for (uint8_t pos = 0; pos < AEHA_MAX_DATA_BYTES; pos++) {
    uint8_t data = 0;
    for (uint8_t mask = 1 << 7; mask != 0; mask >>= 1) {
      if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
        data |= mask;
      } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
        data &= ~mask;
      } else if (pos > 1 && src.expect_mark(TRAILER)) {
        return out;
      } else {
        return {};
      }
    }

    out.data.push_back(data);
  }

  if (src.expect_mark(TRAILER)) {
    return out;
  }

  return {};
}

void AEHAProtocol::dump(const AEHAData &data) {
  // "0xAB," per byte, the last comma replaced by the terminator
  char buf[AEHA_MAX_DATA_BYTES * 5];
  char *p = buf;
  for (uint8_t byte : data.data) {
    *p++ = '0';
    *p++ = 'x';
    *p++ = format_hex_pretty_char(byte >> 4);
    *p++ = format_hex_pretty_char(byte & 0x0F);
    *p++ = ',';
  }
  if (p != buf)
    p--;
  *p = '\0';
  ESP_LOGI(TAG, "Received AEHA: address=0x%04X, data=[%s]", data.address, buf);
}

}  // namespace esphome::remote_base
