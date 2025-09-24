#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

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
  void encode(RemoteTransmitData *dst, const CanalSatData &data) override;
  optional<CanalSatData> decode(RemoteReceiveData src) override;
  void dump(const CanalSatData &data) override;

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

template<typename... Ts> class CanalSatAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, device)
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) {
    CanalSatData data{};
    data.device = this->device_.value(x...);
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    CanalSatProtocol().encode(dst, data);
  }
};

DECLARE_REMOTE_PROTOCOL(CanalSatLD)

template<typename... Ts> class CanalSatLDAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, device)
  TEMPLATABLE_VALUE(uint8_t, address)
  TEMPLATABLE_VALUE(uint8_t, command)

  void encode(RemoteTransmitData *dst, Ts... x) {
    CanalSatData data{};
    data.device = this->device_.value(x...);
    data.address = this->address_.value(x...);
    data.command = this->command_.value(x...);
    CanalSatLDProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
