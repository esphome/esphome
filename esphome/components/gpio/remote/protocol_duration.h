#pragma once

#include "protocol_helper.h"

namespace esphome {

namespace gpio {

class RemoteProtocolCodecDuration : public RemoteProtocolCodec {
 public:
  RemoteProtocolCodecDuration() : RemoteProtocolCodec("duration") {}
  void encode(RemoteSignalData *dst, const std::vector<remote::arg_t> &args) override;

  // std::vector<arg_t> decode(const RemoteSignalData &src) override;

  void dump(const std::vector<remote::arg_t> &command) override;
};

}  // namespace gpio
}  // namespace esphome
