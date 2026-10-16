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

// A received frame that decrypted with the manufacturer key.
struct KeeloqFrame {
  uint32_t serial;
  uint16_t counter;
  uint16_t channels;
  uint8_t command;
  KeeloqType type;
};

class KeeloqRemote : public Component, public remote_base::RemoteReceiverListener {
 public:
  KeeloqRemote(KeeloqType type, uint64_t manufacturer_key, uint32_t serial);
  // Without a serial: serial and counter come from a hand remote and are stored under this key.
  void set_take_over_key(uint32_t key) {
    this->take_over_key_ = key;
    this->take_over_ = true;
    this->known_ = false;
  }

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

  // Frames that decrypt with the manufacturer key; frames of this serial that are ahead move the counter.
  bool on_receive(remote_base::RemoteReceiveData src) override;
  template<typename F> void add_on_frame_callback(F &&callback) {
    this->frame_callback_.add(std::forward<F>(callback));
  }
  // Frames of this serial with a new counter, once per press, as a KeeLoq receiver accepts them.
  template<typename F> void add_on_command_callback(F &&callback) {
    this->command_callback_.add(std::forward<F>(callback));
  }
  // The next valid frame within a minute gives the counter; a remote without serial takes the serial too.
  void take_over_next_frame();
  // False, with a warning, while a remote without serial has not taken one over.
  bool ready_to_send() const;
  uint32_t get_serial() const { return this->serial_; }
  KeeloqType get_type() const { return this->type_; }
  // Channels for frames whose action names none; a taken-over remote's own channels unless configured.
  void set_channels(uint16_t channels) {
    this->channels_ = channels;
    this->channels_configured_ = true;
    this->channels_known_ = true;
  }
  uint16_t get_channels() const { return this->channels_; }
  // The action's channels, else the remote's; false, with a warning, if a frame needs one and there is none.
  bool resolve_channels(bool has_action_channels, uint16_t action_channels, uint16_t &channels) const;

 protected:
  bool decrypt_frame_(const remote_base::KeeloqData &data, KeeloqFrame &frame) const;
  bool decrypt_frame_as_(KeeloqType type, const remote_base::KeeloqData &data, KeeloqFrame &frame) const;
  // Applies the receiver's counter window; true for a frame that counts as a new press.
  bool accept_(uint16_t counter);
  void store_accepted_(bool sync);
  void store_reserved_();
  void use_serial_(uint32_t serial);
  void send_(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels);
  void finish_learn_(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels);

  LazyCallbackManager<void(KeeloqFrame)> frame_callback_;
  LazyCallbackManager<void(KeeloqFrame)> command_callback_;
  uint64_t manufacturer_key_;
  uint64_t device_key_;
  uint32_t serial_;
  ESPPreferenceObject pref_{};
  ESPPreferenceObject accepted_pref_{};
  uint32_t take_over_key_{0};
  uint16_t counter_{0};
  // Counters below this one may have been sent; it is in flash before any of them is.
  uint16_t reserved_{0};
  uint16_t channels_{0};
  // The receiving side: the last counter accepted as a press, kept apart from the sending counter.
  uint16_t accepted_{0};
  uint16_t accepted_stored_{0};
  uint16_t resync_{0};  // a far-ahead counter; the next one in sequence resynchronizes
  KeeloqType type_;
  bool channels_configured_{false};
  bool channels_known_{false};  // configured or taken over
  bool accepted_known_{false};
  bool resync_pending_{false};
  bool take_over_{false};  // no serial in the configuration
  bool known_{true};       // a serial to send with
  bool waiting_{false};    // the take-over window is open
};

static constexpr uint8_t ANY_CHANNEL = 0;
static constexpr uint8_t ANY_COMMAND = 0xFF;

// Channel 1 to 16 is a bit of the frame's channels.
inline bool command_matches(const KeeloqFrame &frame, uint8_t channel, uint8_t command) {
  return (channel == ANY_CHANNEL || (frame.channels & (1 << (channel - 1))) != 0) &&
         (command == ANY_COMMAND || frame.command == command);
}

template<typename... Ts> class SendAction : public remote_base::RemoteTransmitterActionBase<Ts...> {
 public:
  explicit SendAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint8_t, command)
  TEMPLATABLE_VALUE(uint16_t, channels)

 protected:
  void play(const Ts &...x) override {
    bool has_channels = this->channels_.has_value();
    if (!this->remote_->ready_to_send() ||
        !this->remote_->resolve_channels(has_channels, has_channels ? this->channels_.value(x...) : 0,
                                         this->resolved_channels_)) {
      return;
    }
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
        dst, this->remote_->next_frame(this->command_.value(x...), this->resolved_channels_));
  }

  uint16_t resolved_channels_{0};

  KeeloqRemote *remote_;
};

// Sends a sequence with a pause through the scheduler, so it is more than a forwarded value.
template<typename... Ts> class LearnAction : public remote_base::RemoteTransmittable, public Action<Ts...> {
 public:
  explicit LearnAction(KeeloqRemote *remote) : remote_(remote) {}
  TEMPLATABLE_VALUE(uint16_t, channels)
  TEMPLATABLE_VALUE(bool, legacy)

  void play(const Ts &...x) override {
    bool has_channels = this->channels_.has_value();
    uint16_t channels;
    if (this->remote_->resolve_channels(has_channels, has_channels ? this->channels_.value(x...) : 0, channels)) {
      this->remote_->learn(this->transmitter_, channels, this->legacy_.value_or(x..., false));
    }
  }

 protected:
  KeeloqRemote *remote_;
};

}  // namespace esphome::keeloq
