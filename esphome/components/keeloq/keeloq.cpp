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
static constexpr uint32_t LEARN_TIMEOUT_ID = 0;

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
  if (!this->pref_.save(&this->reserved_) || !global_preferences->sync()) {
    ESP_LOGW(TAG, "Could not save the counter, so presses after a restart may be ignored until the counter catches up");
  }
}

remote_base::KeeloqData KeeloqRemote::next_frame(uint8_t command, uint16_t channels) {
  if (this->counter_ == this->reserved_) {
    this->reserved_ = this->counter_ + RESERVE_BLOCK;
    this->store_reserved_();
  }
  // Taken before the log line, so a frame sent from a log trigger gets the next counter.
  const uint16_t counter = this->counter_++;
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
  if (no_channels(channels))
    return;
  transmitter->transmit<remote_base::KeeloqProtocol>(this->next_frame(command, channels), send_times, send_wait);
}

void KeeloqRemote::learn(remote_base::RemoteTransmitterBase *transmitter, uint16_t channels) {
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
