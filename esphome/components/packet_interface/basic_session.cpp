#include "basic_session.h"
#ifdef USE_PACKET_INTERFACE_BASIC_SESSION
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::packet_interface {

ESPHOME_LOG_TAG(TAG, "packet_interface.session");

static constexpr uint8_t PACKET_TYPE_DATA = 0x00;
static constexpr uint8_t PACKET_TYPE_KEEP_ALIVE = 0x01;
// Type byte and the 32-bit boot id, little endian
static constexpr size_t KEEP_ALIVE_SIZE = 5;

BasicSession::BasicSession(PacketInterface *lower) : lower_(lower) {
  this->stack_on_(lower, CAPABILITY_SESSION);
  this->max_packet_size_ = lower->get_max_packet_size() - HEADER_SIZE;
  // Data lost by a reliability layer below must be sent again, as after a restart of the peer
  if (lower->has_capability(CAPABILITY_RELIABILITY)) {
    lower->add_on_session_state_callback([this](bool connected) {
      if (!connected && this->connected_)
        this->set_connected_(false);
    });
  }
}

void BasicSession::setup() {
  this->boot_id_ = random_uint32();
#ifdef USE_BINARY_SENSOR
  if (this->connected_sensor_ != nullptr)
    this->connected_sensor_->publish_initial_state(false);
#endif
  this->lower_->add_packet_interface_listener(
      [this](const PacketBuffer &data, const PacketMetaData &meta_data) { this->receive_(data, meta_data); });
  // A reliability layer below queues this until its link is up, so the peer hears of a restart straight away
  this->send_keep_alive_();
  this->set_interval(this->keep_alive_ms_, [this] { this->on_interval_(); });
}

void BasicSession::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Basic Session on '%s':\n"
                "  Max Packet Size: %zu\n"
                "  Keep Alive: %" PRIu32 "ms\n"
                "  Timeout: %" PRIu32 "ms",
                this->name_, this->max_packet_size_, this->keep_alive_ms_, this->timeout_ms_);
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
#endif
}

bool BasicSession::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  uint8_t header = PACKET_TYPE_DATA;
  PacketBuffer packet(&header, HEADER_SIZE, &data, nullptr, 0);
  return this->lower_->send_to_interface(packet, meta_data);
}

void BasicSession::send_keep_alive_() {
  uint8_t packet[KEEP_ALIVE_SIZE] = {
      PACKET_TYPE_KEEP_ALIVE, static_cast<uint8_t>(this->boot_id_), static_cast<uint8_t>(this->boot_id_ >> 8),
      static_cast<uint8_t>(this->boot_id_ >> 16), static_cast<uint8_t>(this->boot_id_ >> 24)};
  if (!this->lower_->send_to_interface(PacketBuffer(packet, sizeof(packet)), {}))
    ESP_LOGV(TAG, "%s: Lower layer did not accept keep-alive", this->name_);
}

void BasicSession::on_interval_() {
  this->send_keep_alive_();
  if (this->connected_ && App.get_loop_component_start_time() - this->last_receive_ms_ > this->timeout_ms_) {
    ESP_LOGW(TAG, "%s: Peer not heard for %" PRIu32 "ms", this->name_, this->timeout_ms_);
    this->set_connected_(false);
  }
}

void BasicSession::set_connected_(bool connected) {
  this->connected_ = connected;
  ESP_LOGD(TAG, "%s: Session %s", this->name_, connected ? LOG_STR_LITERAL("connected") : LOG_STR_LITERAL("lost"));
#ifdef USE_BINARY_SENSOR
  if (this->connected_sensor_ != nullptr)
    this->connected_sensor_->publish_state(connected);
#endif
  this->session_callback_.call(connected);
}

void BasicSession::receive_(const PacketBuffer &data, const PacketMetaData &meta_data) {
  // Lower layers deliver packets in one piece; the payload is passed up without copying
  if (!data.is_contiguous()) {
    ESP_LOGW(TAG, "%s: Packet split across buffers is not supported", this->name_);
    return;
  }
  auto packet = data.as_span();
  if (packet.empty()) {
    this->note_received_(false);
    return;
  }
  uint8_t type = packet[0];
  if (type == PACKET_TYPE_KEEP_ALIVE) {
    if (packet.size() != KEEP_ALIVE_SIZE) {
      ESP_LOGV(TAG, "%s: Keep-alive of %zu bytes dropped", this->name_, packet.size());
      this->note_received_(false);
      return;
    }
    uint32_t boot_id = encode_uint32(packet[4], packet[3], packet[2], packet[1]);
    if (!this->peer_known_ || boot_id != this->peer_boot_id_) {
      // Data may arrive before the first keep-alive, so only a changed id means a restart
      if (this->connected_ && this->peer_known_) {
        ESP_LOGD(TAG, "%s: Peer has restarted", this->name_);
        this->set_connected_(false);
      }
      this->peer_boot_id_ = boot_id;
      this->peer_known_ = true;
      // Answer a new peer at once; it answers only if our id is new to it, so this cannot repeat
      this->send_keep_alive_();
    }
  } else if (type != PACKET_TYPE_DATA) {
    ESP_LOGV(TAG, "%s: Unknown packet type 0x%02X dropped", this->name_, type);
    this->note_received_(false);
    return;
  }
  this->note_received_(true);
  this->last_receive_ms_ = App.get_loop_component_start_time();
  if (!this->connected_)
    this->set_connected_(true);
  if (type == PACKET_TYPE_DATA)
    this->on_receive_from_interface_(PacketBuffer(packet.data() + HEADER_SIZE, packet.size() - HEADER_SIZE), meta_data);
}

}  // namespace esphome::packet_interface
#endif
