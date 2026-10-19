#include "espnow_packet_interface.h"

#if defined(USE_ESP32) || defined(USE_ESP8266)

#include <cstring>

#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::espnow {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "espnow.packet_interface");

ESPNowPacketInterface::ESPNowPacketInterface(ESPNowComponent *parent, std::span<uint8_t> tx_buffer,
                                             peer_address_t peer_address)
    : packet_interface::PacketInterface(tx_buffer.size()),
      parent_(parent),
      peer_address_(peer_address),
      tx_buffer_(tx_buffer) {
  if (!this->is_broadcast_())
    this->capabilities_ = packet_interface::CAPABILITY_ADDRESSING;
}

void ESPNowPacketInterface::setup() {
  // Each mode hears only its own kind of packet, so a peer link and a broadcast link can share the component
  if (!this->is_broadcast_() || !ESPNOW_REPORTS_DESTINATION)
    this->parent_->register_receive_handler(this);
  if (this->is_broadcast_()) {
    this->parent_->register_broadcast_handler(this);
    this->parent_->register_unknown_peer_handler(this);
  }
}

void ESPNowPacketInterface::dump_config() {
  char peer_buf[MAC_ADDRESS_PRETTY_BUFFER_SIZE];
  format_mac_addr_upper(this->peer_address_.data(), peer_buf);
  ESP_LOGCONFIG(TAG,
                "ESP-NOW Packet Interface '%s':\n"
                "  Peer: %s\n"
                "  Max Packet Size: %zu",
                this->name_, peer_buf, this->max_packet_size_);
}

bool ESPNowPacketInterface::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Layers above are set up first, and may send while setup waits for another component
  if (!this->is_ready())
    return false;
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  const uint8_t *payload;
  if (data.is_contiguous()) {
    payload = data.as_span().data();
  } else {
    data.copy_to(this->tx_buffer_.data(), size);
    payload = this->tx_buffer_.data();
  }
  esp_err_t err = this->parent_->send(this->peer_address_.data(), payload, size);
  if (err != ESP_OK) {
    this->send_failed_(err);
    return false;
  }
  return true;
}

// A full send queue is logged by the espnow component, and ESP-NOW is not running before setup or while disabled.
// Other errors, such as a peer that could not be added, repeat on every send, so warn now and then
void ESPNowPacketInterface::send_failed_(esp_err_t err) {
  static constexpr uint32_t WARNING_INTERVAL_MS = 60000;
  uint32_t now = App.get_loop_component_start_time();
  if (err == ESP_ERR_ESPNOW_NO_MEM || err == ESP_ERR_ESPNOW_NOT_INIT ||
      (this->send_warned_ && now - this->last_send_warning_ms_ < WARNING_INTERVAL_MS)) {
    ESP_LOGV(TAG, "%s: Send failed: %s", this->name_, LOG_STR_ARG(espnow_error_to_str(err)));
    return;
  }
  this->send_warned_ = true;
  this->last_send_warning_ms_ = now;
  ESP_LOGW(TAG, "%s: Send failed: %s", this->name_, LOG_STR_ARG(espnow_error_to_str(err)));
}

bool ESPNowPacketInterface::on_receive(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) {
  this->receive_(info, data, size);
  return false;
}

bool ESPNowPacketInterface::on_broadcast(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) {
  this->receive_(info, data, size);
  return false;
}

bool ESPNowPacketInterface::on_unknown_peer(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) {
  // Only known peers reach the other handlers; with auto_add_peer this sender is added and arrives there
  if (this->parent_->get_auto_add_peer())
    return false;
  if (!ESPNOW_REPORTS_DESTINATION || memcmp(info.des_addr, ESPNOW_BROADCAST_ADDR, ESP_NOW_ETH_ALEN) == 0)
    this->receive_(info, data, size);
  return false;
}

void ESPNowPacketInterface::receive_(const ESPNowRecvInfo &info, const uint8_t *data, uint16_t size) {
  if (size == 0)
    return;
  if (size > this->max_packet_size_) {
    ESP_LOGV(TAG, "%s: Packet of %u bytes is larger than %zu, dropped", this->name_, size, this->max_packet_size_);
    return;
  }
  if (!this->is_broadcast_() && memcmp(info.src_addr, this->peer_address_.data(), ESP_NOW_ETH_ALEN) != 0)
    return;
  PacketMetaData meta_data;
  memcpy(meta_data.mac_address, info.src_addr, ESP_NOW_ETH_ALEN);
#ifdef USE_ESP32
  meta_data.rssi = info.rx_ctrl->rssi;
#endif
  this->on_receive_from_interface_(PacketBuffer(data, size), meta_data);
}

}  // namespace esphome::espnow

#endif  // USE_ESP32 || USE_ESP8266
