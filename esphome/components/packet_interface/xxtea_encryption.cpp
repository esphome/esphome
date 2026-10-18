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
// Different for each mode, so devices set up differently drop each other's packets
static constexpr uint8_t CHECK_BYTES[3] = {0x58, 0x54, 0x45};
static constexpr uint8_t ROLLING_CHECK_BYTES[3] = {0x58, 0x54, 0x52};
static constexpr size_t CHECK_SIZE = 4;
// Boot number, counter, and the echo of the receiver's boot number
static constexpr size_t ROLLING_SIZE = 12;
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

XxteaEncryption::XxteaEncryption(PacketInterface *lower, bool rolling_code)
    : lower_(lower), trailer_size_(rolling_code ? ROLLING_SIZE + CHECK_SIZE : CHECK_SIZE), rolling_code_(rolling_code) {
  this->stack_on_(lower, CAPABILITY_ENCRYPTION);
  // XXTEA works on whole 32-bit words
  this->max_packet_size_ = (lower->get_max_packet_size() & ~size_t{3}) - this->trailer_size_;
}

void XxteaEncryption::set_key(std::initializer_list<uint32_t> key) {
  std::copy_n(key.begin(), std::min(key.size(), this->key_.size()), this->key_.begin());
}

void XxteaEncryption::setup() {
  size_t words = this->lower_->get_max_packet_size() / 4;
  this->tx_buffer_ = std::make_unique<uint32_t[]>(words);
  this->rx_buffer_ = std::make_unique<uint32_t[]>(words);
  if (this->rolling_code_) {
    this->pref_ = global_preferences->make_preference<uint32_t>(fnv1a_hash_extend(PREF_HASH, this->name_), true);
    this->pref_.load(&this->boot_);
    this->next_boot_();
  }
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void XxteaEncryption::dump_config() {
  ESP_LOGCONFIG(TAG,
                "XXTEA Encryption on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Rolling Code: %s",
                this->name_, this->max_packet_size_, YESNO(this->rolling_code_));
  if (this->rolling_code_) {
    ESP_LOGCONFIG(TAG, "  Boot Number: %" PRIu32, this->boot_);
  }
}

// A new boot number must be saved before it is used, or a crash could make the peer see it again
void XxteaEncryption::next_boot_() {
  this->boot_++;
  this->counter_ = 0;
  this->pref_.save(&this->boot_);
  global_preferences->sync();
}

bool XxteaEncryption::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  size_t padding = (4 - (size + this->trailer_size_) % 4) % 4;
  if (size + padding + this->trailer_size_ < MIN_LENGTH)
    padding += 4;
  size_t length = size + padding + this->trailer_size_;
  auto *buf = reinterpret_cast<uint8_t *>(this->tx_buffer_.get());
  data.copy_to(buf, size);
  memset(buf + size, 0, padding);
  uint8_t *check = buf + length - CHECK_SIZE;
  if (this->rolling_code_) {
    put_uint32(check - ROLLING_SIZE, this->boot_);
    put_uint32(check - ROLLING_SIZE + 4, this->counter_);
    put_uint32(check - ROLLING_SIZE + 8, this->echo_boot_);
    memcpy(check, ROLLING_CHECK_BYTES, sizeof(ROLLING_CHECK_BYTES));
    if (++this->counter_ == 0)
      this->next_boot_();
  } else {
    memcpy(check, CHECK_BYTES, sizeof(CHECK_BYTES));
  }
  check[CHECK_SIZE - 1] = padding;
  xxtea::encrypt(this->tx_buffer_.get(), length / 4, this->key_.data());
  return this->lower_->send_to_interface(PacketBuffer(buf, length), meta_data);
}

bool XxteaEncryption::accept_counter_(uint32_t boot, uint32_t counter) {
  if (this->peer_seen_ && (boot < this->peer_boot_ || (boot == this->peer_boot_ && counter <= this->peer_counter_)))
    return false;
  this->peer_seen_ = true;
  this->peer_boot_ = boot;
  this->peer_counter_ = counter;
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
  auto *buf = reinterpret_cast<uint8_t *>(this->rx_buffer_.get());
  data.copy_to(buf, length);
  xxtea::decrypt(this->rx_buffer_.get(), length / 4, this->key_.data());
  const uint8_t *check = buf + length - CHECK_SIZE;
  const uint8_t *expected = this->rolling_code_ ? ROLLING_CHECK_BYTES : CHECK_BYTES;
  size_t padding = check[CHECK_SIZE - 1];
  if (memcmp(check, expected, sizeof(CHECK_BYTES)) != 0 || padding > MAX_PADDING ||
      padding > length - this->trailer_size_) {
    ESP_LOGV(TAG, "%s: Packet failed authentication", this->name_);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  if (this->rolling_code_) {
    uint32_t boot = get_uint32(check - ROLLING_SIZE);
    uint32_t counter = get_uint32(check - ROLLING_SIZE + 4);
    uint32_t echo = get_uint32(check - ROLLING_SIZE + 8);
    // Learn the peer's boot number even from a packet that is then dropped, so both sides can catch up
    if (boot > this->echo_boot_)
      this->echo_boot_ = boot;
    // A packet recorded before this boot cannot carry this boot's number
    if (echo != this->boot_) {
      ESP_LOGV(TAG, "%s: Packet for boot %" PRIu32 " dropped", this->name_, echo);
      return;
    }
    if (!this->accept_counter_(boot, counter)) {
      ESP_LOGV(TAG, "%s: Old packet %" PRIu32 ":%" PRIu32 " dropped", this->name_, boot, counter);
      return;
    }
  }
  this->on_receive_from_interface_(PacketBuffer(buf, length - this->trailer_size_ - padding), meta_data);
}

}  // namespace esphome::packet_interface
#endif
