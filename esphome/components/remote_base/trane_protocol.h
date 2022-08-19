#pragma once

#include "esphome/core/components.h"
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

using TraneData = struct {
  unsigned trane_data_1: 35;
  unsigned trane_data_2: 32;
};

class TraneProtocol : public RemoteProtocol<TraneData>{
 public:
  void encode(RemoteTransmitData *dst, const TraneData &data) override;
  optional<TraneData> decode(RemoteReceiveData data) override;
  void dump(const TraneData &data) override;
};


}
}