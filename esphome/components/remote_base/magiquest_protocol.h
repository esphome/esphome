#pragma once

#include "remote_base.h"

#include <cinttypes>

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8hpp_source.html
 */

namespace esphome {
namespace remote_base {

struct MagiQuestData {
  uint16_t magnitude;
  uint32_t wand_id;
  uint32_t wand_id_legacy;

  bool operator==(const MagiQuestData &rhs) const {
    // The "legacy" implementation only matched on wand_id, so do that first.
    if (rhs.wand_id == this->wand_id_legacy) {
      return true;
    }

    // If a wand_id was specified, and it's not the current wand, do not match.
    if (rhs.wand_id != 0 && rhs.wand_id != this->wand_id) {
      return false;
    }

    // Otherwise, if have the right wand (or any wand is acceptable), apply the
    // magnitude threshold.
    return this->magnitude >= rhs.magnitude;
  }
};

class MagiQuestProtocol : public RemoteProtocol<MagiQuestData> {
 public:
  void encode(RemoteTransmitData *dst, const MagiQuestData &data) override;
  optional<MagiQuestData> decode(RemoteReceiveData src) override;
  void dump(const MagiQuestData &data) override;

 private:
  bool checksum_is_valid_(uint32_t wand_id, uint32_t magnitude_and_checksum);
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

}  // namespace remote_base
}  // namespace esphome
