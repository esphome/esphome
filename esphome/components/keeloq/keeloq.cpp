#include "keeloq.h"
#include "esphome/core/log.h"

#include <cinttypes>

namespace esphome::keeloq {

ESPHOME_LOG_TAG(TAG, "keeloq");

static constexpr uint32_t NLF = 0x3A5C742E;
static constexpr uint16_t ROUNDS = 528;
// Counters reserved in flash at a time. Fewer than the 16 codes a KeeLoq receiver accepts ahead
// of the last one, so the gap after a reboot stays inside that window.
static constexpr uint16_t RESERVE_BLOCK = 8;
static constexpr uint32_t PREF_HASH = 0x4B4C4F51;
// Jarolift sends every press twice with the same counter.
static constexpr uint32_t JAROLIFT_SEND_TIMES = 2;
static constexpr uint8_t JAROLIFT_STOP = 0x4;
static constexpr uint8_t JAROLIFT_UP_DOWN = 0xA;
static constexpr uint8_t JAROLIFT_LEARN_LEGACY = 0x1;
static constexpr uint32_t JAROLIFT_LEARN_PAUSE_MS = 1000;
static constexpr uint32_t TAKE_OVER_TIMEOUT_MS = 60000;
// Scheduler ids.
static constexpr uint32_t LEARN_TIMEOUT_ID = 0;
static constexpr uint32_t TAKE_OVER_TIMEOUT_ID = 1;
// How far ahead a receiver accepts a counter: with one frame, and with a second one in sequence (resync).
struct Windows {
  uint16_t single;
  uint16_t resync;
};

// HCS301 data sheet DS21143C, section 7.3: 16 codes with one press, 32K with two. A new type adds its values here.
static Windows receive_windows(KeeloqType type) {
  switch (type) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      return {16, 0x8000};
  }
}
static constexpr uint32_t ACCEPTED_PREF_HASH = 0x41434350;

static inline uint32_t get_bit(uint64_t value, uint32_t n) { return (value >> n) & 1; }

uint32_t encrypt(uint32_t data, uint64_t key) {
  uint32_t x = data;
  for (uint16_t r = 0; r < ROUNDS; r++) {
    uint32_t nlf = get_bit(
        NLF, get_bit(x, 1) | get_bit(x, 9) << 1 | get_bit(x, 20) << 2 | get_bit(x, 26) << 3 | get_bit(x, 31) << 4);
    x = (x >> 1) | ((get_bit(x, 0) ^ get_bit(x, 16) ^ nlf ^ get_bit(key, r & 63u)) << 31);
  }
  return x;
}

uint32_t decrypt(uint32_t data, uint64_t key) {
  uint32_t x = data;
  for (uint16_t r = 0; r < ROUNDS; r++) {
    uint32_t nlf = get_bit(
        NLF, get_bit(x, 0) | get_bit(x, 8) << 1 | get_bit(x, 19) << 2 | get_bit(x, 25) << 3 | get_bit(x, 30) << 4);
    x = (x << 1) | (get_bit(x, 31) ^ get_bit(x, 15) ^ nlf ^ get_bit(key, (15u - r) & 63u));
  }
  return x;
}

// Microchip normal learning: the serial decrypted twice with the manufacturer key.
static uint64_t normal_learning_key(uint32_t serial, uint64_t manufacturer_key) {
  serial &= 0x0FFFFFFF;
  uint64_t high = decrypt(serial | 0x60000000, manufacturer_key);
  return high << 32 | decrypt(serial | 0x20000000, manufacturer_key);
}

static uint64_t device_key(KeeloqType type, uint32_t serial, uint64_t manufacturer_key) {
  switch (type) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      return normal_learning_key(serial, manufacturer_key);
  }
}

KeeloqRemote::KeeloqRemote(KeeloqType type, uint64_t manufacturer_key, uint32_t serial)
    : manufacturer_key_(manufacturer_key),
      device_key_(device_key(type, serial, manufacturer_key)),
      serial_(serial),
      type_(type) {}

void KeeloqRemote::use_serial_(uint32_t serial) {
  this->serial_ = serial;
  this->device_key_ = device_key(this->type_, serial, this->manufacturer_key_);
  this->known_ = true;
}

bool KeeloqRemote::resolve_channels(bool has_action_channels, uint16_t action_channels, uint16_t &channels) const {
  if (has_action_channels) {
    channels = action_channels;
    return true;
  }
  channels = this->channels_;
  switch (this->type_) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      if (!this->channels_known_) {
        ESP_LOGW(TAG, "No channels: set it on the action or the remote, or take over a hand remote");
        return false;
      }
      return true;
  }
}

bool KeeloqRemote::ready_to_send() const {
  if (!this->known_) {
    ESP_LOGW(TAG, "No serial yet: take over a hand remote first");
  }
  return this->known_;
}

bool KeeloqRemote::decrypt_frame_(const remote_base::KeeloqData &data, KeeloqFrame &frame) const {
  uint64_t key = this->known_ && data.address == this->serial_
                     ? this->device_key_
                     : device_key(this->type_, data.address, this->manufacturer_key_);
  uint32_t plain = decrypt(data.encrypted, key);
  switch (this->type_) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      // A wrong key gives random bits; the serial's low byte inside the code proves the right one.
      if (data.suffix_bits != 6 || ((plain >> 16) & 0xFF) != (data.address & 0xFF)) {
        return false;
      }
      frame.channels = data.group() << 8 | plain >> 24;
      break;
  }
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
    this->use_serial_(frame.serial);
    // A channels from the configuration stays; otherwise the hand remote's channels become the default.
    if (!this->channels_configured_) {
      this->channels_ = frame.channels;
      this->channels_known_ = true;
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
    Windows windows = receive_windows(this->type_);
    if (ahead == 0) {
      ESP_LOGVV(TAG, "Copy of counter %u", counter);
      return false;
    }
    if (ahead >= windows.resync) {
      ESP_LOGD(TAG, "Old counter %u, last %u: possible replay", counter, this->accepted_);
      this->resync_pending_ = false;
      return false;
    }
    if (ahead > windows.single) {
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

// What the receiving side keeps in flash.
struct Accepted {
  uint16_t counter;
  uint8_t known;
} __attribute__((packed));

void KeeloqRemote::store_accepted_(bool sync) {
  Accepted stored{this->accepted_, this->accepted_known_};
  this->accepted_pref_.save(&stored);
  if (sync) {
    this->accepted_stored_ = this->accepted_;
    global_preferences->sync();
  }
}

void KeeloqRemote::take_over_next_frame() {
  ESP_LOGI(TAG, "Press the hand remote now");
  this->waiting_ = true;
  this->set_timeout(TAKE_OVER_TIMEOUT_ID, TAKE_OVER_TIMEOUT_MS, [this]() { this->waiting_ = false; });
}

uint32_t KeeloqRemote::preference_key(KeeloqType type, uint32_t serial) {
  return PREF_HASH ^ (static_cast<uint32_t>(type) << 28 | serial);
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
        this->channels_known_ = true;
      }
    }
  } else {
    this->pref_ = global_preferences->make_preference<uint16_t>(preference_key(this->type_, this->serial_), true);
    this->pref_.load(&this->reserved_);
  }
  this->counter_ = this->reserved_;
  uint32_t key = this->take_over_ ? this->take_over_key_ : preference_key(this->type_, this->serial_);
  this->accepted_pref_ = global_preferences->make_preference<Accepted>(key ^ ACCEPTED_PREF_HASH, true);
  Accepted accepted{};
  if (this->accepted_pref_.load(&accepted)) {
    this->accepted_ = accepted.counter;
    this->accepted_stored_ = accepted.counter;
    this->accepted_known_ = accepted.known != 0;
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
  if (this->take_over_) {
    TakenOver stored{this->serial_, this->reserved_, this->channels_};
    this->pref_.save(&stored);
  } else {
    this->pref_.save(&this->reserved_);
  }
  global_preferences->sync();
}

remote_base::KeeloqData KeeloqRemote::next_frame(uint8_t command, uint16_t channels) {
  // Received frames can move the counter past the reserved block.
  uint16_t left = this->reserved_ - this->counter_;
  if (left == 0 || left > RESERVE_BLOCK) {
    this->reserved_ = this->counter_ + RESERVE_BLOCK;
    this->store_reserved_();
  }
  remote_base::KeeloqData data{};
  data.address = this->serial_;
  data.command = command & 0x0F;
  uint32_t plain = this->counter_;
  switch (this->type_) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      // The channel mask's low byte and the serial's low byte are the discrimination bits; its high byte is the group
      // sent after the word.
      plain |= (channels & 0xFF) << 24 | (this->serial_ & 0xFF) << 16;
      data.set_group(channels >> 8);
      break;
  }
  data.encrypted = encrypt(plain, this->device_key_);
  ESP_LOGV(TAG, "Serial 0x%07" PRIX32 " counter %u", this->serial_, this->counter_);
  // A receiver that hears this frame takes it as the last one, so its echo is no new press; never backwards.
  if (!this->accepted_known_ || static_cast<uint16_t>(this->counter_ - this->accepted_) < 0x8000) {
    this->accepted_ = this->counter_;
    this->accepted_known_ = true;
  }
  this->counter_++;
  return data;
}

uint32_t KeeloqRemote::default_send_times() const {
  switch (this->type_) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      return JAROLIFT_SEND_TIMES;
  }
}

void KeeloqRemote::send_(remote_base::RemoteTransmitterBase *transmitter, uint8_t command, uint16_t channels) {
  transmitter->transmit<remote_base::KeeloqProtocol>(this->next_frame(command, channels), this->default_send_times());
}

void KeeloqRemote::learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels, bool legacy) {
  if (this->take_over_) {
    ESP_LOGW(TAG, "A remote without serial does not learn; it takes over a hand remote");
    return;
  }
  // A new type adds its sequence here; many receivers learn from one plain frame while in learn mode.
  switch (this->type_) {
    case KEELOQ_TYPE_JAROLIFT:
    default:
      // Motors made before 2010 learn from one command 0x1, later ones from up+down, then stop.
      if (legacy) {
        this->send_(transmitter, JAROLIFT_LEARN_LEGACY, channels);
        return;
      }
      this->send_(transmitter, JAROLIFT_UP_DOWN, channels);
      this->set_timeout(LEARN_TIMEOUT_ID, JAROLIFT_LEARN_PAUSE_MS,
                        [this, transmitter, channels]() { this->finish_learn_(transmitter, channels); });
      break;
  }
}

void KeeloqRemote::finish_learn_(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels) {
  this->send_(transmitter, JAROLIFT_STOP, channels);
}

void KeeloqRemote::set_counter(uint16_t counter) {
  this->counter_ = counter;
  this->reserved_ = counter;
  this->store_reserved_();
}

}  // namespace esphome::keeloq
