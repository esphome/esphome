#include "lzss_compression.h"
#ifdef USE_PACKET_INTERFACE_LZSS
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.lzss");

LzssCompression::LzssCompression(PacketInterface *lower, size_t max_packet_size, const uint8_t *dictionary,
                                 size_t dictionary_size, uint8_t check, uint8_t *buffers, uint16_t *chains)
    : PacketInterface(max_packet_size != 0 ? max_packet_size : packet_size(lower->get_max_packet_size())),
      check_(check & HEADER_CHECK_MASK),
      lower_(lower),
      codec_(dictionary, dictionary_size),
      tx_input_(buffers),
      chains_(chains) {
  this->stack_on_(lower, CAPABILITY_COMPRESSION);
}

void LzssCompression::setup() {
  size_t lower_size = this->lower_->get_max_packet_size();
  if (!this->setup_check(this->max_packet_size_ != 0 && lower_size > HEADER_SIZE,
                         LOG_STR("max_packet_size too small for the configured layers")))
    return;
  if (!this->setup_check(this->tx_input_ != nullptr && this->chains_ != nullptr,
                         LOG_STR("Not enough memory for the packet buffers")))
    return;
  this->rx_output_ = this->tx_input_ + this->max_packet_size_;
  this->tx_output_ = this->rx_output_ + this->max_packet_size_;
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void LzssCompression::dump_config() {
  ESP_LOGCONFIG(TAG,
                "LZSS Compression on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Dictionary Size: %zu\n"
                "  Dictionary Check: 0x%02X",
                this->name_, this->max_packet_size_, this->codec_.dictionary_size(), this->check_);
}

bool LzssCompression::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  return this->send_exclusive_([&] { return this->send_(data, meta_data); });
}

bool LzssCompression::send_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  if (this->tx_output_ == nullptr)
    return false;
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  const uint8_t *in = this->tx_input_;
  if (data.is_contiguous()) {
    in = data.as_span().data();
  } else {
    data.copy_to(this->tx_input_, size);
  }
  size_t room = this->lower_->get_max_packet_size() - HEADER_SIZE;
  // Compressed only if that makes it smaller
  size_t length = size > 1 ? this->codec_.compress(in, size, this->tx_output_ + HEADER_SIZE,
                                                   clamp_at_most(room, size - 1), this->chains_)
                           : 0;
  if (length != 0) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes compressed to %zu", this->name_, size, length);
    this->tx_output_[0] = HEADER_COMPRESSED | this->check_;
    return this->lower_->send_to_interface(PacketBuffer(this->tx_output_, length + HEADER_SIZE), meta_data);
  }
  if (size > room) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes does not compress to %zu", this->name_, size, room);
    return false;
  }
  uint8_t header = this->check_;
  return this->lower_->send_to_interface(PacketBuffer(&header, HEADER_SIZE, &data, nullptr, 0), meta_data);
}

void LzssCompression::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Lower layers deliver packets in one piece
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "%s: Packet split across buffers is not supported", this->name_);
    this->note_received_(false);
    return;
  }
  auto packet = data.as_span();
  if (packet.size() < HEADER_SIZE || (packet[0] & HEADER_CHECK_MASK) != this->check_) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes with another dictionary dropped", this->name_, packet.size());
    this->note_received_(false);
    return;
  }
  const uint8_t *body = packet.data() + HEADER_SIZE;
  size_t size = packet.size() - HEADER_SIZE;
  if ((packet[0] & HEADER_COMPRESSED) == 0) {
    if (size > this->max_packet_size_) {
      ESP_LOGV(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
      this->note_received_(false);
      return;
    }
    this->note_received_(true);
    this->on_receive_from_interface_(PacketBuffer(body, size), meta_data);
    return;
  }
  size_t length = this->codec_.expand(body, size, this->rx_output_, this->max_packet_size_);
  if (length == SIZE_MAX) {
    ESP_LOGV(TAG, "%s: Malformed packet of %zu bytes dropped", this->name_, packet.size());
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  this->on_receive_from_interface_(PacketBuffer(this->rx_output_, length), meta_data);
}

}  // namespace esphome::packet_interface
#endif
