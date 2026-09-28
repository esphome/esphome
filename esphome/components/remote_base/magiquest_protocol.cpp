#include "magiquest_protocol.h"
#include "esphome/core/log.h"

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8hpp_source.html
 */

namespace esphome::remote_base {

static const char *const TAG = "remote.magiquest";

static constexpr uint32_t MAGIQUEST_UNIT = 288;  // us
// Half a unit is the widest window that still tells a one unit mark or space from a two unit one.
static constexpr uint32_t MAGIQUEST_TOLERANCE = MAGIQUEST_UNIT / 2;
static constexpr uint32_t MAGIQUEST_ONE_MARK = 2 * MAGIQUEST_UNIT;
static constexpr uint32_t MAGIQUEST_ONE_SPACE = 2 * MAGIQUEST_UNIT;
static constexpr uint32_t MAGIQUEST_ZERO_MARK = MAGIQUEST_UNIT;
static constexpr uint32_t MAGIQUEST_ZERO_SPACE = 3 * MAGIQUEST_UNIT;

void MagiQuestProtocol::encode(RemoteTransmitData *dst, const MagiQuestData &data) {
  // Still the old frame layout, which the decoder no longer reads; changing it would alter what existing configs send.
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
  src.set_tolerance(MAGIQUEST_TOLERANCE, TOLERANCE_MODE_TIME);

  MagiQuestData data{
      .magnitude = 0,
      .wand_id = 0,
  };

  // 8 bit header, always zero
  for (uint8_t i = 0; i < 8; i++) {
    if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  // 31 bit wand id
  for (uint32_t mask = 1 << 30; mask; mask >>= 1) {
    if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      data.wand_id |= mask;
    } else if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  // 9 bit magnitude, then 8 bit checksum
  uint32_t magnitude_and_checksum = 0;
  for (uint32_t mask = 1 << 16; mask; mask >>= 1) {
    if (mask == 1) {
      // The last bit has no trailing space. A wrong or missing mark here is caught by the checksum.
      if (src.expect_mark(MAGIQUEST_ONE_MARK)) {
        magnitude_and_checksum |= mask;
      }
    } else if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      magnitude_and_checksum |= mask;
    } else if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  if (!checksum_is_valid_(data.wand_id, magnitude_and_checksum)) {
    return {};
  }
  data.magnitude = magnitude_and_checksum >> 8;
  return data;
}
void MagiQuestProtocol::dump(const MagiQuestData &data) {
  ESP_LOGI(TAG, "Received MagiQuest: wand_id=0x%08" PRIX32 ", magnitude=%u", data.wand_id, data.magnitude);
}
bool MagiQuestProtocol::checksum_is_valid_(uint32_t wand_id, uint32_t magnitude_and_checksum) {
  // The frame's six payload bytes, which start at the wand id's top bit, must sum to zero.
  const uint32_t id_bytes = wand_id << 1;
  uint8_t sum = 0;
  for (uint8_t shift = 0; shift < 32; shift += 8) {
    sum += (id_bytes >> shift) + (magnitude_and_checksum >> shift);
  }
  return sum == 0;
}

}  // namespace esphome::remote_base
