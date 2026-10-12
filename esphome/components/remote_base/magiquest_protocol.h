#pragma once

#include "remote_base.h"

#include <cinttypes>

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8cpp_source.html
 */

namespace esphome::remote_base {

struct MagiQuestData {
  uint16_t magnitude;
  uint32_t wand_id;

  bool operator==(const MagiQuestData &rhs) const {
    // Treat 0xffff as a special, wildcard magnitude
    // In testing, the wand never produces this value, and this allows us to match
    // on just the wand_id if wanted.
    if (rhs.wand_id != this->wand_id) {
      return false;
    }
    return (this->wand_id == 0xffff || rhs.wand_id == 0xffff || this->wand_id == rhs.wand_id);
  }
};

class MagiQuestProtocol : public RemoteProtocol<MagiQuestData> {
 public:
  void encode(RemoteTransmitData *dst, const MagiQuestData &data);
  optional<MagiQuestData> decode(RemoteReceiveData src);
  void dump(const MagiQuestData &data);
};

DECLARE_REMOTE_PROTOCOL(MagiQuest)

}  // namespace esphome::remote_base
