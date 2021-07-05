#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct KeeloqData {
	uint32_t encrypted;
	uint32_t serial;
	uint8_t button;
	bool low;
	bool repeat;
  uint16_t sync = 0;

  bool operator==(const KeeloqData &rhs) const { return serial == rhs.serial && button == rhs.button; }
};

class KeeloqProtocol : public RemoteProtocol<KeeloqData> {
 public:
  void encode(RemoteTransmitData *dst, const KeeloqData &data) override;
  optional<KeeloqData> decode(RemoteReceiveData src) override;
  void dump(const KeeloqData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Keeloq)

template<typename... Ts> class KeeloqAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint32_t, encrypted)
  TEMPLATABLE_VALUE(uint32_t, serial)
  TEMPLATABLE_VALUE(uint8_t, button)
  TEMPLATABLE_VALUE(bool, low)
  TEMPLATABLE_VALUE(bool, repeat)
//  TEMPLATABLE_VALUE(uint16_t, sync)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    KeeloqData data{};
    data.encrypted = this->encrypted_.value(x...);
    data.serial = this->serial_.value(x...);
    data.button = this->button_.value(x...);
    data.low = this->low_.value(x...);
    data.repeat = this->repeat_.value(x...);
//    data.sync = this->sync_.value(x...);
    KeeloqProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
