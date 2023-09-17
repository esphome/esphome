#include "magiquest_protocol.h"
#include "esphome/core/log.h"

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8cpp_source.html
 */

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.magiquest";

static const uint32_t MAGIQUEST_UNIT = 288;  // us
static const uint32_t MAGIQUEST_ONE_MARK = 2 * MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ONE_SPACE = 2 * MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ZERO_MARK = MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ZERO_SPACE = 3 * MAGIQUEST_UNIT;

void MagiQuestProtocol::encode(RemoteTransmitData *dst, const MagiQuestData &data) {
  dst->reserve(101);  // 2 start bits, 48 data bits, 1 stop bit
  dst->set_carrier_frequency(38000);

  // 2 start bits
  dst->item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE);
  dst->item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE);
  for (uint32_t mask = 1 << 31; mask; mask >>= 1) {
    if (data.wand_id & mask) {
      dst->item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE);
    } else {
      dst->item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE);
    }
  }

  for (uint16_t mask = 1 << 15; mask; mask >>= 1) {
    if (data.magnitude & mask) {
      dst->item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE);
    } else {
      dst->item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE);
    }
  }

  dst->mark(MAGIQUEST_UNIT);
}
optional<MagiQuestData> MagiQuestProtocol::decode(RemoteReceiveData src) {
  MagiQuestData data{
      .magnitude = 0,
      .wand_id = 0,
  };
  // Two start bits
  if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE) ||
      !src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
    return {};
  }

  for (uint32_t mask = 1 << 31; mask; mask >>= 1) {
    if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      data.wand_id |= mask;
    } else if (src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      data.wand_id &= ~mask;
    } else {
      return {};
    }
  }

  for (uint16_t mask = 1 << 15; mask; mask >>= 1) {
    if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      data.magnitude |= mask;
    } else if (src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      data.magnitude &= ~mask;
    } else {
      return {};
    }
  }

  src.expect_mark(MAGIQUEST_UNIT);
  return data;
}
void MagiQuestProtocol::dump(const MagiQuestData &data) {
  ESP_LOGI(TAG, "Received MagiQuest: wand_id=0x%08X, magnitude=0x%04X", data.wand_id, data.magnitude);
}

}  // namespace remote_base
}  // namespace esphome
