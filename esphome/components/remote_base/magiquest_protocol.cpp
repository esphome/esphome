#include "magiquest_protocol.h"
#include "esphome/core/log.h"

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8cpp_source.html
 */

namespace esphome {
namespace remote_base {

static const char *const TAG = "remote.magiquest";

static const uint32_t MAGIQUEST_UNIT = 288;  // us
static const uint32_t MAGIQUEST_TOLERANCE = MAGIQUEST_UNIT / 2;
static const uint32_t MAGIQUEST_ONE_MARK = 2 * MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ONE_SPACE = 2 * MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ZERO_MARK = MAGIQUEST_UNIT;
static const uint32_t MAGIQUEST_ZERO_SPACE = 3 * MAGIQUEST_UNIT;

void MagiQuestProtocol::encode(RemoteTransmitData *dst, const MagiQuestData &data) {
  // This is still the "legacy" encoding - changes here risk breaking existing uses.

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
  // If the default tolerance is in play, override it with something that works better.
  if (src.get_tolerance_mode() == ToleranceMode::TOLERANCE_MODE_PERCENTAGE && src.get_tolerance() == 25) {
    src.set_tolerance(MAGIQUEST_TOLERANCE, ToleranceMode::TOLERANCE_MODE_TIME);
  }

  MagiQuestData data{
      .magnitude = 0,
      .wand_id = 0,
      .wand_id_legacy = 0,
  };

  // 8-bit header
  uint8_t header = 0;
  for (uint32_t mask = 1 << 7; mask; mask >>= 1) {
    if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      header |= mask;
    } else if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  // The header is expected to be all 0s.
  if (header) {
    return {};
  }

  // 31-bit wand_id
  for (uint32_t mask = 1 << 30; mask; mask >>= 1) {
    if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      data.wand_id |= mask;
    } else if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  // The "legacy" wand_id is part header part actual wand_id, and since the header is all 0s we can
  // easily apply a conversion by shifting.
  data.wand_id_legacy = data.wand_id >> 5;

  // 17-bit magnitude + checksum
  uint32_t magnitude_and_checksum = 0;
  for (uint32_t mask = 1 << 16; mask; mask >>= 1) {
    // Special case the final bit, because the space seems to get dropped
    if (mask == 1) {
      if (src.expect_mark(MAGIQUEST_ONE_MARK)) {
        magnitude_and_checksum |= mask;
      }
    } else if (src.expect_item(MAGIQUEST_ONE_MARK, MAGIQUEST_ONE_SPACE)) {
      magnitude_and_checksum |= mask;
    } else if (!src.expect_item(MAGIQUEST_ZERO_MARK, MAGIQUEST_ZERO_SPACE)) {
      return {};
    }
  }

  // Remove the checksum bits from the magnitude.
  data.magnitude = magnitude_and_checksum >> 8;

  // Validate the checksum.
  if (!checksum_is_valid_(data.wand_id, magnitude_and_checksum)) {
    return {};
  }

  return data;
}
void MagiQuestProtocol::dump(const MagiQuestData &data) {
  ESP_LOGI(TAG, "Received MagiQuest: wand_id=0x%08" PRIX32 ", magnitude=%d", data.wand_id, data.magnitude);
}
bool MagiQuestProtocol::checksum_is_valid_(uint32_t wand_id, uint32_t magnitude_and_checksum) {
  uint8_t checksum = 0;

  // shift the wand_id for the checksum calculation.
  wand_id <<= 1;
  uint8_t *wand_id_bytes = reinterpret_cast<uint8_t *>(&wand_id);
  for (size_t i = 0; i < sizeof(wand_id); i++) {
    checksum += wand_id_bytes[i];
  }

  // magnitudeAndChecksum can be used directly.
  uint8_t *magnitude_and_checksum_bytes = reinterpret_cast<uint8_t *>(&magnitude_and_checksum);
  for (size_t i = 0; i < sizeof(magnitude_and_checksum); i++) {
    checksum += magnitude_and_checksum_bytes[i];
  }

  return checksum == 0;
}

}  // namespace remote_base
}  // namespace esphome
