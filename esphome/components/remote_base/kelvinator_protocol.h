#pragma once

/*
  This code could not have been completed without the prior work located here:
  https://github.com/crankyoldgit/IRremoteESP8266/tree/master/src
*/

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "remote_base.h"

namespace esphome {
namespace remote_base {

class KelvinatorData {
 public:
  std::vector<uint64_t> data;
  bool isValidChecksum();
  void applyChecksum();
  uint8_t caclulateBlockChecksum(const uint64_t block);
  void log() const;
};

class KelvinatorProtocol : public RemoteProtocol<KelvinatorData> {
 public:
  void encode(RemoteTransmitData *dst, const KelvinatorData &data) override;
  optional<KelvinatorData> decode(RemoteReceiveData data) override;
  void dump(const KelvinatorData &data) override;

 private:
  void encode_data(RemoteTransmitData *dst, const uint32_t data);
  void encode_footer(RemoteTransmitData *dst);
  bool decode_data(RemoteReceiveData &src, uint64_t* data);
  bool decode_data(RemoteReceiveData &src, uint32_t* data);
  bool decode_footer(RemoteReceiveData &src);
};

DECLARE_REMOTE_PROTOCOL(Kelvinator)

}  // namespace remote_base
}  // namespace esphome
