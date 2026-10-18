#include "basic_fragmentation.h"
#ifdef USE_PACKET_INTERFACE_BASIC_FRAGMENTATION
#include <algorithm>
#include <cstring>
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.fragmentation");

static constexpr uint8_t LAST_FRAGMENT = 0x80;
static constexpr uint8_t INDEX_MASK = 0x7F;

BasicFragmentation::BasicFragmentation(PacketInterface *lower, size_t max_packet_size) : lower_(lower) {
  this->stack_on_(lower, CAPABILITY_FRAGMENTATION);
  this->max_packet_size_ = max_packet_size;
}

size_t BasicFragmentation::fragments_for_(size_t size) const {
  size_t fragment_size = this->fragment_size_();
  return std::max<size_t>(1, (size + fragment_size - 1) / fragment_size);
}

void BasicFragmentation::setup() {
  if (this->fragments_for_(this->max_packet_size_) > MAX_FRAGMENTS) {
    this->mark_failed(LOG_STR("max_packet_size needs more than 128 fragments"));
    return;
  }
  // Checked now, while the layer below holds nothing
  this->queue_too_short_ = this->fragments_for_(this->max_packet_size_) > this->lower_->get_send_capacity();
  this->tx_buffer_ = std::make_unique<uint8_t[]>(this->fragment_size_());
  this->rx_buffer_ = std::make_unique<uint8_t[]>(this->max_packet_size_);
  // Numbering from a random start makes it unlikely that a packet joins fragments from before a restart
  this->tx_number_ = random_uint32();
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
}

void BasicFragmentation::dump_config() {
  size_t fragments = this->fragments_for_(this->max_packet_size_);
  ESP_LOGCONFIG(TAG,
                "Basic Fragmentation on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Fragment Size: %zu\n"
                "  Fragments in the Largest Packet: %zu",
                this->name_, this->max_packet_size_, this->fragment_size_(), fragments);
  if (this->queue_too_short_) {
    ESP_LOGW(TAG, "  The reliability queue is too short for the largest packets, which cannot be sent");
  }
}

bool BasicFragmentation::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  if (this->is_failed())
    return false;
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  size_t fragments = this->fragments_for_(size);
  if (fragments > this->lower_->get_send_capacity()) {
    ESP_LOGV(TAG, "%s: No room for %zu fragments", this->name_, fragments);
    return false;
  }
  uint8_t header[HEADER_SIZE] = {this->tx_number_++, 0};
  if (fragments == 1) {
    header[1] = LAST_FRAGMENT;
    return this->lower_->send_to_interface(PacketBuffer(header, HEADER_SIZE, &data, nullptr, 0), meta_data);
  }
  size_t fragment_size = this->fragment_size_();
  for (size_t index = 0; index != fragments; index++) {
    size_t length = data.copy_to(this->tx_buffer_.get(), fragment_size, index * fragment_size);
    header[1] = index | (index + 1 == fragments ? LAST_FRAGMENT : 0);
    PacketBuffer body(this->tx_buffer_.get(), length);
    if (!this->lower_->send_to_interface(PacketBuffer(header, HEADER_SIZE, &body, nullptr, 0), meta_data)) {
      // The peer drops the part already sent when the next packet starts
      ESP_LOGV(TAG, "%s: Fragment %zu of %zu not accepted", this->name_, index, fragments);
      return false;
    }
  }
  return true;
}

void BasicFragmentation::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Lower layers deliver packets in one piece; a packet in one fragment is passed up without copying
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "%s: Packet split across buffers is not supported", this->name_);
    return;
  }
  auto fragment = data.as_span();
  if (fragment.size() < HEADER_SIZE)
    return;
  uint8_t number = fragment[0];
  uint8_t index = fragment[1] & INDEX_MASK;
  bool last = (fragment[1] & LAST_FRAGMENT) != 0;
  const uint8_t *payload = fragment.data() + HEADER_SIZE;
  size_t length = fragment.size() - HEADER_SIZE;
  if (index == 0) {
    if (this->rx_joining_) {
      ESP_LOGV(TAG, "%s: Incomplete packet %u dropped", this->name_, this->rx_number_);
    }
    this->rx_joining_ = false;
    if (last) {
      if (length > this->max_packet_size_) {
        ESP_LOGV(TAG, "%s: Packet %u is larger than %zu, dropped", this->name_, number, this->max_packet_size_);
        return;
      }
      this->on_receive_from_interface_(PacketBuffer(payload, length), meta_data);
      return;
    }
    this->rx_joining_ = true;
    this->rx_number_ = number;
    this->rx_source_ = meta_data.source_address;
    this->rx_next_index_ = 0;
    this->rx_length_ = 0;
  } else if (!this->rx_joining_ || number != this->rx_number_ || meta_data.source_address != this->rx_source_ ||
             index != this->rx_next_index_) {
    ESP_LOGV(TAG, "%s: Fragment %u of packet %u dropped", this->name_, index, number);
    this->rx_joining_ = false;
    return;
  }
  if (this->rx_length_ + length > this->max_packet_size_) {
    ESP_LOGV(TAG, "%s: Packet %u is larger than %zu, dropped", this->name_, number, this->max_packet_size_);
    this->rx_joining_ = false;
    return;
  }
  memcpy(this->rx_buffer_.get() + this->rx_length_, payload, length);
  this->rx_length_ += length;
  this->rx_next_index_++;
  if (last) {
    this->rx_joining_ = false;
    this->on_receive_from_interface_(PacketBuffer(this->rx_buffer_.get(), this->rx_length_), meta_data);
  }
}

}  // namespace esphome::packet_interface
#endif
