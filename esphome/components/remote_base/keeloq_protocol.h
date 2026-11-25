#pragma once

#include "esphome/core/component.h"
#include "remote_base.h"

namespace esphome::remote_base {

struct KeeloqData {
  uint32_t encrypted;  // 32 bit encrypted field
  uint32_t address;    // 28 bit serial number
  uint8_t command;     // Button Status S2-S1-S0-S3
  bool repeat{true};   // Repeated command bit
  bool vlow;           // Battery status bit
  uint16_t suffix{0};  // Optional bits after the 66-bit HCS301 word
  uint8_t suffix_bits{0};

  // Jarolift TDEF sends 8 group bits after the 64-bit word where the HCS301 has vlow and repeat.
  void set_group(uint8_t group) {
    this->vlow = group & 0x01;
    this->repeat = group & 0x02;
    this->suffix = group >> 2;
    this->suffix_bits = 6;
  }
  // Only a Jarolift TDEF frame (suffix_bits == 6) carries a group; check suffix_bits before using it.
  uint8_t group() const { return static_cast<uint8_t>((this->suffix << 2) | (this->repeat << 1) | this->vlow); }

  bool operator==(const KeeloqData &rhs) const {
    // 0x10 matches any command. The suffix is not compared: the binary sensor does not take it.
    if (address != rhs.address) {
      return false;
    }
    return (rhs.command == 0x10 || command == rhs.command);
  }
};

class KeeloqProtocol : public RemoteProtocol<KeeloqData> {
 public:
  void encode(RemoteTransmitData *dst, const KeeloqData &data);
  optional<KeeloqData> decode(RemoteReceiveData src);
  void dump(const KeeloqData &data);
};

DECLARE_REMOTE_PROTOCOL(Keeloq)

}  // namespace esphome::remote_base
