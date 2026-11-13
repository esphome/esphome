#pragma once

#include "esphome/components/remote_base/keeloq_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::keeloq {

// The KeeLoq block cipher encryption: 32-bit block, 64-bit key as {bits 0-31, bits 32-63}, 528 rounds.
uint32_t encrypt(uint32_t data, const uint32_t key[2]);

enum KeeloqType : uint8_t {
  KEELOQ_TYPE_JAROLIFT,  // normal learning, 16 discrimination bits, 8 group bits after the word
};

class KeeloqRemote : public Component {
 public:
  // The device key is derived from the manufacturer key at build time.
  KeeloqRemote(KeeloqType type, uint32_t serial, uint32_t device_key_high, uint32_t device_key_low)
      : type_(type), serial_(serial), device_key_{device_key_low, device_key_high} {}

  void setup() override;
  void dump_config() override;

  // The next frame to send; advances the counter.
  remote_base::KeeloqData next_frame(uint8_t command, uint16_t channels);
  // Sends the next frame to the channels in the mask (bit 0 is channel 1); a mask of 0 sends nothing.
  void send(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels, uint32_t send_times,
            uint32_t send_wait);
  // The counter of the next frame.
  void set_counter(uint16_t counter);
  uint16_t get_counter() const { return this->counter_; }
  // Sends the pairing sequence; the receiver has to be in learn mode.
  void learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels);

 protected:
  void store_reserved_();

  KeeloqType type_;
  uint16_t counter_{0};
  // Counters below this one may have been sent; it is in flash before any of them is.
  uint16_t reserved_{0};
  // The channels of the pending learn, 0 when none; kept here so the timeout captures only 8 bytes.
  uint16_t learn_channels_{0};
  uint32_t serial_;
  uint32_t device_key_[2];
  ESPPreferenceObject pref_{};
};

}  // namespace esphome::keeloq
