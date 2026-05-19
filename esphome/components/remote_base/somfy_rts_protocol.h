#pragma once

#include "remote_base.h"

#include <cinttypes>

namespace esphome::remote_base {

struct SomfyRtsData {
  uint8_t key;
  uint8_t control;
  uint8_t checksum;
  uint16_t rolling_code;
  uint32_t address;
  bool control_in_key;

  bool operator==(const SomfyRtsData &rhs) const {
    return key == rhs.key && control == rhs.control && rhs.checksum == rhs.control &&
           rolling_code == rhs.rolling_code && address == rhs.address;
  }
};

class SomfyRtsProtocol : public RemoteProtocol<SomfyRtsData> {
 private:
  void wakeup(RemoteTransmitData *dst) const;
  void hw_sync(RemoteTransmitData *dst) const;
  void sw_sync(RemoteTransmitData *dst) const;

  // Helper functions for manchester encoding
  inline void zero(RemoteTransmitData *dst) const;
  inline void one(RemoteTransmitData *dst) const;

 public:
  void encode(RemoteTransmitData *dst, const SomfyRtsData &data) override;
  optional<SomfyRtsData> decode(RemoteReceiveData src) override;
  void dump(const SomfyRtsData &data) override;
};

DECLARE_REMOTE_PROTOCOL(SomfyRts)

template<typename... Ts> class SomfyRtsAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint8_t, key);
  TEMPLATABLE_VALUE(uint16_t, rolling_code);
  TEMPLATABLE_VALUE(uint32_t, address);
  TEMPLATABLE_VALUE(uint8_t, control);
  TEMPLATABLE_VALUE(bool, control_in_key);
  void encode(RemoteTransmitData *dst, Ts... x) override {
    SomfyRtsData data{};
    data.key = this->key_.value(x...);
    data.control = this->control_.value(x...);
    data.rolling_code = this->rolling_code_.value(x...);
    data.address = this->address_.value(x...);
    data.control_in_key = this->control_in_key_.value(x...);
    SomfyRtsProtocol().encode(dst, data);
  }
};

}  // namespace esphome::remote_base
