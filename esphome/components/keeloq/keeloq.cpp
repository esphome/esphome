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
static constexpr uint32_t LEARN_TIMEOUT_ID = 0;

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
    : device_key_(device_key(type, serial, manufacturer_key)), serial_(serial), type_(type) {}

uint32_t KeeloqRemote::preference_key(KeeloqType type, uint32_t serial) {
  return PREF_HASH ^ (static_cast<uint32_t>(type) << 28 | serial);
}

void KeeloqRemote::setup() {
  this->pref_ = global_preferences->make_preference<uint16_t>(preference_key(this->type_, this->serial_), true);
  this->pref_.load(&this->reserved_);
  this->counter_ = this->reserved_;
}

void KeeloqRemote::dump_config() {
  ESP_LOGCONFIG(TAG,
                "KeeLoq remote:\n"
                "  Serial: 0x%07" PRIX32 "\n"
                "  Next counter: %u",
                this->serial_, this->counter_);
}

void KeeloqRemote::store_reserved_() {
  this->pref_.save(&this->reserved_);
  global_preferences->sync();
}

remote_base::KeeloqData KeeloqRemote::next_frame(uint8_t command, uint16_t channels) {
  if (this->counter_ == this->reserved_) {
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
