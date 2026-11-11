#include "keeloq.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::keeloq {

ESPHOME_LOG_TAG(TAG, "keeloq");

static constexpr uint32_t NLF = 0x3A5C742E;
static constexpr uint16_t ROUNDS = 528;
// Fewer than the 16 codes a receiver accepts ahead, so the gap after a reboot stays inside that window.
static constexpr uint16_t RESERVE_BLOCK = 8;
static constexpr uint32_t PREF_HASH = fnv1a_hash("keeloq");
static constexpr uint32_t JAROLIFT_SEND_TIMES = 2;
static constexpr uint8_t JAROLIFT_STOP = 0x4;
static constexpr uint8_t JAROLIFT_UP_DOWN = 0xA;
static constexpr uint32_t JAROLIFT_LEARN_PAUSE_MS = 1000;
static constexpr uint32_t TAKE_OVER_TIMEOUT_MS = 60000;
// Scheduler ids.
static constexpr uint32_t LEARN_TIMEOUT_ID = 0;
static constexpr uint32_t TAKE_OVER_TIMEOUT_ID = 1;
// HCS301 data sheet DS21143C, section 7.3: a receiver accepts 16 codes ahead with one press, 32K with two.
static constexpr uint16_t SINGLE_WINDOW = 16;
static constexpr uint16_t RESYNC_WINDOW = 0x8000;

static inline uint32_t get_bit(uint32_t value, uint32_t n) { return (value >> n) & 1; }

uint32_t encrypt(uint32_t data, const uint32_t key[2]) {
  uint32_t x = data;
  for (uint16_t r = 0; r < ROUNDS; r++) {
    uint32_t nlf = get_bit(
        NLF, get_bit(x, 1) | get_bit(x, 9) << 1 | get_bit(x, 20) << 2 | get_bit(x, 26) << 3 | get_bit(x, 31) << 4);
    x = (x >> 1) | ((get_bit(x, 0) ^ get_bit(x, 16) ^ nlf ^ get_bit(key[(r >> 5) & 1], r & 31)) << 31);
  }
  return x;
}

uint32_t decrypt(uint32_t data, const uint32_t key[2]) {
  uint32_t x = data;
  for (uint16_t r = 0; r < ROUNDS; r++) {
    uint32_t nlf = get_bit(
        NLF, get_bit(x, 0) | get_bit(x, 8) << 1 | get_bit(x, 19) << 2 | get_bit(x, 25) << 3 | get_bit(x, 30) << 4);
    const uint32_t k = 15u - r;
    x = (x << 1) | (get_bit(x, 31) ^ get_bit(x, 15) ^ nlf ^ get_bit(key[(k >> 5) & 1], k & 31));
  }
  return x;
}

// Microchip normal learning: the serial decrypted twice with the manufacturer key, as codegen does for a set serial.
static void normal_learning_key(uint32_t serial, const uint32_t manufacturer_key[2], uint32_t key[2]) {
  serial &= 0x0FFFFFFF;
  key[1] = decrypt(serial | 0x60000000, manufacturer_key);
  key[0] = decrypt(serial | 0x20000000, manufacturer_key);
}

// Depends on the type and serial only, so the counter survives a new firmware.
static uint32_t preference_key(KeeloqType type, uint32_t serial) {
  return fnv1a_hash_extend(fnv1a_hash_extend(PREF_HASH, static_cast<uint8_t>(type)), serial);
}

// The schema rejects a mask of 0, but a channels lambda can still return it.
static bool no_channels(uint16_t channels) {
  if (channels != 0)
    return false;
  ESP_LOGW(TAG, "No channel set, nothing sent");
  return true;
}

void KeeloqRemote::use_serial_(uint32_t serial) {
  this->serial_ = serial;
  normal_learning_key(serial, this->manufacturer_key_, this->device_key_);
  this->known_ = true;
}

bool KeeloqRemote::ready_to_send() const {
  if (!this->known_) {
    ESP_LOGW(TAG, "No serial yet: take over a hand remote first");
  }
  return this->known_;
}

bool KeeloqRemote::decrypt_frame_(const remote_base::KeeloqData &data, KeeloqFrame &frame) const {
  uint32_t key[2] = {this->device_key_[0], this->device_key_[1]};
  if (!this->known_ || data.address != this->serial_) {
    // A remote with serial and without on_frame has no manufacturer key, so it hears only its own serial.
    if (!this->has_manufacturer_key_) {
      return false;
    }
    normal_learning_key(data.address, this->manufacturer_key_, key);
  }
  uint32_t plain = decrypt(data.encrypted, key);
  // A wrong key gives random bits; the serial's low byte inside the code proves the right one.
  if (data.suffix_bits != 6 || ((plain >> 16) & 0xFF) != (data.address & 0xFF)) {
    return false;
  }
  frame.channels = data.group() << 8 | plain >> 24;
  frame.serial = data.address;
  frame.counter = plain & 0xFFFF;
  frame.command = data.command;
  return true;
}

bool KeeloqRemote::on_receive(remote_base::RemoteReceiveData src) {
  auto data = remote_base::KeeloqProtocol().decode(src);
  KeeloqFrame frame{};
  if (!data.has_value() || !this->decrypt_frame_(*data, frame)) {
    return false;
  }
  ESP_LOGV(TAG, "Received serial 0x%07" PRIX32 " counter %u channels 0x%04X command 0x%X", frame.serial, frame.counter,
           frame.channels, frame.command);
  // KeeLoq receivers compare counters modulo 2^16; ahead means less than half the range.
  uint16_t ahead = frame.counter - this->counter_;
  // With a serial only its own frames count; without one the first valid frame gives the serial too.
  bool took_over = this->waiting_ && (this->take_over_ || frame.serial == this->serial_);
  if (took_over) {
    this->waiting_ = false;
    this->cancel_timeout(TAKE_OVER_TIMEOUT_ID);
    // Without serial the hand remote gives the serial and, unless configured, the channels; with serial only the
    // counter, and the device key from the build stays.
    if (this->take_over_) {
      this->use_serial_(frame.serial);
      if (!this->channels_configured_) {
        this->channels_ = frame.channels;
      }
    }
    this->set_counter(frame.counter + 1);
    // The take-over press itself does not count as a command.
    this->accepted_ = frame.counter;
    this->accepted_known_ = true;
    this->store_accepted_(true);
    ESP_LOGI(TAG, "Took over serial 0x%07" PRIX32 ", channels 0x%04X, next counter %u", this->serial_, this->channels_,
             this->counter_);
  } else if (this->known_ && frame.serial == this->serial_ && ahead < 0x8000) {
    // No flash write here: the next frame sent reserves its block first.
    this->counter_ = frame.counter + 1;
  }
  this->frame_callback_.call(frame);
  if (took_over) {
    return true;
  }
  if (this->known_ && frame.serial == this->serial_ && this->accept_(frame.counter)) {
    this->command_callback_.call(frame);
  }
  return true;
}

bool KeeloqRemote::accept_(uint16_t counter) {
  // Compared with the last accepted counter: equal is a copy, ahead within the window is new, else old.
  uint16_t ahead = counter - this->accepted_;
  if (this->accepted_known_) {
    if (ahead == 0) {
      ESP_LOGVV(TAG, "Copy of counter %u", counter);
      return false;
    }
    if (ahead >= RESYNC_WINDOW) {
      ESP_LOGD(TAG, "Old counter %u, last %u: possible replay", counter, this->accepted_);
      this->resync_pending_ = false;
      return false;
    }
    if (ahead > SINGLE_WINDOW) {
      // Far ahead: only the next counter after this one resynchronizes.
      bool in_sequence = this->resync_pending_ && counter == static_cast<uint16_t>(this->resync_ + 1);
      this->resync_pending_ = !in_sequence;
      this->resync_ = counter;
      if (!in_sequence) {
        ESP_LOGV(TAG, "Counter %u far ahead of %u, waiting for the next one", counter, this->accepted_);
        return false;
      }
    }
  }
  this->resync_pending_ = false;
  this->accepted_ = counter;
  this->accepted_known_ = true;
  // Batched by the preferences; a flash write at once only every block.
  this->store_accepted_(static_cast<uint16_t>(counter - this->accepted_stored_) >= RESERVE_BLOCK);
  return true;
}

void KeeloqRemote::store_accepted_(bool sync) {
  bool saved = this->accepted_pref_.save(&this->accepted_);
  if (sync) {
    this->accepted_stored_ = this->accepted_;
    bool synced = global_preferences->sync();
    if (!saved || !synced) {
      ESP_LOGW(
          TAG,
          "Could not save the last press, so after a restart presses may be ignored or a replay may fire on_command");
    }
  }
}

void KeeloqRemote::take_over_next_frame() {
  ESP_LOGI(TAG, "Press the hand remote now");
  this->waiting_ = true;
  this->set_timeout(TAKE_OVER_TIMEOUT_ID, TAKE_OVER_TIMEOUT_MS, [this]() {
    this->waiting_ = false;
    ESP_LOGW(TAG, "Take-over timed out without a press of the hand remote");
  });
}

// What a remote without serial keeps in flash.
struct TakenOver {
  uint32_t serial;
  uint16_t reserved;
  uint16_t channels;
} __attribute__((packed));

void KeeloqRemote::setup() {
  if (this->take_over_) {
    this->pref_ = global_preferences->make_preference<TakenOver>(this->take_over_key_, true);
    TakenOver stored{};
    if (this->pref_.load(&stored)) {
      this->use_serial_(stored.serial);
      this->reserved_ = stored.reserved;
      if (!this->channels_configured_) {
        this->channels_ = stored.channels;
      }
    }
  } else {
    this->pref_ = global_preferences->make_preference<uint16_t>(preference_key(this->type_, this->serial_), true);
    this->pref_.load(&this->reserved_);
  }
  this->counter_ = this->reserved_;
  // Only a remote with a receiver accepts presses, so only it saves the last one.
  if (!this->receiving_) {
    return;
  }
  uint32_t key = this->take_over_ ? this->take_over_key_ : preference_key(this->type_, this->serial_);
  this->accepted_pref_ = global_preferences->make_preference<uint16_t>(fnv1a_hash_extend(key, "accepted"), true);
  uint16_t accepted;
  if (this->accepted_pref_.load(&accepted)) {
    this->accepted_ = accepted;
    this->accepted_stored_ = accepted;
    this->accepted_known_ = true;
  }
  // A hand remote with this serial may have been followed further than the reserved block. Its last stored
  // press lags by less than a block, so continuing a block after it stays ahead and inside the window.
  uint16_t followed = this->accepted_ + RESERVE_BLOCK;
  if (this->accepted_known_ && static_cast<uint16_t>(followed - this->counter_) < 0x8000) {
    this->counter_ = followed;
  }
}

void KeeloqRemote::dump_config() {
  ESP_LOGCONFIG(TAG,
                "KeeLoq remote:\n"
                "  Takes over a hand remote: %s\n"
                "  Serial: 0x%07" PRIX32 "\n"
                "  Next counter: %u",
                YESNO(this->take_over_), this->serial_, this->counter_);
}

void KeeloqRemote::store_reserved_() {
  if (this->take_over_ && !this->known_) {
    ESP_LOGW(TAG, "No serial taken over yet, so the counter stays in RAM until the next take-over replaces it");
    return;
  }
  bool saved;
  if (this->take_over_) {
    TakenOver stored{this->serial_, this->reserved_, this->channels_};
    saved = this->pref_.save(&stored);
  } else {
    saved = this->pref_.save(&this->reserved_);
  }
  if (!saved || !global_preferences->sync()) {
    ESP_LOGW(TAG, "Could not save the counter, so presses after a restart may be ignored until the counter catches up");
  }
}

remote_base::KeeloqData KeeloqRemote::next_frame(uint8_t command, uint16_t channels) {
  // Received frames can move the counter past the reserved block.
  uint16_t left = this->reserved_ - this->counter_;
  if (left == 0 || left > RESERVE_BLOCK) {
    this->reserved_ = this->counter_ + RESERVE_BLOCK;
    this->store_reserved_();
  }
  // Taken before the log line, so a frame sent from a log trigger gets the next counter.
  const uint16_t counter = this->counter_++;
  // A receiver that hears this frame takes it as the last one, so its echo is no new press; never backwards.
  if (this->receiving_ && (!this->accepted_known_ || static_cast<uint16_t>(counter - this->accepted_) < 0x8000)) {
    this->accepted_ = counter;
    this->accepted_known_ = true;
    // Saved with the next flash write, so a recording of this frame does not fire after a restart either.
    this->store_accepted_(false);
  }
  remote_base::KeeloqData data{};
  data.address = this->serial_;
  data.command = command & 0x0F;
  // The channel mask's low byte and the serial's low byte are the discrimination bits; its high byte is the group
  // sent after the word.
  uint32_t plain = counter | (channels & 0xFF) << 24 | (this->serial_ & 0xFF) << 16;
  data.set_group(channels >> 8);
  data.encrypted = encrypt(plain, this->device_key_);
  ESP_LOGV(TAG, "Serial 0x%07" PRIX32 " counter %u", this->serial_, counter);
  return data;
}

void KeeloqRemote::send(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels,
                        uint32_t send_times, uint32_t send_wait) {
  if (!this->ready_to_send() || no_channels(channels))
    return;
  transmitter->transmit<remote_base::KeeloqProtocol>(this->next_frame(command, channels), send_times, send_wait);
}

void KeeloqRemote::learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels) {
  if (this->take_over_) {
    ESP_LOGW(TAG, "A remote without serial does not learn; it takes over a hand remote");
    return;
  }
  // Also keeps 0, the value for no pending learn, out of learn_channels_.
  if (no_channels(channels))
    return;
  // A second learn would replace the pending stop frame, so the first pairing would fail.
  if (this->learn_channels_ != 0) {
    ESP_LOGW(TAG, "Learn skipped, the previous one is still running; pass all channels to one learn");
    return;
  }
  // Pending before the first frame, so a learn that the transmitter's triggers start is skipped too.
  this->learn_channels_ = channels;
  // Up+down, a pause, then stop.
  this->send(transmitter, JAROLIFT_UP_DOWN, channels, JAROLIFT_SEND_TIMES, 0);
  this->set_timeout(LEARN_TIMEOUT_ID, JAROLIFT_LEARN_PAUSE_MS, [this, transmitter]() {
    this->send(transmitter, JAROLIFT_STOP, this->learn_channels_, JAROLIFT_SEND_TIMES, 0);
    this->learn_channels_ = 0;
  });
}

void KeeloqRemote::set_counter(uint16_t counter) {
  this->counter_ = counter;
  this->reserved_ = counter;
  this->store_reserved_();
}

}  // namespace esphome::keeloq
