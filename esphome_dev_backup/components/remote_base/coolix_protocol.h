#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

#include <cinttypes>

namespace esphome {
namespace remote_base {

struct CoolixData {
  CoolixData() {}
  CoolixData(uint32_t a) : first(a), second(a) {}
  CoolixData(uint32_t a, uint32_t b) : first(a), second(b) {}
  bool operator==(const CoolixData &other) const;
  bool is_strict() const { return this->first == this->second; }
  bool has_second() const { return this->second != 0; }
  uint32_t first;
  uint32_t second;
};

class CoolixProtocol : public RemoteProtocol<CoolixData> {
 public:
  void encode(RemoteTransmitData *dst, const CoolixData &data) override;
  optional<CoolixData> decode(RemoteReceiveData data) override;
  void dump(const CoolixData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Coolix)

template<typename... Ts> class CoolixAction : public RemoteTransmitterActionBase<Ts...> {
  TEMPLATABLE_VALUE(uint32_t, first)
  TEMPLATABLE_VALUE(uint32_t, second)
  void encode(RemoteTransmitData *dst, Ts... x) override {
    CoolixProtocol().encode(dst, {this->first_.value(x...), this->second_.value(x...)});
  }
};

}  // namespace remote_base
}  // namespace esphome
