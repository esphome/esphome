#pragma once

#include "esphome/components/remote_base/keeloq_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::keeloq {

// The KeeLoq block cipher: 32-bit block, 64-bit key, 528 rounds.
uint32_t encrypt(uint32_t data, uint64_t key);
uint32_t decrypt(uint32_t data, uint64_t key);

// How the device key and the hopping code are built. Each learning variant is one more value.
enum KeeloqType : uint8_t {
  KEELOQ_TYPE_JAROLIFT,  // normal learning, 16 discrimination bits, 8 group bits after the word
};

class KeeloqRemote : public Component {
 public:
  KeeloqRemote(KeeloqType type, uint64_t manufacturer_key, uint32_t serial);

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  // The next frame to send; advances the counter.
  remote_base::KeeloqData next_frame(uint8_t command, uint16_t channels);
  // The counter of the next frame.
  void set_counter(uint16_t counter);
  uint16_t get_counter() const { return this->counter_; }
  // How often a frame of this type is sent when the action sets no repeat.
  uint32_t default_send_times() const;
  // Sends the pairing sequence of this type; the receiver has to be in learn mode.
  void learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels, bool legacy);
  // Depends on the type and serial only, so the counter survives a new firmware.
  static uint32_t preference_key(KeeloqType type, uint32_t serial);

 protected:
  void store_reserved_();
  void send_(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels);
  void finish_learn_(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels);

  uint64_t device_key_;
  uint32_t serial_;
  ESPPreferenceObject pref_{};
  uint16_t counter_{0};
  // Counters below this one may have been sent; it is in flash before any of them is.
  uint16_t reserved_{0};
  KeeloqType type_;
};

template<typename... Ts> class SendAction : public remote_base::RemoteTransmitterActionBase<Ts...> {
 public:
  explicit SendAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint16_t, channels)

 protected:
  void play(const Ts &...x) override {
    auto call = this->transmitter_->transmit();
    this->encode(call.get_data(), x...);
    // Without repeat, the type decides how often a press is sent.
    call.set_send_times(this->send_times_.has_value() ? this->send_times_.value(x...)
                                                      : this->remote_->default_send_times());
    call.set_send_wait(this->send_wait_.value_or(x..., 0));
    call.perform();
  }
  void encode(remote_base::RemoteTransmitData *dst, Ts... x) override {
    remote_base::KeeloqProtocol().encode(
        dst, this->remote_->next_frame(this->command_.value(x...), this->channels_.value(x...)));
  }

  KeeloqRemote *remote_;
};

// Sends a sequence with a pause through the scheduler, so it is more than a forwarded value.
template<typename... Ts> class LearnAction : public remote_base::RemoteTransmittable, public Action<Ts...> {
 public:
  explicit LearnAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint16_t, channels)
  TEMPLATABLE_VALUE(bool, legacy)

  void play(const Ts &...x) override {
    this->remote_->learn(this->transmitter_, this->channels_.value(x...), this->legacy_.value_or(x..., false));
  }

 protected:
  KeeloqRemote *remote_;
};

}  // namespace esphome::keeloq
