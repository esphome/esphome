#pragma once

#include "remote_base.h"

#include <cinttypes>

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8hpp_source.html
 */

namespace esphome::remote_base {

// Older decoders read the wand id 5 bits too early, so ids in older configs lack its 5 low bits.
static constexpr uint8_t MAGIQUEST_LEGACY_WAND_ID_SHIFT = 5;

struct MagiQuestData {
  uint16_t magnitude;
  uint32_t wand_id;

  // Not symmetric: `this` is the decoded frame and `rhs` the configured match. A configured wand_id of 0 matches
  // any wand. Magnitude is not compared; it appears to encode the cast type rather than a strength.
  bool operator==(const MagiQuestData &rhs) const {
    return rhs.wand_id == 0 || rhs.wand_id == this->wand_id ||
           rhs.wand_id == (this->wand_id >> MAGIQUEST_LEGACY_WAND_ID_SHIFT);
  }
};

class MagiQuestProtocol : public RemoteProtocol<MagiQuestData> {
 public:
  void encode(RemoteTransmitData *dst, const MagiQuestData &data);
  optional<MagiQuestData> decode(RemoteReceiveData src);
  void dump(const MagiQuestData &data);

 protected:
  static bool checksum_is_valid_(uint32_t wand_id, uint32_t magnitude_and_checksum);
};

DECLARE_REMOTE_PROTOCOL(MagiQuest)

template<typename... Ts> class MagiQuestAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint16_t, magnitude)
  TEMPLATABLE_VALUE(uint32_t, wand_id)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    MagiQuestData data{};
    data.magnitude = this->magnitude_.value(x...);
    data.wand_id = this->wand_id_.value(x...);
    MagiQuestProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
