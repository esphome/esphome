#pragma once

#include "remote_base.h"

namespace esphome::remote_base {

struct CanalSatData {
  uint8_t device : 7;
  uint8_t address : 6;
  uint8_t repeat : 1;
  uint8_t command : 7;

  bool operator==(const CanalSatData &rhs) const {
    return device == rhs.device && address == rhs.address && command == rhs.command;
  }
};

struct CanalSatLDData : public CanalSatData {};

class CanalSatBaseProtocol : public RemoteProtocol<CanalSatData> {
 public:
  void encode(RemoteTransmitData *dst, const CanalSatData &data);
  optional<CanalSatData> decode(RemoteReceiveData src);
  void dump(const CanalSatData &data);

 protected:
  uint16_t frequency_;
  uint16_t unit_;
  const char *tag_;
};

class CanalSatProtocol : public CanalSatBaseProtocol {
 public:
  CanalSatProtocol();
};

class CanalSatLDProtocol : public CanalSatBaseProtocol {
 public:
  CanalSatLDProtocol();
};

DECLARE_REMOTE_PROTOCOL(CanalSat)

DECLARE_REMOTE_PROTOCOL(CanalSatLD)

}  // namespace esphome::remote_base
