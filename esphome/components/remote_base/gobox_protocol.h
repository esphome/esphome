#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct GoboxData {
  int code;
  bool operator==(const GoboxData &rhs) const { return code == rhs.code; }
};

enum {
  GOBOX_MENU = 0xaa55,
  GOBOX_RETURN = 0x22dd,
  GOBOX_UP = 0x0af5,
  GOBOX_LEFT = 0x8a75,
  GOBOX_RIGHT = 0x48b7,
  GOBOX_DOWN = 0xa25d,
  GOBOX_OK = 0xc837,
  GOBOX_TOGGLE = 0xb847,
  GOBOX_PROFILE = 0xfa05,
  GOBOX_FASTER = 0xf00f,
  GOBOX_SLOWER = 0xd02f,
  GOBOX_LOUDER = 0xb04f,
  GOBOX_SOFTER = 0xf807,
};

class GoboxProtocol : public RemoteProtocol<GoboxData> {
 private:
  void dump_timings_(const RawTimings &timings) const;

 public:
  void encode(RemoteTransmitData *dst, const GoboxData &data) override;
  optional<GoboxData> decode(RemoteReceiveData src) override;
  void dump(const GoboxData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Gobox)

template<typename... Ts> class GoboxAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint64_t, code);

  void encode(RemoteTransmitData *dst, Ts... x) override {
    GoboxData data{};
    data.code = this->code_.value(x...);
    GoboxProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
