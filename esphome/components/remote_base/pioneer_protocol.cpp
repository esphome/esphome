#include "pioneer_protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.pioneer";

static const uint32_t HEADER_HIGH_US = 9000;
static const uint32_t HEADER_LOW_US = 4500;
static const uint32_t BIT_HIGH_US = 560;
static const uint32_t BIT_ONE_LOW_US = 1690;
static const uint32_t BIT_ZERO_LOW_US = 560;
static const uint32_t TRAILER_SPACE_US = 25500;

void PioneerProtocol::encode(RemoteTransmitData *dst, const PioneerData &data) {
  uint32_t address1 = ((data.rc_code_1 & 0xff00) | (~(data.rc_code_1 >> 8) & 0xff));
  uint32_t address2 = ((data.rc_code_2 & 0xff00) | (~(data.rc_code_2 >> 8) & 0xff));
  uint32_t command1 = 0;
  uint32_t command2 = 0;

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((data.rc_code_1 >> bit) & 1)
      command1 |= (1UL << (7 - bit));
  }

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((data.rc_code_1 >> (bit + 4)) & 1)
      command1 |= (1UL << (3 - bit));
  }

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((data.rc_code_2 >> bit) & 1)
      command2 |= (1UL << (7 - bit));
  }

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((data.rc_code_2 >> (bit + 4)) & 1)
      command2 |= (1UL << (3 - bit));
  }

  command1 = (command1 << 8) | ((~command1) & 0xff);
  command2 = (command2 << 8) | ((~command2) & 0xff);

  if (data.rc_code_2 == 0) {
    dst->reserve(68);
  } else {
    dst->reserve((68 * 2) + 1);
  }

  dst->set_carrier_frequency(40000);

  dst->item(HEADER_HIGH_US, HEADER_LOW_US);
  for (uint32_t mask = 1UL << 15; mask; mask >>= 1) {
    if (address1 & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  for (uint32_t mask = 1UL << 15; mask; mask >>= 1) {
    if (command1 & mask) {
      dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
    } else {
      dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
    }
  }

  dst->mark(BIT_HIGH_US);

  if (data.rc_code_2 != 0) {
    dst->space(TRAILER_SPACE_US);
    dst->item(HEADER_HIGH_US, HEADER_LOW_US);
    for (uint32_t mask = 1UL << 15; mask; mask >>= 1) {
      if (address2 & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }

    for (uint32_t mask = 1UL << 15; mask; mask >>= 1) {
      if (command2 & mask) {
        dst->item(BIT_HIGH_US, BIT_ONE_LOW_US);
      } else {
        dst->item(BIT_HIGH_US, BIT_ZERO_LOW_US);
      }
    }

    dst->mark(BIT_HIGH_US);
  }
}
optional<PioneerData> PioneerProtocol::decode(RemoteReceiveData src) {
  uint16_t address1 = 0;
  uint16_t command1 = 0;

  PioneerData data{
      .rc_code_1 = 0,
      .rc_code_2 = 0,
  };
  if (!src.expect_item(HEADER_HIGH_US, HEADER_LOW_US))
    return {};

  for (uint32_t mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      address1 |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      address1 &= ~mask;
    } else {
      return {};
    }
  }

  for (uint32_t mask = 1UL << 15; mask != 0; mask >>= 1) {
    if (src.expect_item(BIT_HIGH_US, BIT_ONE_LOW_US)) {
      command1 |= mask;
    } else if (src.expect_item(BIT_HIGH_US, BIT_ZERO_LOW_US)) {
      command1 &= ~mask;
    } else {
      return {};
    }
  }

  if (!src.expect_mark(BIT_HIGH_US))
    return {};

  if ((address1 >> 8) != ((~address1) & 0xff))
    return {};

  if ((command1 >> 8) != ((~command1) & 0xff))
    return {};

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((~command1 >> bit) & 1)
      data.rc_code_1 |= (1UL << (7 - bit));
  }

  for (uint32_t bit = 0; bit < 4; bit++) {
    if ((~command1 >> (bit + 4)) & 1)
      data.rc_code_1 |= (1UL << (3 - bit));
  }
  data.rc_code_1 |= address1 & 0xff00;

  return data;
}
void PioneerProtocol::dump(const PioneerData &data) {
  if (data.rc_code_2 == 0) {
    ESP_LOGI(TAG, "Received Pioneer: rc_code_X=0x%04X", data.rc_code_1);
  } else {
    ESP_LOGI(TAG, "Received Pioneer: rc_code_1=0x%04X, rc_code_2=0x%04X", data.rc_code_1, data.rc_code_2);
  }
}

}  // namespace remote_base
}  // namespace esphome
