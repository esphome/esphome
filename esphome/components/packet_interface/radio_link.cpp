#include "radio_link.h"
#include <cinttypes>
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.radio");

bool RadioLinkBase::check_size_(size_t size) const {
  if (size <= this->max_packet_size_)
    return true;
  ESP_LOGW(TAG, "Packet of %zu bytes is larger than %zu", size, this->max_packet_size_);
  return false;
}

bool RadioLinkBase::may_send_(bool busy) {
  if (busy)
    this->enable_loop();
  return this->listen_before_talk_.may_send(busy, this->current_time_ms());
}

bool RadioLinkBase::sent_(bool sent, bool busy) {
  if (busy) {
    uint32_t busy_ms = this->listen_before_talk_.busy_for(this->current_time_ms());
    // The radio listens again, and a reception that starts now must be waited for afresh
    this->listen_before_talk_.end_busy();
    // The limit is longer than any packet takes, so the radio's busy indication is stuck or the channel is jammed
    ESP_LOGW(TAG, "Radio busy for %" PRIu32 "ms, sent anyway", busy_ms);
  }
  return sent;
}

bool RadioLinkBase::skip_empty_() const {
  ESP_LOGV(TAG, "Empty packet not sent");
  return true;
}

void RadioLinkBase::radio_not_ready_(bool failed) {
  if (!failed || this->failed_warned_)
    return;
  this->failed_warned_ = true;
  ESP_LOGW(TAG, "%s: The radio failed to set up, so nothing can be sent", this->name_);
}

void RadioLinkBase::radio_free_() {
  this->listen_before_talk_.end_busy();
  this->disable_loop();
}

void RadioLinkBase::receive_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  // The radio takes packets up to its own limit, so a larger one is from a device configured differently
  if (packet.size() > this->max_packet_size_) {
    ESP_LOGV(TAG, "%s: Packet of %zu bytes is larger than %zu, dropped", this->name_, packet.size(),
             this->max_packet_size_);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  PacketMetaData meta_data;
  meta_data.rssi = rssi;
  meta_data.snr = snr;
  this->on_receive_from_interface_(PacketBuffer(packet.data(), packet.size()), meta_data);
}

}  // namespace esphome::packet_interface
