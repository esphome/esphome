#pragma once

#include "esphome/components/remote_base/keeloq_protocol.h"
#include "esphome/components/remote_base/remote_base.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::keeloq {

// The KeeLoq block cipher: 32-bit block, 64-bit key as {bits 0-31, bits 32-63}, 528 rounds.
uint32_t encrypt(uint32_t data, const uint32_t key[2]);
uint32_t decrypt(uint32_t data, const uint32_t key[2]);

enum KeeloqType : uint8_t {
  KEELOQ_TYPE_JAROLIFT,  // normal learning, 16 discrimination bits, 8 group bits after the word
};

// A received frame that decrypted with the manufacturer key.
struct KeeloqFrame {
  uint32_t serial;
  uint16_t counter;
  uint16_t channels;
  uint8_t command;
};

class KeeloqRemote : public Component, public remote_base::RemoteReceiverListener {
 public:
  // The device key is derived from the manufacturer key at build time.
  KeeloqRemote(KeeloqType type, uint32_t serial, uint32_t device_key_high, uint32_t device_key_low)
      : type_(type), serial_(serial), device_key_{device_key_low, device_key_high} {}
  // With a receiver: the last accepted press is kept in flash too.
  void set_receiving() { this->receiving_ = true; }
  // Only without a serial or with on_frame: frames of other serials and a hand remote's serial need their device key at
  // run time.
  void set_manufacturer_key(uint32_t high, uint32_t low) {
    this->manufacturer_key_[0] = low;
    this->manufacturer_key_[1] = high;
    this->has_manufacturer_key_ = true;
  }
  // Without a serial: serial and counter come from a hand remote and are stored under this key.
  void set_take_over_key(uint32_t key) {
    this->take_over_key_ = key;
    this->take_over_ = true;
    this->known_ = false;
  }

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
  // Channels for actions that name none; a taken-over remote's own channels unless configured.
  void set_channels(uint16_t channels) {
    this->channels_ = channels;
    this->channels_configured_ = true;
  }
  // 0 while the remote has none.
  uint16_t get_channels() const { return this->channels_; }

 protected:
  bool decrypt_frame_(const remote_base::KeeloqData &data, KeeloqFrame &frame) const;
  // Applies the receiver's counter window; true for a frame that counts as a new press.
  bool accept_(uint16_t counter);
  void store_accepted_(bool sync);
  void store_reserved_();
  void use_serial_(uint32_t serial);

  LazyCallbackManager<void(KeeloqFrame)> frame_callback_;
  LazyCallbackManager<void(KeeloqFrame)> command_callback_;
  KeeloqType type_;
  bool take_over_{false};  // no serial in the configuration
  bool known_{true};       // a serial to send with
  bool waiting_{false};    // the take-over window is open
  uint16_t counter_{0};
  // Counters below this one may have been sent; it is in flash before any of them is.
  uint16_t reserved_{0};
  // The channels of the pending learn, 0 when none; kept here so the timeout captures only 8 bytes.
  uint16_t learn_channels_{0};
  uint16_t channels_{0};
  // The receiving side: the last counter accepted as a press, kept apart from the sending counter.
  uint16_t accepted_{0};
  uint16_t accepted_stored_{0};
  uint16_t resync_{0};  // a far-ahead counter; the next one in sequence resynchronizes
  bool channels_configured_{false};
  bool accepted_known_{false};
  bool resync_pending_{false};
  bool receiving_{false};
  bool has_manufacturer_key_{false};
  uint32_t serial_;
  uint32_t device_key_[2];
  uint32_t manufacturer_key_[2]{};
  uint32_t take_over_key_{0};
  ESPPreferenceObject pref_{};
  ESPPreferenceObject accepted_pref_{};
};

static constexpr uint8_t ANY_CHANNEL = 0;
static constexpr uint8_t ANY_COMMAND = 0xFF;

// Channel 1 to 16 is a bit of the frame's channels.
inline bool command_matches(const KeeloqFrame &frame, uint8_t channel, uint8_t command) {
  return (channel == ANY_CHANNEL || (frame.channels & (1 << (channel - 1))) != 0) &&
         (command == ANY_COMMAND || frame.command == command);
}

}  // namespace esphome::keeloq
