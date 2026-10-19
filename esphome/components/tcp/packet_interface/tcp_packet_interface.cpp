#include "tcp_packet_interface.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::tcp {

using packet_interface::PacketBuffer;
using packet_interface::PacketMetaData;

ESPHOME_LOG_TAG(TAG, "tcp.packet_interface");

// The packet length before each packet
static constexpr size_t LENGTH_SIZE = 2;

TcpPacketInterface::TcpPacketInterface(std::span<uint8_t> rx_buffer)
    : packet_interface::PacketInterface(rx_buffer.size()), rx_buffer_(rx_buffer) {
  this->capabilities_ = packet_interface::CAPABILITY_RELIABILITY | packet_interface::CAPABILITY_SESSION |
                        packet_interface::CAPABILITY_ADDRESSING;
}

void TcpPacketInterface::setup() {
  this->link_.begin(TAG);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.begin(TAG);
#endif
#ifdef USE_BINARY_SENSOR
  if (this->connected_sensor_ != nullptr)
    this->connected_sensor_->publish_initial_state(false);
#endif
}

void TcpPacketInterface::dump_config() {
  bool server = false;
#ifdef USE_SOCKET_TCP_LISTENER
  server = this->server_;
#endif
  ESP_LOGCONFIG(TAG,
                "TCP Packet Interface '%s':\n"
                "  Role: %s\n"
                "  Address: %s:%u\n"
                "  Reconnect Interval: %" PRIu32 "ms\n"
                "  Keep Alive: %" PRIu32 "ms\n"
                "  Timeout: %" PRIu32 "ms\n"
                "  Max Packet Size: %zu",
                this->name_, server ? LOG_STR_LITERAL("server") : LOG_STR_LITERAL("client"),
                server ? LOG_STR_LITERAL("*") : this->link_.host(), this->link_.port(),
                this->link_.reconnect_interval(), this->keep_alive_ms_, this->timeout_ms_, this->max_packet_size_);
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.dump_config();
#endif
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Connected", this->connected_sensor_);
#endif
}

void TcpPacketInterface::on_shutdown() {
  this->link_.close();
#ifdef USE_SOCKET_TCP_LISTENER
  this->listener_.close();
#endif
}

void TcpPacketInterface::loop() {
#ifdef USE_SOCKET_TCP_LISTENER
  if (this->server_) {
    // A new client is accepted only once the drop of the last has been reported
    this->listener_.poll(this->link_, !this->connected_);
  } else {
    this->link_.poll();
  }
#else
  this->link_.poll();
#endif
  if (this->link_.connected() != this->connected_)
    this->on_connection_(this->link_.connected());
  if (!this->connected_)
    return;
  this->read_();
  if (!this->connected_)
    return;
  uint32_t now = App.get_loop_component_start_time();
  if (now - this->last_rx_ms_ > this->timeout_ms_) {
    ESP_LOGW(TAG, "%s: Nothing received for %" PRIu32 "ms, closing the connection", this->name_, this->timeout_ms_);
    this->link_.close();
    this->link_.note_attempt();
    this->on_connection_(false);
    return;
  }
  if (now - this->last_tx_ms_ >= this->keep_alive_ms_)
    this->queue_frame_(nullptr, 0);
  this->link_.flush_tx();
}

void TcpPacketInterface::on_connection_(bool connected) {
  ESP_LOGD(TAG, "%s: Connection %s", this->name_, connected ? LOG_STR_LITERAL("up") : LOG_STR_LITERAL("down"));
  this->connected_ = connected;
  this->rx_have_ = 0;
  this->last_rx_ms_ = this->last_tx_ms_ = App.get_loop_component_start_time();
#ifdef USE_BINARY_SENSOR
  if (this->connected_sensor_ != nullptr)
    this->connected_sensor_->publish_state(connected);
#endif
  this->session_callback_.call(connected);
}

void TcpPacketInterface::read_() {
  for (;;) {
    ssize_t count;
    if (this->rx_have_ < LENGTH_SIZE) {
      count = this->link_.read(this->rx_header_ + this->rx_have_, LENGTH_SIZE - this->rx_have_);
    } else {
      count = this->link_.read(this->rx_buffer_.data() + this->rx_have_ - LENGTH_SIZE,
                               this->rx_length_ + LENGTH_SIZE - this->rx_have_);
    }
    // Nothing more for now; a dropped link is seen on the next loop
    if (count <= 0)
      return;
    this->last_rx_ms_ = App.get_loop_component_start_time();
    this->rx_have_ += count;
    if (this->rx_have_ < LENGTH_SIZE)
      continue;
    if (this->rx_have_ == LENGTH_SIZE) {
      this->rx_length_ = encode_uint16(this->rx_header_[1], this->rx_header_[0]);
      if (this->rx_length_ > this->max_packet_size_) {
        // The stream cannot be resynchronised, so start a new connection
        ESP_LOGW(TAG, "%s: Packet of %u bytes is larger than %zu, closing the connection", this->name_,
                 this->rx_length_, this->max_packet_size_);
        this->link_.close();
        this->link_.note_attempt();
        this->on_connection_(false);
        return;
      }
    }
    if (this->rx_have_ == this->rx_length_ + LENGTH_SIZE) {
      this->rx_have_ = 0;
      // A zero length packet is a keep-alive
      if (this->rx_length_ != 0)
        this->on_receive_from_interface_(PacketBuffer(this->rx_buffer_.data(), this->rx_length_), {});
    }
  }
}

bool TcpPacketInterface::queue_frame_(const PacketBuffer *data, size_t size) {
  if (this->link_.tx_free() < size + LENGTH_SIZE)
    return false;
  uint8_t *tail = this->link_.tx_tail();
  tail[0] = static_cast<uint8_t>(size);
  tail[1] = static_cast<uint8_t>(size >> 8);
  if (data != nullptr)
    data->copy_to(tail + LENGTH_SIZE, size);
  this->link_.tx_commit(size + LENGTH_SIZE);
  this->last_tx_ms_ = App.get_loop_component_start_time();
  return true;
}

bool TcpPacketInterface::send_to_interface(const PacketBuffer &data, const PacketMetaData &meta_data) {
  size_t size = data.size();
  if (size > this->max_packet_size_) {
    ESP_LOGW(TAG, "%s: Packet of %zu bytes is larger than %zu", this->name_, size, this->max_packet_size_);
    return false;
  }
  if (!this->connected_ || !this->queue_frame_(&data, size))
    return false;
  this->link_.flush_tx();
  return true;
}

}  // namespace esphome::tcp
