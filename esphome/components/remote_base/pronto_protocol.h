#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

#include <vector>

namespace esphome {
namespace remote_base {

std::vector<uint16_t> encode_pronto(const std::string &str);

struct ProntoData {
  std::string data;
  int delta;

  bool operator==(const ProntoData &rhs) const;
};

class ProntoProtocol : public RemoteProtocol<ProntoData> {
 private:
  void send_pronto_(RemoteTransmitData *dst, const std::vector<uint16_t> &data);
  void send_pronto_(RemoteTransmitData *dst, const std::string &str);

  uint16_t effective_frequency_(uint16_t frequency);
  uint16_t to_timebase_(uint16_t frequency);
  uint16_t to_frequency_code_(uint16_t frequency);
  std::string dump_digit_(uint8_t x);
  std::string dump_number_(uint16_t number, bool end = false);
  std::string dump_duration_(uint32_t duration, uint16_t timebase, bool end = false);
  std::string compensate_and_dump_sequence_(const RawTimings &data, uint16_t timebase);

 public:
  void encode(RemoteTransmitData *dst, const ProntoData &data) override;
  optional<ProntoData> decode(RemoteReceiveData src) override;
  void dump(const ProntoData &data) override;
};

DECLARE_REMOTE_PROTOCOL(Pronto)

template<typename... Ts> class ProntoAction : public RemoteTransmitterActionBase<Ts...> {
 public:
  TEMPLATABLE_VALUE(std::string, data)
  TEMPLATABLE_VALUE(int, delta)

  void encode(RemoteTransmitData *dst, Ts... x) override {
    ProntoData data{};
    data.data = this->data_.value(x...);
    data.delta = this->delta_.value(x...);
    ProntoProtocol().encode(dst, data);
  }
};

}  // namespace remote_base
}  // namespace esphome
