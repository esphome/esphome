#include "aeha_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.aeha";

static const uint16_t BITWISE = 425;
static const uint16_t HEADER_HIGH_US = BITWISE * 8;
static const uint16_t HEADER_LOW_US = BITWISE * 4;
static const uint16_t BIT_HIGH_US = BITWISE;
static const uint16_t BIT_ONE_LOW_US = BITWISE * 3;
static const uint16_t BIT_ZERO_LOW_US = BITWISE;
static const uint16_t TRAILER = BITWISE;

void AEHAProtocol::encode(RemoteTransmitData *dst, const AEHAData &data) {
  dst->set_carrier_frequency(38000);
  dst->reserve(2 + 32 + (data.data.size() * 2) + 1);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint16_t mask = 1 << 15; mask != 0; mask >>= 1) {
    if (data.address & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint8_t bit : data.data) {
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

  for (uint8_t pos = 0; pos < 35; pos++) {
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

std::string AEHAProtocol::format_data_(const std::vector<uint8_t> &data) {
  std::string out;
  for (uint8_t byte : data) {
    char buf[6];
    sprintf(buf, "0x%02X,", byte);
    out += buf;
  }
  out.pop_back();
  return out;
}

void AEHAProtocol::dump(const AEHAData &data) {
  auto data_str = format_data_(data.data);
  ESP_LOGI(TAG, "Received AEHA: address=0x%04X, data=[%s]", data.address, data_str.c_str());
}

}  // namespace remote_base
}  // namespace esphome
