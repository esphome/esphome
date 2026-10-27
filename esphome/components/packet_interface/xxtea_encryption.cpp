#include "xxtea_encryption.h"
#ifdef USE_PACKET_INTERFACE_XXTEA
#include <algorithm>
#include <cstring>
#include "esphome/components/xxtea/xxtea.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.xxtea");

static constexpr uint32_t PREF_HASH = fnv1a_hash("packet_interface.xxtea");
static constexpr uint32_t PEER_PREF_HASH = fnv1a_hash("packet_interface.xxtea.peer");
// Different for each mode, so devices set up differently drop each other's packets
static constexpr uint8_t CHECK_BYTES[3] = {0x58, 0x54, 0x45};
static constexpr uint8_t ROLLING_CHECK_BYTES[3] = {0x58, 0x54, 0x52};
static constexpr uint8_t BROADCAST_CHECK_BYTES[3] = {0x58, 0x54, 0x42};
static constexpr size_t CHECK_SIZE = 4;
// Boot number and counter, then in echo mode the echo of the receiver's boot number
static constexpr size_t BOOT_OFFSET = 0;
static constexpr size_t COUNTER_OFFSET = 4;
static constexpr size_t ECHO_OFFSET = 8;
// XXTEA needs at least two words
static constexpr size_t MIN_LENGTH = 8;
static constexpr size_t MAX_PADDING = 4;

static void put_uint32(uint8_t *buf, uint32_t value) {
  buf[0] = value;
  buf[1] = value >> 8;
  buf[2] = value >> 16;
  buf[3] = value >> 24;
}

static uint32_t get_uint32(const uint8_t *buf) { return encode_uint32(buf[3], buf[2], buf[1], buf[0]); }

// The size of the rolling code, which comes before the check bytes
static constexpr size_t rolling_size(RollingCode rolling_code) {
  switch (rolling_code) {
    case RollingCode::ROLLING_CODE_ECHO:
      return 12;
    case RollingCode::ROLLING_CODE_BROADCAST:
      return 8;
    default:
      return 0;
  }
}

static constexpr size_t trailer_size(RollingCode rolling_code) { return rolling_size(rolling_code) + CHECK_SIZE; }

static const uint8_t *check_bytes(RollingCode rolling_code) {
  switch (rolling_code) {
    case RollingCode::ROLLING_CODE_ECHO:
      return ROLLING_CHECK_BYTES;
    case RollingCode::ROLLING_CODE_BROADCAST:
      return BROADCAST_CHECK_BYTES;
    default:
      return CHECK_BYTES;
  }
}

XxteaEncryption::XxteaEncryption(PacketInterface *lower, RollingCode rolling_code)
    // XXTEA works on whole 32-bit words
    : PacketInterface(remaining_packet_size(lower->get_max_packet_size() & ~size_t{3}, trailer_size(rolling_code))),
      lower_(lower),
      trailer_size_(trailer_size(rolling_code)),
      rolling_code_(rolling_code) {
  this->stack_on_(lower, CAPABILITY_ENCRYPTION);
}

void XxteaEncryption::set_key(std::initializer_list<uint32_t> key) {
  std::copy_n(key.begin(), std::min(key.size(), this->key_.size()), this->key_.begin());
}

void XxteaEncryption::setup() {
  if (this->max_packet_size_ == 0) {
    this->mark_failed(LOG_STR("max_packet_size too small for the configured layers"));
    return;
  }
  size_t words = this->lower_->get_max_packet_size() / 4;
  RAMAllocator<uint32_t> allocator;
  this->tx_buffer_ = allocator.allocate(words * 2);
  if (this->tx_buffer_ == nullptr) {
    this->mark_failed(LOG_STR("Not enough memory for the packet buffers"));
    return;
  }
  this->rx_buffer_ = this->tx_buffer_ + words;
  if (this->has_rolling_code_()) {
    this->pref_ = global_preferences->make_preference<uint32_t>(fnv1a_hash_extend(PREF_HASH, this->name_), true);
    this->pref_.load(&this->boot_);
    this->next_boot_();
  }
  if (this->save_peer_boot_ && this->rolling_code_ == RollingCode::ROLLING_CODE_BROADCAST) {
    this->peer_pref_ =
        global_preferences->make_preference<uint32_t>(fnv1a_hash_extend(PEER_PREF_HASH, this->name_), true);
    if (this->peer_pref_.load(&this->saved_peer_boot_))
      this->peer_boot_ = this->saved_peer_boot_;
  } else {
    this->save_peer_boot_ = false;
  }
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void XxteaEncryption::dump_config() {
  const LogString *mode = LOG_STR("NO");
  if (this->rolling_code_ == RollingCode::ROLLING_CODE_ECHO) {
    mode = LOG_STR("ECHO");
  } else if (this->rolling_code_ == RollingCode::ROLLING_CODE_BROADCAST) {
    mode = LOG_STR("BROADCAST");
  }
  ESP_LOGCONFIG(TAG,
                "XXTEA Encryption on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Rolling Code: %s",
                this->name_, this->max_packet_size_, LOG_STR_ARG(mode));
  if (this->has_rolling_code_()) {
    ESP_LOGCONFIG(TAG, "  Boot Number: %" PRIu32, this->boot_);
  }
  if (this->save_peer_boot_) {
    ESP_LOGCONFIG(TAG, "  Saved Peer Boot Number: %" PRIu32, this->saved_peer_boot_);
  }
}

// A new boot number must be saved before it is used, or a crash could make the peer see it again
void XxteaEncryption::next_boot_() {
  this->boot_++;
  this->counter_ = 0;
  if (!this->pref_.save(&this->boot_) || !global_preferences->sync()) {
    ESP_LOGW(TAG, "%s: Could not save the boot number, so a restart could reuse it", this->name_);
  }
}

void XxteaEncryption::reset_peer() {
  ESP_LOGI(TAG, "%s: Rolling code of the peer reset", this->name_);
  this->peer_seen_ = false;
  this->peer_boot_ = 0;
  this->peer_counter_ = 0;
  this->old_boot_warned_ = false;
  if (this->save_peer_boot_) {
    this->saved_peer_boot_ = 0;
    this->peer_pref_.save(&this->saved_peer_boot_);
  }
}

bool XxteaEncryption::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  if (this->tx_buffer_ == nullptr)
    return false;
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  size_t padding = (4 - (size + this->trailer_size_) % 4) % 4;
  if (size + padding + this->trailer_size_ < MIN_LENGTH)
    padding += 4;
  size_t length = size + padding + this->trailer_size_;
  auto *buf = reinterpret_cast<uint8_t *>(this->tx_buffer_);
  data.copy_to(buf, size);
  memset(buf + size, 0, padding);
  uint8_t *check = buf + length - CHECK_SIZE;
  memcpy(check, check_bytes(this->rolling_code_), sizeof(CHECK_BYTES));
  if (this->has_rolling_code_()) {
    uint8_t *rolling = check - rolling_size(this->rolling_code_);
    put_uint32(rolling + BOOT_OFFSET, this->boot_);
    put_uint32(rolling + COUNTER_OFFSET, this->counter_);
    if (this->rolling_code_ == RollingCode::ROLLING_CODE_ECHO)
      put_uint32(rolling + ECHO_OFFSET, this->echo_boot_);
    if (++this->counter_ == 0)
      this->next_boot_();
  }
  check[CHECK_SIZE - 1] = padding;
  xxtea::encrypt(this->tx_buffer_, length / 4, this->key_.data());
  return this->lower_->send_to_interface(PacketBuffer(buf, length), meta_data);
}

// Before the first packet from the peer, peer_boot_ is the saved boot number, or 0
bool XxteaEncryption::accept_counter_(uint32_t boot, uint32_t counter) {
  if (boot < this->peer_boot_ || (this->peer_seen_ && boot == this->peer_boot_ && counter <= this->peer_counter_))
    return false;
  this->peer_seen_ = true;
  this->peer_boot_ = boot;
  this->peer_counter_ = counter;
  // Saved only when the peer restarts, so the flash is rarely written
  if (this->save_peer_boot_ && boot > this->saved_peer_boot_) {
    this->saved_peer_boot_ = boot;
    this->peer_pref_.save(&this->saved_peer_boot_);
  }
  return true;
}

void XxteaEncryption::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t length = data.size();
  if (length % 4 != 0 || length < std::max(MIN_LENGTH, this->trailer_size_) ||
      length > this->lower_->get_max_packet_size()) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes dropped", this->name_, length);
    this->note_received_(false);
    return;
  }
  auto *buf = reinterpret_cast<uint8_t *>(this->rx_buffer_);
  data.copy_to(buf, length);
  xxtea::decrypt(this->rx_buffer_, length / 4, this->key_.data());
  const uint8_t *check = buf + length - CHECK_SIZE;
  const uint8_t *expected = check_bytes(this->rolling_code_);
  size_t padding = check[CHECK_SIZE - 1];
  if (memcmp(check, expected, sizeof(CHECK_BYTES)) != 0 || padding > MAX_PADDING ||
      padding > length - this->trailer_size_) {
    ESP_LOGV(TAG, "%s: Packet failed authentication", this->name_);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  if (this->has_rolling_code_()) {
    const uint8_t *rolling = check - rolling_size(this->rolling_code_);
    uint32_t boot = get_uint32(rolling + BOOT_OFFSET);
    uint32_t counter = get_uint32(rolling + COUNTER_OFFSET);
    if (this->rolling_code_ == RollingCode::ROLLING_CODE_ECHO) {
      uint32_t echo = get_uint32(rolling + ECHO_OFFSET);
      // Learn the peer's boot number even from a packet that is then dropped, so both sides can catch up
      if (boot > this->echo_boot_)
        this->echo_boot_ = boot;
      // A packet recorded before this boot cannot carry this boot's number
      if (echo != this->boot_) {
        ESP_LOGV(TAG, "%s: Packet for boot %" PRIu32 " dropped", this->name_, echo);
        return;
      }
    }
    if (!this->accept_counter_(boot, counter)) {
      // A boot number lower than one already heard means the peer lost its saved boot number, or a replay
      if (boot < this->peer_boot_ && !this->old_boot_warned_) {
        this->old_boot_warned_ = true;
        ESP_LOGW(TAG,
                 "%s: Packets from an older boot %" PRIu32 " of the peer are dropped; if its flash was erased, "
                 "reset the rolling code or restart this device",
                 this->name_, boot);
      }
      ESP_LOGV(TAG, "%s: Old packet %" PRIu32 ":%" PRIu32 " dropped", this->name_, boot, counter);
      return;
    }
  }
  this->on_receive_from_interface_(PacketBuffer(buf, length - this->trailer_size_ - padding), meta_data);
}

}  // namespace esphome::packet_interface
#endif
