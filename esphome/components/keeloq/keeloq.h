#pragma once

#include "esphome/components/remote_base/keeloq_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/automation.h"
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
  // The counter of the next frame.
  void set_counter(uint16_t counter);
  uint16_t get_counter() const { return this->counter_; }
  // Sends the pairing sequence; the receiver has to be in learn mode.
  void learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels);

 protected:
  void store_reserved_();
  void send_(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels);

  KeeloqType type_;
  uint16_t counter_{0};
  // Counters below this one may have been sent; it is in flash before any of them is.
  uint16_t reserved_{0};
  // Kept here so the learn timeout captures only 8 bytes and does not allocate.
  uint16_t learn_channels_{0};
  uint32_t serial_;
  uint32_t device_key_[2];
  ESPPreferenceObject pref_{};
};

template<typename... Ts> class SendAction : public remote_base::RemoteTransmitterActionBase<Ts...> {
 public:
  explicit SendAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint16_t, channels)

 protected:
  void encode(remote_base::RemoteTransmitData *dst, Ts... x) override {
    remote_base::KeeloqProtocol().encode(
        dst, this->remote_->next_frame(this->command_.value(x...), this->channels_.value(x...)));
  }

  KeeloqRemote *remote_;
};

template<typename... Ts> class LearnAction : public remote_base::RemoteTransmittable, public Action<Ts...> {
 public:
  explicit LearnAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint16_t, channels)

  void play(const Ts &...x) override { this->remote_->learn(this->transmitter_, this->channels_.value(x...)); }

 protected:
  KeeloqRemote *remote_;
};

}  // namespace esphome::keeloq
