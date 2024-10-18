#pragma once

#include "remote_base.h"

namespace esphome {
namespace remote_base {

struct ElectraRC3Data {
  union {
    uint64_t q_word;
    struct {
      uint64_t zeros1 : 1;
      uint64_t ones1 : 1;
      uint64_t zeros2 : 16;
      uint64_t sleep : 1;
      uint64_t temperature : 4;
      uint64_t zeros3 : 1;
      uint64_t ifeel : 1;
      uint64_t swing : 1;
      uint64_t zeros4 : 2;
      uint64_t fan : 2;
      uint64_t mode : 3;
      uint64_t power : 1;
    };
  };

  ElectraRC3Data() : q_word(0) { ones1 = 1; }

  bool operator==(const ElectraRC3Data &rhs) const { return (q_word == rhs.q_word); }
};

class ElectraRC3Protocol : public RemoteProtocol<ElectraRC3Data> {
 public:
  void encode(RemoteTransmitData *dst, const ElectraRC3Data &data) override;
  optional<ElectraRC3Data> decode(RemoteReceiveData src) override;
  void dump(const ElectraRC3Data &data) override;
};

DECLARE_REMOTE_PROTOCOL(ElectraRC3)

template<typename... Ts> class ElectraRC3Action : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(uint64_t, power)
  TEMPLATABLE_VALUE(uint64_t, mode)
  TEMPLATABLE_VALUE(uint64_t, fan)
  TEMPLATABLE_VALUE(uint64_t, swing)
  TEMPLATABLE_VALUE(uint64_t, ifeel)
  TEMPLATABLE_VALUE(uint64_t, temperature)
  TEMPLATABLE_VALUE(uint64_t, sleep)

  void encode(RemoteTransmitData *dst, Ts... x) {
    ElectraRC3Data data{};
    data.mode = 0;
    data.power = this->power_.value(x...);
    data.mode = this->mode_.value(x...);
    data.fan = this->fan_.value(x...);
    data.swing = this->swing_.value(x...);
    data.ifeel = this->ifeel_.value(x...);
    data.temperature = this->temperature_.value(x...);
    data.sleep = this->sleep_.value(x...);
    ElectraRC3Protocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
