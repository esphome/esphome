#pragma once

#include "remote_base.h"

/* Based on protocol analysis from
 * https://arduino-irremote.github.io/Arduino-IRremote/ir__MagiQuest_8cpp_source.html
 */

namespace esphome {
namespace remote_base {

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
  void encode(RemoteTransmitData *dst, const MagiQuestData &data) override;
  optional<MagiQuestData> decode(RemoteReceiveData src) override;
  void dump(const MagiQuestData &data) override;
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
