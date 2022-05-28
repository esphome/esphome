#include "samsung_protocol.h"
#include "esphome/core/log.h"
#include <cinttypes>

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.samsung";

static const uint32_t HEADER_HIGH_US = 4500;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;
static const uint32_t FOOTER_HIGH_US = 560;
static const uint32_t FOOTER_LOW_US = 560;

void SamsungProtocol::encode(RemoteTransmitData *dst, const SamsungData &data) {
  dst->set_carrier_frequency(38000);

  uint64_t raw{0};

  if (!data.data) {
    for (uint32_t mask = 1; mask < 0x10000; mask <<= 1) {
      raw = raw << 1 | ((data.address & mask) > 0);
    }
    for (uint32_t mask = 1; mask < 0x100; mask <<= 1) {
      raw = raw << 1 | ((data.command & mask) > 0);
    }
    for (uint32_t mask = 1; mask < 0x100; mask <<= 1) {
      raw = raw << 1 | ((data.command & mask) == 0);
    }
    for (uint32_t mask = 0x100; mask < 0x10000; mask <<= 1) {
      raw = raw << 1 | ((data.command & mask) > 0);
    }
    for (uint32_t mask = 0x100; mask < 0x10000; mask <<= 1) {
      raw = raw << 1 | ((data.command & mask) == 0);
    }
  } else {
    raw = data.data;
  }

  uint8_t length{32};
  if (raw > 0xFFFFFF) {
    length = 48;
  }

  dst->reserve(4 + length * 2u);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);

  for (uint8_t bit = length; bit > 0; bit--) {
    if ((raw >> (bit - 1)) & 1) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->item(FOOTER_HIGH_US, FOOTER_LOW_US);
}
optional<SamsungData> SamsungProtocol::decode(RemoteReceiveData src) {
  SamsungData data{
      .data = 0,
      .address = 0,
      .command = 0,
  };
  uint32_t command_buffer{0};

  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US)) {
    return {};
  }

  for (int i = 0; i < 16; i++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      data.address = (data.address >> 1) | 0x8000;
      data.data = (data.data << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      data.address = (data.address >> 1) | 0;
      data.data = (data.data << 1) | 0;
    } else {
      return {};
    }
  }

  for (int i = 0; i < 32; i++) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      command_buffer = (command_buffer >> 1) | 0x80000000;
      data.data = (data.data << 1) | 1;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      command_buffer = (command_buffer >> 1) | 0;
      data.data = (data.data << 1) | 0;
    } else if (i == 16) {
      if (!src.peek_mark(FOOTER_HIGH_US)) {
        return {};
      }
      command_buffer >>= 16;
      break;
    } else {
      return {};
    }
  }

  if (!src.expect_mark(FOOTER_HIGH_US)) {
    return {};
  }

  uint16_t mask{static_cast<uint16_t>(((0xFF000000 & command_buffer) >> 16) | ((0x0000FF00 & command_buffer) >> 8))};
  data.command = ((0x00FF0000 & command_buffer) >> 8) | ((0x000000FF & command_buffer));

  if ((data.command & mask) != 0) {
    return {};
  }

  return data;
}
void SamsungProtocol::dump(const SamsungData &data) {
  ESP_LOGD(TAG, "Received Samsung: address=0x%02X, command=0x%04X", data.address, data.command);
}

}  // namespace remote_base
}  // namespace esphome
