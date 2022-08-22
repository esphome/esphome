#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

//using TraneData = struct {
//  unsigned mode: 3;
//  unsigned power_1: 1;
//  unsigned fan_mode: 2;
//  unsigned sweep_mode: 1;
//  unsigned sleep: 1;
//  unsigned temperature: 4;
//  unsigned unknown: 8;
//  unsigned turbo: 1;
//  unsigned unit_light: 1;
//  unsigned power_2: 1;
//  unsigned constant_1: 9;
//};

struct TraneData {
  uint64_t trane_data_1;
  uint64_t trane_data_2;
};

class TraneProtocol : public RemoteProtocol<TraneData>{
 public:
  void encode(RemoteTransmitData *dst, const TraneData &data) override;
  optional<TraneData> decode(RemoteReceiveData data) override;
  void dump(const TraneData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Trane)
template<typename... Ts> class TraneAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint64_t, trane_data_1)
  TEMPLATABLE_VALUE(uint32_t, trane_data_2)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    TraneData data{};
    data.trane_data_1 = this->trane_data_1_.value(x...);
    data.trane_data_2 = this->trane_data_2_.value(x...);
    TraneProtocol().encode(dst, data);
  }
};

}
}