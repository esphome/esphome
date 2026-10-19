#include "radio_link.h"
#include <cinttypes>
#include "esphome/core/application.h"
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
  uint32_t now = App.get_loop_component_start_time();
  if (busy)
    this->enable_loop();
  if (!this->listen_before_talk_.may_send(busy, now))
    return false;
  if (busy) {
    ESP_LOGD(TAG, "Radio busy for %" PRIu32 "ms, sending anyway", this->listen_before_talk_.busy_for(now));
  }
  return true;
}

void RadioLinkBase::radio_free_() {
  this->listen_before_talk_.end_busy();
  this->disable_loop();
}

void RadioLinkBase::receive_(const std::vector<uint8_t> &packet, float rssi, float snr) {
  PacketMetaData meta_data;
  meta_data.rssi = rssi;
  meta_data.snr = snr;
  this->on_receive_from_interface_(PacketBuffer(packet.data(), packet.size()), meta_data);
}

}  // namespace esphome::packet_interface
