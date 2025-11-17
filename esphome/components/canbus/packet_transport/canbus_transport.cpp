#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "canbus_transport.h"

namespace esphome {
namespace canbus {

static const char *const TAG = "canbus_transport";

void CanbusTransport::setup() {
  PacketTransport::setup();
  // Register callback to receive CAN frames
  this->parent_->add_callback(
      [this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
        this->handle_can_frame_(can_id, extended_id, rtr, data);
      });
  ESP_LOGCONFIG(TAG, "CAN packet transport using CAN ID 0x%03X%s", this->can_id_,
                this->use_extended_id_ ? " (extended)" : "");
}

void CanbusTransport::loop() { PacketTransport::loop(); }

void CanbusTransport::update() {
  this->updated_ = true;
  this->resend_data_ = true;
  PacketTransport::update();
}

void CanbusTransport::handle_can_frame_(uint32_t can_id, bool extended_id, bool rtr,
                                        const std::vector<uint8_t> &data) {
  // Ignore frames not for us
  if (can_id != this->can_id_ || extended_id != this->use_extended_id_ || rtr)
    return;

  // Need at least 1 byte (header)
  if (data.empty()) {
    ESP_LOGW(TAG, "Received empty CAN frame");
    return;
  }

  uint8_t header = data[0];
  uint8_t sequence = header & SEQUENCE_MASK;
  bool is_last = (header & LAST_FRAME_FLAG) != 0;

  // Check for sequence errors
  if (this->receiving_ && sequence != this->expected_sequence_) {
    ESP_LOGD(TAG, "Sequence error: expected %d, got %d. Resetting.", this->expected_sequence_, sequence);
    this->receive_buffer_.clear();
    this->receiving_ = false;
    this->expected_sequence_ = 0;
  }

  // Start of new packet
  if (!this->receiving_ && sequence == 0) {
    this->receiving_ = true;
    this->receive_buffer_.clear();
  }

  if (!this->receiving_)
    return;

  // Append payload data (skip header byte)
  for (size_t i = 1; i < data.size(); i++) {
    if (this->receive_buffer_.size() >= MAX_PACKET_SIZE) {
      ESP_LOGD(TAG, "Packet too large, discarding");
      this->receive_buffer_.clear();
      this->receiving_ = false;
      this->expected_sequence_ = 0;
      return;
    }
    this->receive_buffer_.push_back(data[i]);
  }

  if (is_last) {
    // Complete packet received
    ESP_LOGV(TAG, "Received complete packet: %zu bytes", this->receive_buffer_.size());
    this->process_(this->receive_buffer_);
    this->receive_buffer_.clear();
    this->receiving_ = false;
    this->expected_sequence_ = 0;
  } else {
    // Expect next sequence
    this->expected_sequence_ = (sequence + 1) & SEQUENCE_MASK;
  }
}

void CanbusTransport::send_packet(const std::vector<uint8_t> &buf) const {
  if (buf.empty())
    return;

  size_t offset = 0;
  uint8_t sequence = 0;

  while (offset < buf.size()) {
    std::vector<uint8_t> frame_data;
    frame_data.reserve(CAN_MAX_DATA_LENGTH);

    // Calculate how many bytes we can send in this frame
    size_t remaining = buf.size() - offset;
    size_t payload_size = std::min(remaining, static_cast<size_t>(PAYLOAD_BYTES_PER_FRAME));
    bool is_last = (offset + payload_size >= buf.size());

    // Build header byte
    uint8_t header = sequence & SEQUENCE_MASK;
    if (is_last)
      header |= LAST_FRAME_FLAG;

    frame_data.push_back(header);

    // Add payload
    for (size_t i = 0; i < payload_size; i++) {
      frame_data.push_back(buf[offset + i]);
    }

    // Send CAN frame
    auto result = this->parent_->send_data(this->can_id_, this->use_extended_id_, false, frame_data);
    if (result != Error::ERROR_OK) {
      ESP_LOGW(TAG, "Failed to send CAN frame (sequence %d)", sequence);
    }

    offset += payload_size;
    sequence = (sequence + 1) & SEQUENCE_MASK;

    // Small delay between frames to avoid overwhelming the bus
    if (!is_last) {
      delayMicroseconds(500);  // 500us between frames
    }
  }

  ESP_LOGV(TAG, "Sent packet: %zu bytes in %d frames", buf.size(), sequence);
}

}  // namespace canbus
}  // namespace esphome
